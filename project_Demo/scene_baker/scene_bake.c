#include "scene_bake.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Light.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void failure(char* error, size_t capacity, const char* message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}
static uint64_t hashNumber(uint64_t hash, int64_t value)
{
    uint64_t bits=(uint64_t)value;
    for (int i=0;i<8;i++) {hash^=(bits>>(8*i))&255;hash*=UINT64_C(1099511628211);}
    return hash;
}
static uint64_t hashFloat(uint64_t hash, float value)
{
    return hashNumber(hash, isfinite(value) && fabsf(value)<1e12f ? llround((double)value*1000) : INT64_MIN);
}
uint64_t SceneBake_Fingerprint(GRE_Object4d mesh, GRE_List lights)
{
    uint64_t hash=UINT64_C(14695981039346656037);
    if (!mesh || !lights) return 0;
    hash=hashNumber(hash,mesh->pointNum);hash=hashNumber(hash,mesh->polygonNum);
    hash=hashNumber(hash,mesh->renderMode);
    for(int i=0;i<mesh->pointNum;i++) {
        gre_fvector4d p=mesh->pointList[i].pos;
        hash=hashFloat(hash,p.x);hash=hashFloat(hash,p.y);hash=hashFloat(hash,p.z);
        if(mesh->renderMode!=GRE_RenderMode_Face && mesh->pointList_wN) {
            p=mesh->pointList_wN[i].normal;
            hash=hashFloat(hash,p.x);hash=hashFloat(hash,p.y);hash=hashFloat(hash,p.z);
        }
    }
    for(int i=0;i<mesh->polygonNum;i++) {
        GRE_Polygon4d p=&mesh->polygonList[i];hash=hashNumber(hash,p->num);
        for(int j=0;j<p->num;j++) hash=hashNumber(hash,p->index[j]);
        hash=hashFloat(hash,p->pN.x);hash=hashFloat(hash,p->pN.y);hash=hashFloat(hash,p->pN.z);
    }
    for(GRE_ListNode node=lights->listhead;node;node=node->next) {
        GRE_Light4d l=node->data;
        hash=hashNumber(hash,l->type);hash=hashFloat(hash,l->proper.strength);
        hash=hashNumber(hash,l->proper.lightcolor.R);hash=hashNumber(hash,l->proper.lightcolor.G);
        hash=hashNumber(hash,l->proper.lightcolor.B);
        if(l->type==GRE_GlobalLight) continue;
        hash=hashFloat(hash,l->pos.x);hash=hashFloat(hash,l->pos.y);hash=hashFloat(hash,l->pos.z);
        hash=hashFloat(hash,l->proper.kc0);hash=hashFloat(hash,l->proper.kc1);hash=hashFloat(hash,l->proper.kc2);
        if(l->type==GRE_SpotLight) {
            hash=hashFloat(hash,l->proper.spot.direct.x);hash=hashFloat(hash,l->proper.spot.direct.y);
            hash=hashFloat(hash,l->proper.spot.direct.z);
            hash=hashFloat(hash,l->proper.spot.cs_outer_angle);hash=hashFloat(hash,l->proper.spot.cs_div_);
        }
    }
    return hash;
}

static GRE_Lightmap allocateMap(uint16 size, uint32 count)
{
    GRE_Lightmap map=GRE_malloc0(sizeof(gre_lightmap));
    if(!map) return NULL;
    memset(map,0,sizeof(*map));map->width=map->height=size;map->triangleCount=count;map->enabled=1;
    map->uv1=GRE_malloc1((size_t)count*6*sizeof(float32));
    map->pixels=GRE_ImageBuff_Malloc((size_t)size*size*sizeof(GRErgb24));
    if(!map->uv1 || !map->pixels) {YMGRE_Free_Lightmap(map);return NULL;}
    memset(map->pixels,0,(size_t)size*size*sizeof(GRErgb24));
    return map;
}

static GRE_Lightmap createView(GRE_Object4d mesh, GRE_List lights, uint16 size,
    uint8 includeLighting, const gre_fvector4d* view, float32 mirrorKs, uint8 power,
    GRErgb24 specularColor, GRE_Material base, char* error, size_t capacity)
{
    if(!mesh || !lights || mesh->nextObject || mesh->polygonNum<=0 || !mesh->pointList ||
        !mesh->polygonList || size<32 || size>1024) {
        failure(error,capacity,"烘焙需要单个三角网格，图集尺寸须为 32–1024");return NULL;
    }
    int grid=(int)ceil(sqrt((double)mesh->polygonNum));
    int tile=size/grid;
    if(tile<8) {failure(error,capacity,"三角形过多：请降低细分或增加图集分辨率");return NULL;}
    for(int i=0;i<mesh->polygonNum;i++) {
        GRE_Polygon4d p=&mesh->polygonList[i];
        if(p->num!=3 || !p->index) {failure(error,capacity,"网格包含非三角形面");return NULL;}
        for(int j=0;j<3;j++) if(p->index[j]>=mesh->pointNum) {
            failure(error,capacity,"网格顶点索引无效");return NULL;
        }
        gre_fvector4d a=mesh->pointList[p->index[0]].pos,b=mesh->pointList[p->index[1]].pos,c=mesh->pointList[p->index[2]].pos;
        gre_fvector4d ab={b.x-a.x,b.y-a.y,b.z-a.z,0},ac={c.x-a.x,c.y-a.y,c.z-a.z,0},n;
        YMGRE_Fvector4d_CrossToResult(&ab,&ac,&n);
        float area=YMGRE_Fvector4d_Len1(&n);
        int uvPole=0;
        if(isfinite(area)&&area<1e-8f&&p->index[0]!=p->index[1]&&p->index[1]!=p->index[2]&&p->index[0]!=p->index[2]) {
            /* Longitude grids duplicate the pole at different UVs. These zero-area
               faces carry the rectangular UV boundary and render no surface. */
            for(int j=0;j<3;j++) {
                gre_vertex4d x=mesh->pointList[p->index[j]],y=mesh->pointList[p->index[(j+1)%3]],z=mesh->pointList[p->index[(j+2)%3]];
                if(x.pos.x==y.pos.x&&x.pos.y==y.pos.y&&x.pos.z==y.pos.z&&(x.u!=y.u||x.v!=y.v)&&
                   (x.pos.x!=z.pos.x||x.pos.y!=z.pos.y||x.pos.z!=z.pos.z))uvPole=1;
            }
        }
        if(!isfinite(area)||(area<1e-8f&&!uvPole)){failure(error,capacity,"网格包含退化或非法三角形");return NULL;}
    }
    if(!mesh->pointList_wN && !YMGRE_Object_GenerateVertexAttributes(mesh)) {
        failure(error,capacity,"无法分配顶点属性");return NULL;
    }
    GRE_Lightmap map=allocateMap(size,mesh->polygonNum);
    if(!map) {failure(error,capacity,"无法分配光照贴图");return NULL;}
    map->materialOnly=!includeLighting;map->enabled=includeLighting;
    map->colorsBaked=includeLighting && base;
    if(base){map->capturedAmbient=base->ambient;map->capturedDiffuse=base->diffuse;}
    if(includeLighting && view) {
        if(!isfinite(view->x)||!isfinite(view->y)||!isfinite(view->z)||!isfinite(mirrorKs)||mirrorKs<0) {
            YMGRE_Free_Lightmap(map);failure(error,capacity,"高光烘焙参数无效");return NULL;
        }
        map->specularPixels=GRE_ImageBuff_Malloc((size_t)size*size*sizeof(GRErgb24));
        if(!map->specularPixels){YMGRE_Free_Lightmap(map);failure(error,capacity,"无法分配高光图集");return NULL;}
        memset(map->specularPixels,0,(size_t)size*size*sizeof(GRErgb24));
        map->referenceView=*view;map->specularKs=mirrorKs;map->specularPower=power?power:30;
        map->specularColor=specularColor;
    }
    for(int i=0;i<mesh->polygonNum;i++) {
        GRE_Polygon4d p=&mesh->polygonList[i];
        float* uv=&map->uv1[i*6];
        int left=(i%grid)*tile,top=(i/grid)*tile;
        float low=2.5f,high=tile-2.5f;
        uv[0]=(left+low)/size;uv[1]=(top+low)/size;
        uv[2]=(left+high)/size;uv[3]=uv[1];uv[4]=uv[0];uv[5]=(top+high)/size;
        gre_polygon4d white=*p;white.planeColor=(GRErgb24){255,255,255};
        for(int y=0;y<tile;y++) for(int x=0;x<tile;x++) {
            /* Fill each chart's padding from its nearest point in UV space.
               Separate tiles never exchange padding, including along seams. */
            float b=GREMax(0,(x+0.5f-low)/(high-low));
            float c=GREMax(0,(y+0.5f-low)/(high-low));
            if(b+c>1) {float shift=(b+c-1)*0.5f;b-=shift;c-=shift;
                if(b<0){b=0;c=1;}if(c<0){c=0;b=1;}}
            float weights[3]={1-b-c,b,c};
            gre_fvector4d pos={0,0,0,1},normal={0,0,0,0};
            for(int j=0;j<3;j++) {
                GRE_Vertex4d_wN v=&mesh->pointList_wN[p->index[j]];
                gre_fvector4d world=mesh->pointList[p->index[j]].pos;
                pos.x+=weights[j]*world.x;pos.y+=weights[j]*world.y;pos.z+=weights[j]*world.z;
                normal.x+=weights[j]*v->normal.x;normal.y+=weights[j]*v->normal.y;normal.z+=weights[j]*v->normal.z;
            }
            if(mesh->renderMode==GRE_RenderMode_Face) normal=p->pN;
            YMGRE_Fvector4d_Normalize(&normal);
            GRErgb24 illumination={0,0,0},specular={0,0,0};
            if(!includeLighting)illumination=(GRErgb24){255,255,255};
            for(GRE_ListNode node=includeLighting?lights->listhead:NULL;node;node=node->next) {
                gre_light4d light=*(GRE_Light4d)node->data;
                light.proper.pos_=light.pos;light.proper.shadowK=0;
                gre_fvector4d shadingPosition=pos;
                if(map->specularPixels) {
                    /* Rotate-free camera coordinates: subtract the same view origin
                       from the surface and lights; dot products remain invariant. */
                    shadingPosition.x-=view->x;shadingPosition.y-=view->y;shadingPosition.z-=view->z;
                    light.proper.pos_.x-=view->x;light.proper.pos_.y-=view->y;light.proper.pos_.z-=view->z;
                }
                /* Match the live renderer: material color participates before each
                   RGB8 light accumulation clamps, so bright regions do not darken. */
                if(base)white.planeColor=light.type==GRE_GlobalLight?base->ambient:base->diffuse;
                YMGRE_PolygonLighting_ComponentsAdvanced(&white,&shadingPosition,&normal,&light,
                    &illumination,&specular,map->specularPixels?mirrorKs:0,map->specularPower,specularColor);
            }
            map->pixels[(top+y)*size+left+x]=illumination;
            if(map->specularPixels)map->specularPixels[(top+y)*size+left+x]=specular;
        }
    }
    return map;
}

GRE_Lightmap SceneBake_CreateView(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,const gre_fvector4d* view,float32 mirrorKs,uint8 power,
    GRErgb24 specularColor,char* error,size_t capacity)
{return createView(mesh,lights,size,includeLighting,view,mirrorKs,power,specularColor,NULL,error,capacity);}

GRE_Lightmap SceneBake_CreateSurface(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,const gre_fvector4d* view,GRE_Material base,char* error,size_t capacity)
{
    if(!mesh || !base){failure(error,capacity,"缺少烘焙模型或材质");return NULL;}
    uint8 power=base->advanced && base->advanced->specularPower?base->advanced->specularPower:30;
    return createView(mesh,lights,size,includeLighting,view,mesh->mirrorKs,power,base->specular,base,error,capacity);
}

GRE_Lightmap SceneBake_CreateMode(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,char* error,size_t capacity)
{return SceneBake_CreateView(mesh,lights,size,includeLighting,NULL,0,30,(GRErgb24){0,0,0},error,capacity);}

GRE_Lightmap SceneBake_Create(GRE_Object4d mesh,GRE_List lights,uint16 size,char* error,size_t capacity)
{return SceneBake_CreateMode(mesh,lights,size,1,error,capacity);}
uint64_t SceneBake_ModeFingerprint(GRE_Object4d mesh,GRE_List lights,uint8 materialOnly)
{gre_list empty={0};return SceneBake_Fingerprint(mesh,materialOnly?&empty:lights);}

static int makeDirectories(const char* path)
{
    char temp[4096];if(!path || !*path || strlen(path)>=sizeof(temp)) return 0;
    strcpy(temp,path);
    for(char* p=temp+1;;p++) if(*p=='/' || *p=='\0') {
        char saved=*p;*p='\0';
        if(mkdir(temp,0775)!=0 && errno!=EEXIST) return 0;
        struct stat st;if(stat(temp,&st)!=0 || !S_ISDIR(st.st_mode)) return 0;
        *p=saved;if(!saved) break;
    }
    return 1;
}
static void put16(FILE* f,unsigned n) {fputc(n&255,f);fputc((n>>8)&255,f);}
static void put32(FILE* f,unsigned n) {put16(f,n&65535);put16(f,n>>16);}
static int writeBmp(const char* path,GRE_Lightmap map)
{
    FILE* f=fopen(path,"wb");if(!f) return 0;
    unsigned stride=(map->width*3u+3u)&~3u;
    fputs("BM",f);put32(f,54+stride*map->height);put32(f,0);put32(f,54);
    put32(f,40);put32(f,map->width);put32(f,map->height);put16(f,1);put16(f,24);
    put32(f,0);put32(f,stride*map->height);put32(f,2835);put32(f,2835);put32(f,0);put32(f,0);
    for(int y=map->height-1;y>=0;y--) {
        for(int x=0;x<map->width;x++){GRErgb24 c=map->pixels[y*map->width+x];fputc(c.B,f);fputc(c.G,f);fputc(c.R,f);}
        for(unsigned x=map->width*3u;x<stride;x++) fputc(0,f);
    }
    int ok=!ferror(f);if(fclose(f)!=0)ok=0;return ok;
}
static GRE_Lightmap loadLegacy(const char* path,GRE_Object4d mesh,GRE_List lights,char* error,size_t capacity)
{
    FILE* f=fopen(path,"rb");GRE_Lightmap map=NULL;unsigned w,h,count;uint64_t fingerprint;
    char line[64];
    if(!f) {failure(error,capacity,"找不到烘焙资源");return NULL;}
    if(!fgets(line,sizeof(line),f) || strcmp(line,"YMGRE_LIGHTMAP 1\n") ||
        fscanf(f,"%u %u %u %" SCNu64,&w,&h,&count,&fingerprint)!=4 ||
        w<32 || w>1024 || h!=w || !mesh || count!=(unsigned)mesh->polygonNum ||
        count==0 || count>(w/8)*(w/8)) goto invalid;
    if(fingerprint!=SceneBake_Fingerprint(mesh,lights)) {
        failure(error,capacity,"物体或灯光已变化，请重新烘焙");fclose(f);return NULL;
    }
    map=allocateMap(w,count);if(!map)goto invalid;
    for(uint32 i=0;i<count*6;i++) if(fscanf(f,"%f",&map->uv1[i])!=1 ||
        !isfinite(map->uv1[i]) || map->uv1[i]<=0 || map->uv1[i]>=1) goto invalid;
    if(fgetc(f)!='\n' || !fgets(line,sizeof(line),f) || strcmp(line,"RGB\n"))goto invalid;
    for(uint32 i=0;i<w*h;i++) {
        int r=fgetc(f),g=fgetc(f),b=fgetc(f);if(r==EOF||g==EOF||b==EOF)goto invalid;
        map->pixels[i]=(GRErgb24){r,g,b};
    }
    if(fgetc(f)!=EOF || ferror(f)) goto invalid;
    fclose(f);return map;
invalid:
    fclose(f);YMGRE_Free_Lightmap(map);failure(error,capacity,"烘焙资源格式无效或网格不匹配");return NULL;
}

static GRErgb24* readBmp(const char* path,uint16* width,uint16* height)
{
    FILE* f=fopen(path,"rb");if(!f)return NULL;
    unsigned char h[54];GRErgb24* pixels=NULL;
    if(fread(h,1,54,f)!=54)goto done;
#define U16(p) ((unsigned)(p)[0] | (unsigned)(p)[1]<<8)
#define U32(p) ((uint32)(p)[0] | (uint32)(p)[1]<<8 | (uint32)(p)[2]<<16 | (uint32)(p)[3]<<24)
    uint32 offset=U32(h+10),w=U32(h+18),heightValue=U32(h+22);
    if(h[0]!='B'||h[1]!='M'||U32(h+14)!=40||U16(h+26)!=1||U16(h+28)!=24||
        U32(h+30)!=0||offset<54||!w||w>4096||!heightValue||heightValue>4096)goto done;
    uint32 stride=(w*3+3)&~3u;
    if(fseek(f,0,SEEK_END)||ftell(f)<(long)offset+(long)stride*heightValue||fseek(f,offset,SEEK_SET))goto done;
    pixels=GRE_ImageBuff_Malloc((size_t)w*heightValue*sizeof(GRErgb24));if(!pixels)goto done;
    for(int y=(int)heightValue-1;y>=0;y--) {
        for(uint32 x=0;x<w;x++) {
            int b=fgetc(f),g=fgetc(f),r=fgetc(f);
            if(b==EOF||g==EOF||r==EOF){GRE_ImageBuff_Free(pixels);pixels=NULL;goto done;}
            pixels[y*w+x]=(GRErgb24){r,g,b};
        }
        if(fseek(f,stride-w*3,SEEK_CUR)){GRE_ImageBuff_Free(pixels);pixels=NULL;goto done;}
    }
    *width=w;*height=heightValue;
done:
    fclose(f);return pixels;
#undef U16
#undef U32
}

int SceneBake_SaveMaterial(GRE_Lightmap map,uint64_t fingerprint,GRE_Material base,const char* root,
    char* resourcePath,size_t pathCapacity,char* error,size_t capacity)
{
    char directory[4000],resource[4096],bmp[4096],albedoPath[4096],uvfile[4096],manifest[4096],specularPath[4096];
    if(!map || !map->pixels || !map->uv1 || !resourcePath || !makeDirectories(root) ||
        snprintf(directory,sizeof(directory),"%s/bake-XXXXXX",root)>=(int)sizeof(directory)-32 || !mkdtemp(directory)) {
        failure(error,capacity,"无法创建烘焙输出目录");return 0;
    }
    snprintf(resource,sizeof(resource),"%s/baked.material",directory);
    snprintf(bmp,sizeof(bmp),"%s/lightmap.bmp",directory);
    snprintf(albedoPath,sizeof(albedoPath),"%s/albedo.bmp",directory);
    snprintf(specularPath,sizeof(specularPath),"%s/specular.bmp",directory);
    snprintf(uvfile,sizeof(uvfile),"%s/uv1.csv",directory);
    snprintf(manifest,sizeof(manifest),"%s/bake_manifest.txt",directory);
    GRErgb24 white={255,255,255};
    gre_lightmap albedo={.width=1,.height=1,.pixels=&white};
    if(base && base->pixel && base->width && base->height) {
        albedo.width=base->width;albedo.height=base->height;albedo.pixels=base->pixel;
    }
    GRErgb24 diffuse=base?base->diffuse:white,ambient=base?base->ambient:white;
    int ok=albedo.width<=4096 && albedo.height<=4096 && writeBmp(bmp,map) && writeBmp(albedoPath,&albedo);
    if(ok && map->specularPixels) {gre_lightmap spec=*map;spec.pixels=map->specularPixels;ok=writeBmp(specularPath,&spec);}
    FILE* f=NULL;
    if(ok) {
        f=fopen(uvfile,"w");ok=f!=NULL;
        if(f) {
            fputs("triangle,corner,u1,v1\n",f);
            for(uint32 i=0;i<map->triangleCount*3;i++)fprintf(f,"%u,%u,%.9g,%.9g\n",i/3,i%3,map->uv1[i*2],map->uv1[i*2+1]);
            ok=!ferror(f);if(fclose(f)!=0)ok=0;
        }
    }
    if(ok) {
        f=fopen(resource,"w");ok=f!=NULL;
        if(f) {
            fprintf(f,"// YMGRE baked material v1; UV1 lightmap replaces live diffuse lighting.\n"
                "material baked_%s\n{\n    technique\n    {\n        pass\n        {\n"
                "            ambient %.9g %.9g %.9g\n            diffuse %.9g %.9g %.9g\n"
                "            specular 0 0 0\n            texture_unit\n            {\n                texture albedo.bmp\n            }\n"
                "            lightmap lightmap.bmp\n            lightmap_uv1 uv1.csv\n"
                "            lightmap_size %u %u\n            lightmap_triangles %u\n"
                "            lightmap_uv_channel 1\n            lightmap_mode %s\n"
                "            lightmap_fingerprint %" PRIu64 "\n",
                directory+strlen(directory)-6,ambient.R/255.0,ambient.G/255.0,ambient.B/255.0,
                diffuse.R/255.0,diffuse.G/255.0,diffuse.B/255.0,map->width,map->height,map->triangleCount,map->materialOnly?"material_only":(map->colorsBaked?"baked_color":"replace_diffuse"),fingerprint);
            if(map->specularPixels)fprintf(f,
                "            lightmap_specular specular.bmp\n            lightmap_view %.9g %.9g %.9g\n"
                "            lightmap_specular_strength %.9g\n            lightmap_specular_power %u\n"
                "            lightmap_specular_color %u %u %u\n",
                map->referenceView.x,map->referenceView.y,map->referenceView.z,map->specularKs,map->specularPower,
                map->specularColor.R,map->specularColor.G,map->specularColor.B);
            fputs("        }\n    }\n}\n",f);
            ok=!ferror(f);if(fclose(f)!=0)ok=0;
        }
    }
    if(ok) {
        f=fopen(manifest,"w");ok=f!=NULL;
        if(f) {
            fprintf(f,"format=ymgre_bake_v3\nbackend=uv_raster_direct\nmaterial=baked.material\nlightmap=lightmap.bmp\nalbedo=albedo.bmp\nuv_layout=uv1.csv\nwidth=%u\nheight=%u\ntriangles=%u\nuv_channel=1\npadding=2\ncolor_space=engine_rgb8\ncontents=ambient_plus_direct_diffuse_white_albedo\nshadows=none\nspecular=%s\nfingerprint=%" PRIu64 "\n",map->width,map->height,map->triangleCount,map->specularPixels?"baked_from_reference_view":"none",fingerprint);
            ok=!ferror(f);if(fclose(f)!=0)ok=0;
        }
    }
    char absolute[4096];
    if(ok)ok=realpath(resource,absolute)!=NULL && strlen(absolute)<pathCapacity;
    if(ok){strcpy(resourcePath,absolute);return 1;}
    unlink(resource);unlink(bmp);unlink(albedoPath);unlink(specularPath);unlink(uvfile);unlink(manifest);rmdir(directory);
    failure(error,capacity,"烘焙材质或图片写入失败，原有预览已保留");return 0;
}
int SceneBake_Save(GRE_Lightmap map,uint64_t fingerprint,const char* root,
    char* resourcePath,size_t pathCapacity,char* error,size_t capacity)
{
    return SceneBake_SaveMaterial(map,fingerprint,NULL,root,resourcePath,pathCapacity,error,capacity);
}

static int siblingPath(const char* materialPath,const char* name,char* output,size_t size)
{
    if(!name[0] || strchr(name,'/') || strchr(name,'\\') || !strcmp(name,".") || !strcmp(name,".."))return 0;
    const char* slash=strrchr(materialPath,'/');size_t prefix=slash?(size_t)(slash-materialPath+1):0;
    if(prefix+strlen(name)>=size)return 0;
    memcpy(output,materialPath,prefix);strcpy(output+prefix,name);return 1;
}
static int materialColor(const char* line,const char* key,GRErgb24* color)
{
    float r,g,b;char extra;
    if(sscanf(line+strlen(key),"%f %f %f %c",&r,&g,&b,&extra)!=3 ||
        !isfinite(r)||!isfinite(g)||!isfinite(b)||r<0||g<0||b<0||r>1||g>1||b>1)return 0;
    *color=(GRErgb24){lroundf(r*255),lroundf(g*255),lroundf(b*255)};return 1;
}
GRE_Lightmap SceneBake_LoadMaterial(const char* path,GRE_Object4d mesh,GRE_List lights,
    GRE_Material* baseMaterial,char* error,size_t capacity)
{
    if(baseMaterial)*baseMaterial=NULL;
    FILE* f=fopen(path,"rb");GRE_Lightmap map=NULL;GRE_Material base=NULL;
    if(!f){failure(error,capacity,"找不到烘焙材质文件");return NULL;}
    char line[512],name[64]="",texture[64]="",lightmap[64]="",uvfile[64]="",mode[32]="";
    unsigned w=0,h=0,count=0,channel=0;uint64_t fingerprint=0;unsigned seen=0;
    GRErgb24 ambient={255,255,255},diffuse=ambient;
    char specularFile[64]="";unsigned specSeen=0,specPower=0,sr=0,sg=0,sb=0;
    gre_fvector4d view={0};float32 specKs=0;
    if(!fgets(line,sizeof(line),f))goto invalid;
    if(!strcmp(line,"YMGRE_LIGHTMAP 1\n")) {fclose(f);return loadLegacy(path,mesh,lights,error,capacity);}
    rewind(f);
    while(fgets(line,sizeof(line),f)) {
        char* p=line;while(*p==' '||*p=='\t')p++;
        if(!strncmp(p,"material ",9)){if(sscanf(p,"material %63s",name)!=1)goto invalid;seen|=1;}
        else if(!strncmp(p,"ambient ",8)){if(!materialColor(p,"ambient",&ambient))goto invalid;seen|=2;}
        else if(!strncmp(p,"diffuse ",8)){if(!materialColor(p,"diffuse",&diffuse))goto invalid;seen|=4;}
        else if(!strncmp(p,"texture ",8)){if(sscanf(p,"texture %63s",texture)!=1)goto invalid;seen|=8;}
        else if(!strncmp(p,"lightmap ",9)){if(sscanf(p,"lightmap %63s",lightmap)!=1)goto invalid;seen|=16;}
        else if(!strncmp(p,"lightmap_uv1 ",13)){if(sscanf(p,"lightmap_uv1 %63s",uvfile)!=1)goto invalid;seen|=32;}
        else if(!strncmp(p,"lightmap_size ",14)){if(sscanf(p,"lightmap_size %u %u",&w,&h)!=2)goto invalid;seen|=64;}
        else if(!strncmp(p,"lightmap_triangles ",19)){if(sscanf(p,"lightmap_triangles %u",&count)!=1)goto invalid;seen|=128;}
        else if(!strncmp(p,"lightmap_uv_channel ",20)){if(sscanf(p,"lightmap_uv_channel %u",&channel)!=1)goto invalid;seen|=256;}
        else if(!strncmp(p,"lightmap_mode ",14)){if(sscanf(p,"lightmap_mode %31s",mode)!=1)goto invalid;seen|=512;}
        else if(!strncmp(p,"lightmap_fingerprint ",21)){if(sscanf(p,"lightmap_fingerprint %" SCNu64,&fingerprint)!=1)goto invalid;seen|=1024;}
        else if(!strncmp(p,"lightmap_specular ",18)){if(sscanf(p,"lightmap_specular %63s",specularFile)!=1)goto invalid;specSeen|=1;}
        else if(!strncmp(p,"lightmap_view ",14)){if(sscanf(p,"lightmap_view %f %f %f",&view.x,&view.y,&view.z)!=3)goto invalid;specSeen|=2;}
        else if(!strncmp(p,"lightmap_specular_strength ",27)){if(sscanf(p,"lightmap_specular_strength %f",&specKs)!=1)goto invalid;specSeen|=4;}
        else if(!strncmp(p,"lightmap_specular_power ",24)){if(sscanf(p,"lightmap_specular_power %u",&specPower)!=1)goto invalid;specSeen|=8;}
        else if(!strncmp(p,"lightmap_specular_color ",24)){if(sscanf(p,"lightmap_specular_color %u %u %u",&sr,&sg,&sb)!=3)goto invalid;specSeen|=16;}
    }
    if(ferror(f)||seen!=2047||w<32||w>1024||h!=w||!mesh||mesh->nextObject||count!=(unsigned)mesh->polygonNum||
        !count||count>(w/8)*(w/8)||channel!=1||(strcmp(mode,"replace_diffuse") && strcmp(mode,"material_only") && strcmp(mode,"baked_color")))goto invalid;
    fclose(f);f=NULL;
    if(fingerprint!=SceneBake_ModeFingerprint(mesh,lights,!strcmp(mode,"material_only"))) {failure(error,capacity,"物体或灯光已变化，请重新烘焙");return NULL;}
    map=allocateMap(w,count);if(!map)goto invalid;
    map->materialOnly=!strcmp(mode,"material_only");map->enabled=!map->materialOnly;
    map->colorsBaked=!strcmp(mode,"baked_color");map->capturedAmbient=ambient;map->capturedDiffuse=diffuse;
    char resolved[4096];uint16 bw,bh;
    if(!siblingPath(path,lightmap,resolved,sizeof(resolved)))goto invalid;
    GRErgb24* pixels=readBmp(resolved,&bw,&bh);
    if(!pixels)goto missing;
    if(bw!=w||bh!=h){GRE_ImageBuff_Free(pixels);goto invalid;}
    GRE_ImageBuff_Free(map->pixels);map->pixels=pixels;
    if(specSeen) {
        if(specSeen!=31||map->materialOnly||!isfinite(view.x)||!isfinite(view.y)||!isfinite(view.z)||
            !isfinite(specKs)||specKs<0||specPower<1||specPower>255||sr>255||sg>255||sb>255)goto invalid;
        if(!siblingPath(path,specularFile,resolved,sizeof(resolved)))goto invalid;
        map->specularPixels=readBmp(resolved,&bw,&bh);if(!map->specularPixels)goto missing;
        if(bw!=w||bh!=h)goto invalid;
        map->referenceView=view;map->specularKs=specKs;map->specularPower=specPower;map->specularColor=(GRErgb24){sr,sg,sb};
    }
    if(!siblingPath(path,uvfile,resolved,sizeof(resolved)))goto invalid;
    f=fopen(resolved,"r");if(!f)goto missing;
    if(!fgets(line,sizeof(line),f)||strcmp(line,"triangle,corner,u1,v1\n"))goto invalid;
    for(uint32 i=0;i<count*3;i++) {
        unsigned triangle,corner;float u,v;char extra;
        if(!fgets(line,sizeof(line),f)||sscanf(line,"%u,%u,%f,%f %c",&triangle,&corner,&u,&v,&extra)!=4||
            triangle!=i/3||corner!=i%3||!isfinite(u)||!isfinite(v)||u<=0||v<=0||u>=1||v>=1)goto invalid;
        map->uv1[i*2]=u;map->uv1[i*2+1]=v;
    }
    if(fgetc(f)!=EOF||ferror(f))goto invalid;
    fclose(f);f=NULL;
    if(!siblingPath(path,texture,resolved,sizeof(resolved)))goto invalid;
    base=YMGRE_Creat_Material(name);if(!base)goto invalid;
    base->ambient=ambient;base->diffuse=diffuse;base->specular=(GRErgb24){0,0,0};
    if(map->specularPixels) {
        base->specular=map->specularColor;base->advanced=GRE_malloc0(sizeof(gre_material_advanced));
        if(!base->advanced)goto invalid;memset(base->advanced,0,sizeof(gre_material_advanced));
        base->advanced->specularPower=map->specularPower;
    }
    base->pixel=readBmp(resolved,&base->width,&base->height);if(!base->pixel)goto missing;
    if(baseMaterial)*baseMaterial=base;else YMGRE_Free_Material(base);
    return map;
missing:
    failure(error,capacity,"烘焙材质引用的图片或 UV1 文件缺失或损坏");goto cleanup;
invalid:
    failure(error,capacity,"烘焙材质格式无效或网格不匹配");
cleanup:
    if(f)fclose(f);YMGRE_Free_Lightmap(map);YMGRE_Free_Material(base);return NULL;
}
GRE_Lightmap SceneBake_Load(const char* path,GRE_Object4d mesh,GRE_List lights,char* error,size_t capacity)
{
    return SceneBake_LoadMaterial(path,mesh,lights,NULL,error,capacity);
}
