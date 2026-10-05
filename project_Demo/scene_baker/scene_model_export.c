#include "scene_model_export.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_OgreMeshInfo.h"
#include "YMGRE_LOD.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

static void u16(FILE* f,unsigned n){fputc(n&255,f);fputc(n>>8&255,f);}
static void u32(FILE* f,uint32 n){u16(f,n&65535);u16(f,n>>16);}
static void real32(FILE* f,float32 n){uint32 bits;memcpy(&bits,&n,4);u32(f,bits);}
static long begin(FILE* f,unsigned type){long at=ftell(f);u16(f,type);u32(f,0);return at;}
static void end(FILE* f,long at){long here=ftell(f);fseek(f,at+2,SEEK_SET);u32(f,(uint32)(here-at));fseek(f,here,SEEK_SET);}
static void element(FILE* f,unsigned type,unsigned semantic,unsigned offset,unsigned index)
{long c=begin(f,OGRE_GEOMETRY_VERTEX_ELEMENT);u16(f,0);u16(f,type);u16(f,semantic);u16(f,offset);u16(f,index);end(f,c);}
static int directories(const char* root)
{
    char path[3800];if(!root||!*root||strlen(root)>=sizeof(path))return 0;strcpy(path,root);
    for(char* p=path+1;;p++)if(*p=='/'||!*p){char c=*p;*p=0;
        if(mkdir(path,0755)&&errno!=EEXIST)return 0;
        struct stat s;if(stat(path,&s)||!S_ISDIR(s.st_mode))return 0;
        *p=c;if(!c)break;}
    return 1;
}
static int bitmap(const char* path,GRErgb24* pixels,unsigned w,unsigned h)
{
    FILE* f=fopen(path,"wb");if(!f)return 0;unsigned stride=(w*3+3)&~3u;
    fputs("BM",f);u32(f,54+stride*h);u32(f,0);u32(f,54);u32(f,40);u32(f,w);u32(f,h);
    u16(f,1);u16(f,24);u32(f,0);u32(f,stride*h);u32(f,2835);u32(f,2835);u32(f,0);u32(f,0);
    for(int y=(int)h-1;y>=0;y--){for(unsigned x=0;x<w;x++){GRErgb24 c=pixels[y*w+x];fputc(c.B,f);fputc(c.G,f);fputc(c.R,f);}
        for(unsigned i=w*3;i<stride;i++)fputc(0,f);}
    int ok=!ferror(f);if(fclose(f))ok=0;return ok;
}
static const char* materialName(GRE_Object4d part)
{return part->materiaName && part->materiaName[0]?part->materiaName:"Default";}
static const char* exportedMaterialName(GRE_Object4d part,const char* suffix,char out[32])
{
    const char* original=materialName(part);
    if(!suffix[0])return original;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(const unsigned char* p=(const unsigned char*)original;*p;p++){
        hash^=*p;hash*=UINT64_C(1099511628211);
    }
    snprintf(out,32,"%c%016llx",suffix[1],(unsigned long long)hash);
    return out;
}
static int writeMesh(const char* path,GRE_Object4d mesh,const gre_fvector4d* origin,
    const char* materialSuffix)
{
    FILE* f=fopen(path,"wb");if(!f)return 0;
    u16(f,OGRE_HEADER);fputs("[MeshSerializer_v1.40]\n",f);
    long root=begin(f,OGRE_MESH);fputc(0,f);
    float32 low[3]={INFINITY,INFINITY,INFINITY},high[3]={-INFINITY,-INFINITY,-INFINITY},radius=0;
    unsigned partIndex=0;
    for(GRE_Object4d part=mesh;part;part=part->nextObject,partIndex++) {
        unsigned uvCount=part->importedUvs?part->importedUvCount:1,stride=24+8*uvCount;
        char material[32];
        long sub=begin(f,OGRE_SUBMESH);
        fprintf(f,"%s\n",exportedMaterialName(part,materialSuffix,material));fputc(0,f);
        int wide=part->pointNum>65536;
        u32(f,(uint32)part->polygonNum*3u);fputc(wide,f);
        for(int i=0;i<part->polygonNum;i++)for(int j=0;j<3;j++) {
            if(wide)u32(f,part->polygonList[i].index[j]);
            else u16(f,part->polygonList[i].index[j]);
        }
        long geometry=begin(f,OGRE_GEOMETRY);u32(f,part->pointNum);
        long declaration=begin(f,OGRE_GEOMETRY_VERTEX_DECLARATION);
        element(f,2,1,0,0);element(f,2,4,12,0);
        for(unsigned uv=0;uv<uvCount;uv++)element(f,1,7,24+8*uv,uv);
        end(f,declaration);
        long buffer=begin(f,OGRE_GEOMETRY_VERTEX_BUFFER);u16(f,0);u16(f,stride);
        long data=begin(f,OGRE_GEOMETRY_VERTEX_BUFFER_DATA);
        for(int i=0;i<part->pointNum;i++) {
            gre_fvector4d p=part->pointList[i].pos;
            if(origin){p.x-=origin->x;p.y-=origin->y;p.z-=origin->z;}
            float32 xyz[3]={p.x,p.y,p.z};for(int k=0;k<3;k++){real32(f,xyz[k]);if(xyz[k]<low[k])low[k]=xyz[k];if(xyz[k]>high[k])high[k]=xyz[k];}
            float32 r=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);if(r>radius)radius=r;
            gre_fvector4d n=part->importedNormals?part->importedNormals[i]:part->pointList_wN[i].normal;
            real32(f,n.x);real32(f,n.y);real32(f,n.z);
            for(unsigned uv=0;uv<uvCount;uv++) {
                real32(f,uv?part->importedUvs[(i*uvCount+uv)*2]:part->pointList[i].u);
                real32(f,uv?part->importedUvs[(i*uvCount+uv)*2+1]:part->pointList[i].v);
            }
        }
        end(f,data);end(f,buffer);end(f,geometry);
        long operation=begin(f,OGRE_SUBMESH_OPERATION);u16(f,OGRE_TRIANGLE_LIST);end(f,operation);end(f,sub);
    }
    long bounds=begin(f,OGRE_MESH_BOUNDS);for(int i=0;i<3;i++)real32(f,low[i]);for(int i=0;i<3;i++)real32(f,high[i]);real32(f,radius);end(f,bounds);
    long names=begin(f,OGRE_SUBMESH_NAME_TABLE);
    for(unsigned i=0;i<partIndex;i++){long item=begin(f,OGRE_SUBMESH_NAME_TABLE_ELEMENT);u16(f,i);fprintf(f,"submesh%u\n",i);end(f,item);}
    end(f,names);end(f,root);int ok=!ferror(f);if(fclose(f))ok=0;return ok;
}
static uint64_t textureHash(const GRErgb24* pixels,unsigned width,unsigned height)
{
    uint64_t hash=UINT64_C(14695981039346656037);
    unsigned dimensions[2]={width,height};
    for(int i=0;i<2;i++)for(int shift=0;shift<32;shift+=8){
        hash^=(dimensions[i]>>shift)&255u;hash*=UINT64_C(1099511628211);
    }
    for(size_t i=0;i<(size_t)width*height;i++){
        hash^=pixels[i].R;hash*=UINT64_C(1099511628211);
        hash^=pixels[i].G;hash*=UINT64_C(1099511628211);
        hash^=pixels[i].B;hash*=UINT64_C(1099511628211);
    }
    return hash;
}
static int exportTexture(const char* directory,const char* prefix,unsigned index,
    const GRErgb24* pixels,unsigned width,unsigned height,int shared,
    char* filename,size_t capacity)
{
    int length=shared?snprintf(filename,capacity,"%s_%016llx.bmp",prefix,
        (unsigned long long)textureHash(pixels,width,height)):
        snprintf(filename,capacity,"%s_%u.bmp",prefix,index);
    if(length<0||(size_t)length>=capacity)return 0;
    char path[4096];
    if(snprintf(path,sizeof(path),"%s/%s",directory,filename)>=(int)sizeof(path))return 0;
    return shared&&access(path,F_OK)==0?1:bitmap(path,(GRErgb24*)pixels,width,height);
}
static int exportModel(GRE_Object4d mesh,GRE_List materials,GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,const char* stem,
    char* meshPath,size_t pathCapacity,char* error,size_t errorCapacity,
    int existingDirectory,const char* materialSuffix,int sharedTextures)
{
    char directory[3840],path[4096],materialPath[4096];unsigned count=0;
    if(!mesh || !materials || !meshPath || !pathCapacity || !stem || !*stem || strlen(stem)>80 || strchr(stem,'/') || strchr(stem,'\\'))goto invalid;
    for(GRE_Object4d part=mesh;part;part=part->nextObject) {
        if(++count>64||part->pointNum<1||part->pointNum>YMGRE_MAX_VERTICES||part->polygonNum<1||(uint32)part->polygonNum>UINT32_MAX/12u||!part->pointList||!part->polygonList||
            strlen(materialName(part))>=20||strpbrk(materialName(part)," \t\r\n{}")||part->importedUvCount>8||
            (part->importedUvs && !part->importedUvCount))goto invalid;
        for(int i=0;i<part->polygonNum;i++)for(int j=0;j<3;j++)
            if(part->polygonList[i].num!=3||!part->polygonList[i].index||part->polygonList[i].index[j]>=part->pointNum)goto invalid;
        if(!part->pointList_wN && !YMGRE_Object_GenerateVertexAttributes(part))goto invalid;
    }
    if(!directories(root))goto invalid;
    if(existingDirectory) {if(snprintf(directory,sizeof(directory),"%s",root)>=(int)sizeof(directory))goto invalid;}
    else if(snprintf(directory,sizeof(directory),"%s/model-XXXXXX",root)>=(int)sizeof(directory)||!mkdtemp(directory))goto invalid;
    snprintf(path,sizeof(path),"%s/%s.mesh",directory,stem);snprintf(materialPath,sizeof(materialPath),"%s/%s.material",directory,stem);
    int ok=writeMesh(path,mesh,origin,materialSuffix);FILE* f=ok?fopen(materialPath,"w"):NULL;ok=ok&&f!=NULL;
    unsigned index=0;
    for(GRE_Object4d part=mesh;ok&&part;part=part->nextObject,index++) {
        GRE_Material base=YMGRE_Material_Find(materials,part->materiaName);if(!base)base=fallback;
        GRErgb24 white={255,255,255},ambient=base?base->ambient:white,diffuse=base?base->diffuse:part->polygonList[0].planeColor,specular=base?base->specular:(GRErgb24){0,0,0};
        char texture[96];
        unsigned w=base&&base->pixel?base->width:1,h=base&&base->pixel?base->height:1;
        ok=w&&h&&exportTexture(directory,"texture",index,base&&base->pixel?base->pixel:&white,
            w,h,sharedTextures,texture,sizeof(texture));
        char material[32];
        fprintf(f,"material %s\n{\n    technique\n    {\n        pass\n        {\n"
            "            ambient %.9g %.9g %.9g\n            diffuse %.9g %.9g %.9g\n            specular %.9g %.9g %.9g\n"
            "            texture_unit\n            {\n                texture %s\n            }\n",
            exportedMaterialName(part,materialSuffix,material),ambient.R/255.0,ambient.G/255.0,ambient.B/255.0,diffuse.R/255.0,diffuse.G/255.0,diffuse.B/255.0,
            specular.R/255.0,specular.G/255.0,specular.B/255.0,texture);
        fprintf(f,"            lighting %s\n",base && base->unlit?"off":"on");
        if(base&&base->advanced) {
            if(base->advanced->rayType)fprintf(f,"            ymgre_ray %u %.9g %.9g %u %u %u %.9g\n",
                base->advanced->rayType,base->advanced->reflectivity,base->advanced->ior,
                base->advanced->transmissionColor.R,base->advanced->transmissionColor.G,base->advanced->transmissionColor.B,base->advanced->raySpecularStrength);
            fprintf(f,"            specular_power %u\n",base->advanced->specularPower);
            if(base->advanced->normalPixel) {
                ok=ok&&exportTexture(directory,"normal",index,base->advanced->normalPixel,
                    base->advanced->normalWidth,base->advanced->normalHeight,
                    sharedTextures,texture,sizeof(texture));
                fprintf(f,"            normal_map %s\n",texture);
            }
        }
        fputs("        }\n    }\n}\n",f);
    }
    if(f){if(ferror(f))ok=0;if(fclose(f))ok=0;}
    char absolute[4096];if(ok)ok=realpath(path,absolute)!=NULL && strlen(absolute)<pathCapacity;
    if(ok){strcpy(meshPath,absolute);return 1;}
    unlink(path);unlink(materialPath);
    if(!sharedTextures){for(unsigned i=0;i<count;i++){snprintf(path,sizeof(path),"%s/texture_%u.bmp",directory,i);unlink(path);snprintf(path,sizeof(path),"%s/normal_%u.bmp",directory,i);unlink(path);}rmdir(directory);}
    snprintf(error,errorCapacity,"模型导出写入失败，原模型保留");return 0;
invalid:
    snprintf(error,errorCapacity,"模型或导出目录无效（需静态三角网格及最多8套UV）");return 0;
}

int SceneModel_Export(GRE_Object4d mesh,GRE_List materials,GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,const char* stem,
    char* meshPath,size_t pathCapacity,char* error,size_t errorCapacity)
{return exportModel(mesh,materials,fallback,origin,root,stem,meshPath,pathCapacity,error,errorCapacity,0,"",0);}

int SceneModel_ExportIntoDirectory(GRE_Object4d mesh,GRE_List materials,GRE_Material fallback,
    const gre_fvector4d* origin,const char* directory,const char* stem,
    char* meshPath,size_t pathCapacity,char* error,size_t errorCapacity)
{return exportModel(mesh,materials,fallback,origin,directory,stem,meshPath,pathCapacity,error,errorCapacity,1,"",0);}

static void lodName(char out[64],const char* requested)
{
    size_t length=0;
    if(requested)for(const unsigned char* p=(const unsigned char*)requested;*p&&length<48;){
        unsigned char c=*p;
        if(c==' '||c=='\t'){if(length&&out[length-1]!='_')out[length++]='_';p++;}
        else if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                (c>='0'&&c<='9')||c=='_'||c=='-')out[length++]=(char)*p++;
        else if(c>=0xc2&&c<=0xf4){
            size_t bytes=c<0xe0?2:c<0xf0?3:4;
            if(length+bytes>48)break;
            size_t valid=1;
            while(valid<bytes&&p[valid]&&(p[valid]&0xc0)==0x80)valid++;
            if(valid==bytes){memcpy(out+length,p,bytes);length+=bytes;p+=bytes;}
            else p++;
        }else p++;
    }
    while(length&&out[length-1]=='_')length--;
    if(!length){memcpy(out,"object",7);return;}
    out[length]=0;
}
static int createLodFolder(char* folder,size_t capacity,const char* root,const char* objectName)
{
    char name[64];lodName(name,objectName);
    for(unsigned suffix=1;suffix<10000;suffix++){
        int n=suffix==1?snprintf(folder,capacity,"%s/%s_lod",root,name):
            snprintf(folder,capacity,"%s/%s_lod_%u",root,name,suffix);
        if(n<0||(size_t)n>=capacity)return 0;
        if(mkdir(folder,0755)==0)return 1;
        if(errno!=EEXIST)return 0;
    }
    return 0;
}
int SceneModel_ExportLODWithImageNamed(GRE_Object4d levels[3],GRE_List materials[3],
    const grass_impostor *farImage,GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,const char* objectName,float nearPixels,float middlePixels,
    float hysteresis,char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity)
{
    if(!levels||!materials||!root||!lodPath||!pathCapacity||
       !isfinite(nearPixels)||!isfinite(middlePixels)||
       !isfinite(hysteresis)||hysteresis<0||hysteresis>=1||
       nearPixels<=middlePixels||middlePixels<=0){
        snprintf(error,errorCapacity,"LOD 模型或阈值无效");return 0;
    }
    const char* names[3]={"near","middle","far"};
    char base[64];lodName(base,objectName);
    YMGRE_LOD_Object description;YMGRE_LOD_Init(&description,hysteresis);
    for(int i=0;i<3;i++){
        int image=i==2&&farImage;
        char resource[128];
        if(image)snprintf(resource,sizeof(resource),"views8.bin");
        else snprintf(resource,sizeof(resource),"%s_%s.mesh",base,names[i]);
        if((image?0:(!levels[i]||!materials[i]))||
           !YMGRE_LOD_Add(&description,image?YMGRE_LOD_IMAGE:YMGRE_LOD_MESH,
                          i==0?nearPixels:i==1?middlePixels:0,
                          resource,image?(void*)farImage:levels[i])){
            snprintf(error,errorCapacity,"LOD 层级资源无效");return 0;
        }
    }
    char folder[4096],meshPath[4096],descriptionPath[4096];
    if(!createLodFolder(folder,sizeof(folder),root,objectName)){
        snprintf(error,errorCapacity,"无法创建 LOD 资源目录");return 0;
    }
    for(int i=0;i<3;i++){
        if(i==2&&farImage){
            if(!SceneLodImage_Save(farImage,folder,origin,error,errorCapacity))return 0;
        }else{
            char stem[80],suffix[16];
            snprintf(stem,sizeof(stem),"%s_%s",base,names[i]);
            snprintf(suffix,sizeof(suffix),"_%s",names[i]);
            if(!exportModel(levels[i],materials[i],fallback,origin,folder,stem,
                meshPath,sizeof(meshPath),error,errorCapacity,1,suffix,1))return 0;
        }
    }
    if(snprintf(descriptionPath,sizeof(descriptionPath),"%s/object.lod",folder)>=(int)sizeof(descriptionPath)||
       strlen(descriptionPath)>=pathCapacity){
        snprintf(error,errorCapacity,"LOD 描述文件路径过长");return 0;
    }
    FILE* file=fopen(descriptionPath,"w");
    if(!file){snprintf(error,errorCapacity,"无法写入 LOD 描述文件");return 0;}
    fprintf(file,"YMGRE_LOD 1\nhysteresis %.2f\n",description.hysteresis);
    for(int i=0;i<3;i++)fprintf(file,"level %s %.6g %s\n",
        description.levels[i].kind==YMGRE_LOD_IMAGE?"image":"mesh",
        description.levels[i].min_pixels,description.levels[i].asset);
    int writeError=ferror(file),closeError=fclose(file);
    if(writeError||closeError){snprintf(error,errorCapacity,"无法完成 LOD 描述文件");return 0;}
    strcpy(lodPath,descriptionPath);return 1;
}

int SceneModel_ExportLODWithImage(GRE_Object4d levels[3],GRE_List materials[3],
    const grass_impostor *farImage,GRE_Material fallback,const gre_fvector4d* origin,
    const char* root,float nearPixels,float middlePixels,float hysteresis,
    char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity)
{
    return SceneModel_ExportLODWithImageNamed(levels,materials,farImage,fallback,origin,
        root,levels&&levels[0]?levels[0]->objName:NULL,nearPixels,middlePixels,
        hysteresis,lodPath,pathCapacity,error,errorCapacity);
}

int SceneModel_ExportLOD(GRE_Object4d levels[3],GRE_List materials[3],GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,float nearPixels,float middlePixels,
    float hysteresis,char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity)
{
    return SceneModel_ExportLODWithImage(levels,materials,NULL,fallback,origin,root,
        nearPixels,middlePixels,hysteresis,lodPath,pathCapacity,error,errorCapacity);
}

/* The caller owns this newly created bake directory. UV0 becomes the atlas;
   splitting vertices at chart seams makes it usable by ordinary Ogre loaders. */
int SceneModel_ExportBaked(GRE_Object4d source,GRE_Lightmap map,GRE_Material base,
    const gre_fvector4d* origin,const char* directory,char* path,size_t pathCapacity,
    char* error,size_t capacity)
{
    if(!source || !directory || !*directory || source->nextObject || !map || !map->pixels || !map->uv1 ||
        map->triangleCount!=(unsigned)source->polygonNum || map->triangleCount>YMGRE_MAX_VERTICES/3u) {
        snprintf(error,capacity,"烘焙图与三角网格不匹配");return 0;
    }
    char materialName[20];snprintf(materialName,sizeof(materialName),"Baked_%.6s",directory+ (strlen(directory)>6?strlen(directory)-6:0));
    GRE_Object4d mesh=YMGRE_Creat_Object(source->polygonNum*3,source->polygonNum,"Baked",materialName);
    if(!mesh)return 0;
    memset(mesh->polygonList,0,mesh->polygonNum*sizeof(gre_polygon4d));
    mesh->importedNormals=GRE_malloc1(mesh->pointNum*sizeof(gre_fvector4d));
    GRErgb24* pixels=malloc((size_t)map->width*map->height*sizeof(GRErgb24));
    if(!pixels || !mesh->importedNormals){free(pixels);YMGRE_Free_Object(mesh);return 0;}
    memset(pixels,0,(size_t)map->width*map->height*sizeof(GRErgb24));
    int grid=(int)ceil(sqrt((double)source->polygonNum)),tile=map->width/grid;
    for(int i=0;i<source->polygonNum;i++) {
        GRE_Polygon4d face=&source->polygonList[i],dest=&mesh->polygonList[i];
        dest->num=3;dest->pN=face->pN;dest->planeColor=(GRErgb24){255,255,255};
        dest->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
        if(!dest->index){free(pixels);YMGRE_Free_Object(mesh);return 0;}
        for(int j=0;j<3;j++) {
            int n=i*3+j;dest->index[j]=n;mesh->pointList[n]=source->pointList[face->index[j]];
            mesh->pointList[n].u=map->uv1[i*6+j*2];mesh->pointList[n].v=map->uv1[i*6+j*2+1];
            mesh->importedNormals[n]=source->renderMode==GRE_RenderMode_Face?face->pN:source->pointList_wN[face->index[j]].normal;
        }
        int left=i%grid*tile,top=i/grid*tile;float low=2.5f,high=tile-2.5f;
        GRErgb24 tint=map->colorsBaked?(GRErgb24){255,255,255}:(base?base->diffuse:face->planeColor);
        for(int y=0;y<tile;y++)for(int x=0;x<tile;x++) {
            float b=fmaxf(0,(x+.5f-low)/(high-low)),c=fmaxf(0,(y+.5f-low)/(high-low));
            if(b+c>1){float shift=(b+c-1)*.5f;b-=shift;c-=shift;if(b<0){b=0;c=1;}if(c<0){c=0;b=1;}}
            float weights[3]={1-b-c,b,c},u=0,v=0;
            for(int j=0;j<3;j++){u+=weights[j]*source->pointList[face->index[j]].u;v+=weights[j]*source->pointList[face->index[j]].v;}
            GRErgb24 color={255,255,255};
            if(base && base->pixel && base->width && base->height) {
                int tx=(int)(fabsf(u-(int)u)*base->width+1e-5f),ty=(int)(fabsf(v-(int)v)*base->height+1e-5f);
                if(tx>=base->width)tx=base->width-1;if(ty>=base->height)ty=base->height-1;
                color=base->pixel[ty*base->width+tx];
            }
            int index=(top+y)*map->width+left+x;
            GRErgb24 light=map->materialOnly?(GRErgb24){255,255,255}:map->pixels[index];
            GRErgb24 spec=(!map->materialOnly && map->specularPixels)?map->specularPixels[index]:(GRErgb24){0,0,0};
            pixels[index]=(GRErgb24){GREMin(255,color.R*(tint.R*light.R/255)/255+spec.R),
                GREMin(255,color.G*(tint.G*light.G/255)/255+spec.G),GREMin(255,color.B*(tint.B*light.B/255)/255+spec.B)};
        }
    }
    gre_material material={0};material.valid=1;material.name=materialName;
    material.ambient=material.diffuse=(GRErgb24){255,255,255};
    material.unlit=!map->materialOnly;material.width=map->width;material.height=map->height;material.pixel=pixels;
    gre_material_advanced advanced={0};
    if(map->materialOnly&&base&&base->advanced){advanced=*base->advanced;advanced.normalPixel=NULL;
        advanced.normalWidth=advanced.normalHeight=0;material.advanced=&advanced;}
    gre_list empty={0};
    int ok=exportModel(mesh,&empty,&material,origin,directory,"baked_model",path,pathCapacity,error,capacity,1,"",0);
    free(pixels);YMGRE_Free_Object(mesh);return ok;
}
