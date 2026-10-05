#include "demo_host.h"
#include "YMGRE_ScenceManager.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_TriangleRaster.h"
#include "YMGRE_LOD.h"
#include "YMCS_File_IO.h"
#include "YMGUI_Invalidate.h"
#include "grass_impostor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP 257
#define SIDE 65
#define FORWARD_LOAD_SCALE 2.5f /* Stretch only the forward extent, not the sideways extent */
#define WORLD_SIZE 96.f
#define HALF_WORLD (WORLD_SIZE*.5f)
#define TERRAIN_STEP (WORLD_SIZE/(SIDE-1))
#include "vegetation_catalog.h"
#define TREES 54
#define TREE_CLEARANCE .7f
#define ACCENT_COUNT 17800
#define ACCENT_RADIUS 42.f
#define PATCH_GRID 24
#define PATCH_SIZE (WORLD_SIZE/PATCH_GRID)
#define BLADE_GRID 28
#define BLADE_LAYERS 2 /* Baseline short-grass density: 98 blades per square unit */
#define BLADES_PER_PATCH (BLADE_GRID*BLADE_GRID*BLADE_LAYERS)
#define PATCH_COUNT (PATCH_GRID*PATCH_GRID)
#define SKY 512
#define GROUND_TEX 256
static GRErgb24 groundSoil[GROUND_TEX*GROUND_TEX],groundGreen[GROUND_TEX*GROUND_TEX];
static unsigned char heights[MAP*MAP];
static GRErgb24 skybox[6][SKY*SKY];
static unsigned rng=73921;
static GRE_Object4d grassCache[PATCH_COUNT];
static gre_listnode grassNodes[PATCH_COUNT];
static float grassScale[GRASS_TYPES];
typedef struct { float x,z,scale,angle;int kind;GRE_Object4d mesh;YMGRE_LOD_Instance lod; } accent_instance;
static accent_instance accentsOnMap[ACCENT_COUNT];
static gre_listnode accentNodes[ACCENT_COUNT];
static int accentChunkHead[PATCH_COUNT],accentNext[ACCENT_COUNT];
static int impostorInstances[ACCENT_COUNT],impostorCount,activeAccentChunks;
static float accentBaseY[ACCENT_COUNT];
static grass_impostor impostors[GRASS_TYPES];
static YMGRE_LOD_Object accentLod[GRASS_TYPES];
typedef struct { GRE_Object4d mesh; grass_impostor *image; } accent_lod_assets;
static void *resolveAccentLod(YMGRE_LOD_Kind kind,const char *name,void *context) {
    accent_lod_assets *assets=(accent_lod_assets*)context;
    if(kind==YMGRE_LOD_MESH&&!strcmp(name,"original"))return assets->mesh;
    if(kind==YMGRE_LOD_IMAGE&&!strcmp(name,"baked8"))return assets->image;
    return NULL;
}
typedef char blade_patch_must_fit_indices[(BLADES_PER_PATCH*3<=YMGRE_MAX_VERTICES)?1:-1];
static unsigned hash(unsigned x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; return x^(x>>16); }
static float unit(unsigned x) { return (hash(x)>>8)*(1.f/16777216.f); }
static float random01(void) { rng=1664525u*rng+1013904223u; return (rng>>8)*(1.f/16777216.f); }
static float clamp(float x,float a,float b) { return fmaxf(a,fminf(b,x)); }
static gre_fvector4d normalized(float x,float y,float z) { float k=1/sqrtf(x*x+y*y+z*z); return (gre_fvector4d){x*k,y*k,z*k,0}; }
static GRErgb24 mix(GRErgb24 a,GRErgb24 b,float t) { return (GRErgb24){a.R+(b.R-a.R)*t,a.G+(b.G-a.G)*t,a.B+(b.B-a.B)*t}; }
static float sample(int x,int z) { return heights[z*MAP+x]*(8.f/255); }
static float height(float x,float z) {
    /* Sample the same triangle split as the terrain mesh, including edges. */
    float u=clamp((x+HALF_WORLD)/TERRAIN_STEP,0,SIDE-1.001f),v=clamp((z+HALF_WORLD)/TERRAIN_STEP,0,SIDE-1.001f);
    int ix=(int)u,iz=(int)v; u-=ix; v-=iz;
    float a=sample(ix*4,iz*4),b=sample(ix*4+4,iz*4),c=sample(ix*4,iz*4+4),d=sample(ix*4+4,iz*4+4);
    return u+v<=1 ? a+(b-a)*u+(c-a)*v : d+(c-d)*(1-u)+(b-d)*(1-v);
}
static int readHeight(const char *path) {
    FILE *f=fopen(path,"rb"); char magic[3]; int w,h,max;
    if(!f) { fprintf(stderr,"Heightmap missing: %s\n",path); return 0; }
    int ok=fscanf(f,"%2s%d%d%d",magic,&w,&h,&max)==4 && !strcmp(magic,"P5") && w==MAP && h==MAP && max==255;
    if(ok) { int c=fgetc(f); if(c=='\r') c=fgetc(f); ok=c=='\n' && fread(heights,1,sizeof heights,f)==sizeof heights; }
    fclose(f); return ok;
}
static void finishMesh(GRE_Object4d o) {
    o->BoundingSphereR=0; o->boundType=GRE_Bounding_Sphere_R;
    for(int i=0;i<o->pointNum;i++) {
        gre_fvector4d p=o->pointList[i].pos;
        float x=p.x-o->WorldCoordinate.x,y=p.y-o->WorldCoordinate.y,z=p.z-o->WorldCoordinate.z;
        o->BoundingSphereR=fmaxf(o->BoundingSphereR,sqrtf(x*x+y*y+z*z));
    }
    for(int i=0;i<o->polygonNum;i++) {
        GRE_Polygon4d p=&o->polygonList[i];
        gre_fvector4d a=o->pointList[p->index[0]].pos,b=o->pointList[p->index[1]].pos,c=o->pointList[p->index[2]].pos;
        gre_fvector4d u={b.x-a.x,b.y-a.y,b.z-a.z,0},v={c.x-a.x,c.y-a.y,c.z-a.z,0};
        YMGRE_Fvector4d_CrossToResult(&u,&v,&p->pN); YMGRE_Fvector4d_Normalize(&p->pN);
    }
    YMGRE_Object_GenerateVertexAttributes(o); o->renderMode=GRE_RenderMode_Vertex; o->mirrorKs=0;
}
static GRE_Object4d terrain(void) {
    GRE_Object4d o=YMGRE_Creat_Object(SIDE*SIDE,(SIDE-1)*(SIDE-1)*2,"heightmap terrain","meadow");
    for(int z=0;z<SIDE;z++) for(int x=0;x<SIDE;x++) {
        float wx=x*TERRAIN_STEP-HALF_WORLD,wz=z*TERRAIN_STEP-HALF_WORLD;
        o->pointList[z*SIDE+x]=(gre_vertex4d){{wx,height(wx,wz),wz,1},(float)x/(SIDE-1),(float)z/(SIDE-1)};
    }
    int pi=0;
    for(int z=0;z<SIDE-1;z++) for(int x=0;x<SIDE-1;x++) {
        int a=z*SIDE+x; GRE_Index ids[6]={a,a+SIDE,a+1,a+1,a+SIDE,a+SIDE+1};
        for(int k=0;k<2;k++) { GRE_Polygon4d p=&o->polygonList[pi++]; p->num=3; p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index)); memcpy(p->index,ids+k*3,3*sizeof(GRE_Index)); p->planeColor=(GRErgb24){255,255,255}; }
    }
    finishMesh(o); return o;
}
static GRE_Object4d load(gre_scence *s,const char *folder,const char *name) {
    char path[1024]; snprintf(path,sizeof path,"%s/vegetation_lowpoly/%s/%s/%s.mesh",RESOURCE_ROOT,folder,name,name);
    FILE *f=fopen(path,"rb"); if(!f) { fprintf(stderr,"Missing mesh: %s\n",path); return NULL; } fclose(f);
    return YMGRE_LoadOgreMeshAndMaterial(s,path);
}
static GRE_Object4d plant(GRE_Object4d src,float x,float z,float scale,float angle) {
    GRE_Object4d o=YMGRE_Object_Clone(src); float c=cosf(angle),s=sinf(angle),y=height(x,z)-.04f;
    for(GRE_Object4d p=o;p;p=p->nextObject) {
        p->WorldCoordinate=(gre_fvector4d){x,y,z,1};
        for(int i=0;i<p->pointNum;i++) {
            gre_fvector4d v=p->pointList[i].pos;
            p->pointList[i].pos=(gre_fvector4d){x+scale*(v.x*c-v.z*s),y+scale*v.y,z+scale*(v.x*s+v.z*c),1};
            if(p->importedNormals) { v=p->importedNormals[i]; p->importedNormals[i]=(gre_fvector4d){v.x*c-v.z*s,v.y,v.x*s+v.z*c,0}; }
        }
        finishMesh(p);
    }
    return o;
}
static int addSceneProps(gre_scence *scene) {
    /* Both tank files use the same original coordinates: transform them together. */
    GRE_Object4d body=YMGRE_LoadOgreMeshAndMaterial(scene,RESOURCE_ROOT "/obj/Tank1_Body.mesh");
    GRE_Object4d head=YMGRE_LoadOgreMeshAndMaterial(scene,RESOURCE_ROOT "/obj/Tank1_Head.mesh");
    if(!body || !head) { YMGRE_Free_Object(body);YMGRE_Free_Object(head);return 0; }
    GRE_Object4d tail=body;while(tail->nextObject)tail=tail->nextObject;tail->nextObject=head;
    const float x=-4.1f,z=-17.f,scale=.72f,angle=.55f,c=cosf(angle),s=sinf(angle);
    float base=-1e9f;
    /* Raise the rigid tank until its lower hull clears the uneven terrain. */
    for(GRE_Object4d p=body;p;p=p->nextObject)for(int i=0;i<p->pointNum;i++) {
        gre_fvector4d v=p->pointList[i].pos;
        float wx=x+scale*(v.x*c-v.z*s),wz=z+scale*(v.x*s+v.z*c);
        base=fmaxf(base,height(wx,wz)-scale*v.y);
    }
    for(GRE_Object4d p=body;p;p=p->nextObject) {
        p->WorldCoordinate=(gre_fvector4d){x,base,z,1};
        for(int i=0;i<p->pointNum;i++) {
            gre_fvector4d v=p->pointList[i].pos;
            p->pointList[i].pos=(gre_fvector4d){x+scale*(v.x*c-v.z*s),base+scale*v.y,z+scale*(v.x*s+v.z*c),1};
            if(p->importedNormals) { v=p->importedNormals[i];p->importedNormals[i]=(gre_fvector4d){v.x*c-v.z*s,v.y,v.x*s+v.z*c,0}; }
        }
        finishMesh(p);
    }
    YMGRE_List_Append(&scene->ObjList,sizeof(gre_object4d),body);
    GRE_Material earth=YMGRE_Creat_Material("nature_earth");
    YMGRE_Bmp_File_LoadTo_Image(RESOURCE_ROOT "/地球贴图.bmp",&earth->pixel,&earth->width,&earth->height);
    if(!earth->pixel) { YMGRE_Free_Material(earth);return 0; }
    earth->ambient=earth->diffuse=(GRErgb24){255,255,255};
    YMGRE_List_Append(&scene->MaterialList,sizeof(gre_material),earth);
    enum { LAT=32,LON=64 };
    const float ex=4.1f,ez=-16.f,r=2.1f;
    float ey=height(ex,ez)+r;
    GRE_Object4d globe=YMGRE_Creat_Object((LAT+1)*(LON+1),2*LON*(LAT-1),"Earth globe","nature_earth");
    globe->WorldCoordinate=(gre_fvector4d){ex,ey,ez,1};
    globe->importedNormals=GRE_malloc1(globe->pointNum*sizeof(gre_fvector4d));
    for(int y=0;y<=LAT;y++)for(int ix=0;ix<=LON;ix++) {
        float u=(float)ix/LON,v=(float)y/LAT,phi=3.14159265f*v,theta=6.2831853f*u+1.1f;
        gre_fvector4d n={sinf(phi)*cosf(theta),cosf(phi),sinf(phi)*sinf(theta),0};
        int i=y*(LON+1)+ix;
        globe->pointList[i]=(gre_vertex4d){{ex+r*n.x,ey+r*n.y,ez+r*n.z,1},u,v};
        globe->importedNormals[i]=n;
    }
    float raise=0;
    for(int i=0;i<globe->pointNum;i++) { gre_fvector4d p=globe->pointList[i].pos;raise=fmaxf(raise,height(p.x,p.z)-p.y); }
    globe->WorldCoordinate.y+=raise;
    for(int i=0;i<globe->pointNum;i++)globe->pointList[i].pos.y+=raise;
    int pi=0;
    for(int y=0;y<LAT;y++)for(int ix=0;ix<LON;ix++) {
        int a=y*(LON+1)+ix,b=a+1,c0=a+LON+1,d=c0+1;
        GRE_Index ids[6]={a,c0,d,a,d,b};
        for(int k=0;k<2;k++) {
            if((y==LAT-1 && k==0)||(y==0 && k==1))continue;
            GRE_Polygon4d p=&globe->polygonList[pi++];p->num=3;p->planeColor=(GRErgb24){255,255,255};
            p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));memcpy(p->index,ids+k*3,3*sizeof(GRE_Index));
        }
    }
    finishMesh(globe);
    YMGRE_List_Append(&scene->ObjList,sizeof(gre_object4d),globe);
    printf("Scene props: complete tank at (-4.1, -17), Earth globe at (4.1, -16)\n");
    return 1;
}
static void updateGround(GRE_Material ground,GRE_Camera4d camera) {
    /* Match the fine-grass footprint, with a four-unit soft transition instead
     * of coloring individual chunk squares. Only a small texture is updated. */
    float fx=camera->move.cn.x,fz=camera->move.cn.z,len=hypotf(fx,fz);
    if(len>1e-6f) { fx/=len;fz/=len; } else { fx=0;fz=1; }
    for(int z=0;z<GROUND_TEX;z++)for(int x=0;x<GROUND_TEX;x++) {
        float dx=(x+.5f)*WORLD_SIZE/GROUND_TEX-HALF_WORLD-camera->pos.x;
        float dz=(z+.5f)*WORLD_SIZE/GROUND_TEX-HALF_WORLD-camera->pos.z;
        float forward=dx*fx+dz*fz,side=-dx*fz+dz*fx;
        if(forward>0)forward/=FORWARD_LOAD_SCALE;
        float distance=hypotf(forward,side);
        float coverage=clamp((21.f-distance)/4.f,0,1);
        if(distance>7)coverage*=clamp((forward-distance*.25f+4)/3.f,0,1);
        coverage=coverage*coverage*(3-2*coverage);
        int i=z*GROUND_TEX+x;ground->pixel[i]=mix(groundGreen[i],groundSoil[i],coverage);
    }
}
/* Dense short blades are batched by spatial patch. One double-sided triangle per blade,
 * rather than cloning a detailed imported mesh for every blade. The full 32
 * imported species remain as separate, sparser accent plants. */
static GRE_Object4d bladePatch(int px,int pz) {
    const int blades=BLADES_PER_PATCH;
    GRE_Object4d o=YMGRE_Creat_Object(blades*3,blades,"dense blade patch","grass_carpet");
    float originX=px*PATCH_SIZE-HALF_WORLD,originZ=pz*PATCH_SIZE-HALF_WORLD;
    o->WorldCoordinate=(gre_fvector4d){originX+PATCH_SIZE*.5f,height(originX+PATCH_SIZE*.5f,originZ+PATCH_SIZE*.5f),originZ+PATCH_SIZE*.5f,1};
    o->importedNormals=GRE_malloc1(blades*3*sizeof(gre_fvector4d));
    for(int iz=0;iz<BLADE_GRID;iz++)for(int ix=0;ix<BLADE_GRID;ix++)for(int layer=0;layer<BLADE_LAYERS;layer++) {
        int i=(iz*BLADE_GRID+ix)*BLADE_LAYERS+layer;unsigned seed=hash((pz*PATCH_GRID+px)*blades+i+37891);
        float x=originX+(ix+.15f+.7f*unit(seed))*PATCH_SIZE/BLADE_GRID;
        float z=originZ+(iz+.15f+.7f*unit(seed+1))*PATCH_SIZE/BLADE_GRID;
        float y=height(x,z)-.012f,h=.15f+.21f*unit(seed+2),a=unit(seed+3)*6.283185f;
        float width=.012f+.018f*unit(seed+4),dx=cosf(a)*width,dz=sinf(a)*width;
        float bend=.035f+.05f*unit(seed+5),u=unit(seed+6),v=unit(seed+7);
        o->pointList[i*3]=(gre_vertex4d){{x-dx,y,z-dz,1},u,v};
        o->pointList[i*3+1]=(gre_vertex4d){{x+dx,y,z+dz,1},u,v};
        o->pointList[i*3+2]=(gre_vertex4d){{x+cosf(a)*bend,y+h,z+sinf(a)*bend,1},u,v};
        for(int j=0;j<3;j++)o->importedNormals[i*3+j]=(gre_fvector4d){0,1,0,0};
        GRE_Polygon4d p=&o->polygonList[i];p->num=3;p->planeColor=(GRErgb24){255,255,255};
        p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));p->index[0]=i*3;p->index[1]=i*3+1;p->index[2]=i*3+2;
    }
    finishMesh(o);
    /* The sun is fixed: bake grass lighting once, then use the cheaper vertex
     * color rasterizer. Imported plants retain their texture and lighting. */
    for(int i=0;i<blades;i++) {
        GRErgb24 c=mix((GRErgb24){55,86,25},(GRErgb24){111,135,44},o->pointList[i*3].u);
        c=(GRErgb24){c.R*.83f,c.G*.86f,c.B*.87f};
        for(int j=0;j<3;j++)o->pointList_wN[i*3+j].color=c;
    }
    return o;
}
/* Stretch the original footprint only along the horizontal view direction.
 * Positive forward distances are divided by 2.5 before the original range
 * test; lateral and backward distances retain their old limits. */
static int withinLoadingRange(float dx,float dz,GRE_Camera4d camera,float radius) {
    float fx=camera->move.cn.x,fz=camera->move.cn.z,length=hypotf(fx,fz);
    if(length>1e-6f) { fx/=length;fz/=length; } else { fx=0;fz=1; }
    float forward=dx*fx+dz*fz,side=-dx*fz+dz*fx;
    if(forward>0)forward/=FORWARD_LOAD_SCALE;
    float distance=hypotf(forward,side);
    return distance<=radius && (distance<=7 || forward>=distance*.25f-4);
}
static int testLoadingBounds(void) {
    gre_camera4d c={0};c.move.cn=(gre_fvector4d){0,0,1,0};
    float r=17.f+PATCH_SIZE;
    if(!withinLoadingRange(0,r*FORWARD_LOAD_SCALE-.01f,&c,r) ||
        withinLoadingRange(0,r*FORWARD_LOAD_SCALE+.01f,&c,r) ||
        withinLoadingRange(r+.01f,10,&c,r) ||
        withinLoadingRange(-r-.01f,10,&c,r) ||
        withinLoadingRange(0,-r*2,&c,r))return 0;
    c.move.cn=(gre_fvector4d){1,0,0,0};
    if(!withinLoadingRange(r*FORWARD_LOAD_SCALE-.01f,0,&c,r) ||
        withinLoadingRange(10,r+.01f,&c,r))return 0;
    return 1;
}
static int updateGrass(GRE_Camera4d camera,gre_list *draw) {
    int n=0;
    for(int pz=0;pz<PATCH_GRID;pz++)for(int px=0;px<PATCH_GRID;px++) {
        int id=pz*PATCH_GRID+px;
        float x=(px+.5f)*PATCH_SIZE-HALF_WORLD,z=(pz+.5f)*PATCH_SIZE-HALF_WORLD;
        if(!withinLoadingRange(x-camera->pos.x,z-camera->pos.z,camera,17.f+PATCH_SIZE)) {
            YMGRE_Free_Object(grassCache[id]);grassCache[id]=NULL;continue;
        }
        if(!grassCache[id])grassCache[id]=bladePatch(px,pz);
        grassNodes[n]=(gre_listnode){sizeof(gre_object4d),grassCache[id],NULL};
        if(n)grassNodes[n-1].next=&grassNodes[n];
        n++;
    }
    draw->listhead=n?grassNodes:NULL;draw->len=n;
    return n*BLADES_PER_PATCH;
}
static void drawBlades(GRE_Camera4d source,gre_list *patches,GRE_RenderWorkspace ws) {
    gre_camera4d camera=*source;camera.img=*YMGRE_Camera_GetRenderTarget(source);camera.target=&camera.img;
    for(GRE_ListNode node=patches->listhead;node;node=node->next) {
        GRE_Object4d o=node->data;
        if(YMGRE_Object_FrustumCullingCal(o,&camera))continue;
        YMGRE_RenderWorkspace_Reserve(ws,o->pointNum,o->polygonNum,0);
        if(!YMGRE_RenderWorkspace_EnableVertexAttributes(ws,o->pointNum))continue;
        YMGRE_Object_WorldToCameraTo_wN(o,&camera.move.TMat,ws->pointList_wN);
        float sx=camera.perspectPlane.Dis*camera.img.width/(camera.perspectPlane.pR-camera.perspectPlane.pL);
        float sy=camera.perspectPlane.Dis*camera.img.height/(camera.perspectPlane.pU-camera.perspectPlane.pD);
        /* Project each shared vertex once. Screen bounds are clipped by the
         * rasterizer; only triangles crossing near/far planes need 3D clipping. */
        for(int i=0;i<o->pointNum;i++) {
            gre_fvector4d *p=&ws->pointList_wN[i].base.pos;
            if(p->z>=camera.frustum.Znear && p->z<=camera.frustum.Zfar) {
                p->x=p->x*sx/p->z+camera.img.width*.5f;
                p->y=-p->y*sy/p->z+camera.img.height*.5f;
            }
        }
        for(int pi=0;pi<o->polygonNum;pi++) {
            GRE_Polygon4d polygon=&o->polygonList[pi];
            int inside=0,behind=0;
            for(int k=0;k<3;k++) {
                float z=ws->pointList_wN[polygon->index[k]].base.pos.z;
                inside+=z>=camera.frustum.Znear && z<=camera.frustum.Zfar;
                behind+=z<camera.frustum.Znear;
            }
            if(inside==3) { YMGRE_TriangleRaster_FillVertexColor_wN(ws->pointList_wN,polygon,&camera);continue; }
            if(behind==3)continue;
            gre_vertex4d_wN input[3],clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
            for(int k=0;k<3;k++) {
                input[k]=o->pointList_wN[polygon->index[k]];
                YMGRE_Fvector4d_MatMultTo(&camera.move.TMat,&input[k].base.pos,&input[k].base.pos);
            }
            uint16 n=YMGRE_Polygon_FrustumClip_wN(input,3,clipped,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&camera);
            if(n<3)continue;
            YMGRE_VertexList_CameraToViewPlane_wN(clipped,n,camera.perspectPlane.Dis);
            YMGRE_VertexList_ViewPlaneToWindows_wN(clipped,n,&camera);
            for(uint16 k=1;k+1<n;k++) {
                GRE_Index indices[3]={0,k,k+1};gre_polygon4d triangle=*polygon;triangle.index=indices;
                YMGRE_TriangleRaster_FillVertexColor_wN(clipped,&triangle,&camera);
            }
        }
    }
}
static void freeGrass(void) { for(int i=0;i<PATCH_COUNT;i++) { YMGRE_Free_Object(grassCache[i]);grassCache[i]=NULL; } }
static int loadAccentLod(GRE_Object4d *sources) {
    char path[512],text[2048];
    snprintf(path,sizeof path,"%s/assets/grass.lod",NATURE_ROOT);
    FILE *file=fopen(path,"rb");if(!file)return 0;
    size_t len=fread(text,1,sizeof text-1,file);
    int complete=feof(file);fclose(file);if(!complete)return 0;
    text[len]=0;
    YMGRE_LOD_Object description;
    if(!YMGRE_LOD_Parse(&description,text))return 0;
    for(int k=0;k<GRASS_TYPES;k++) {
        accentLod[k]=description;
        accent_lod_assets assets={sources[k],&impostors[k]};
        if(!YMGRE_LOD_Resolve(&accentLod[k],resolveAccentLod,&assets))return 0;
    }
    return 1;
}
/* Placement records are bucketed once. Whole offscreen chunks are skipped;
 * only nearby instances allocate full meshes. Distant instances share eight
 * cached cutout views per species instead of duplicating their geometry. */
static void indexAccentChunks(void) {
    for(int i=0;i<PATCH_COUNT;i++)accentChunkHead[i]=-1;
    for(int i=0;i<ACCENT_COUNT;i++) {
        accent_instance *a=&accentsOnMap[i];
        int x=(int)clamp((a->x+HALF_WORLD)/PATCH_SIZE,0,PATCH_GRID-1);
        int z=(int)clamp((a->z+HALF_WORLD)/PATCH_SIZE,0,PATCH_GRID-1),id=z*PATCH_GRID+x;
        accentNext[i]=accentChunkHead[id];accentChunkHead[id]=i;
        accentBaseY[i]=height(a->x,a->z)-.04f;
    }
}
static int updateAccents(GRE_Camera4d camera,gre_list *draw) {
    int n=0;impostorCount=0;activeAccentChunks=0;
    for(int chunk=0;chunk<PATCH_COUNT;chunk++) {
        if(accentChunkHead[chunk]<0)continue;
        float x=(chunk%PATCH_GRID+.5f)*PATCH_SIZE-HALF_WORLD,z=(chunk/PATCH_GRID+.5f)*PATCH_SIZE-HALF_WORLD;
        gre_object4d bounds={0};bounds.boundType=GRE_Bounding_Sphere_R;bounds.scale=1;bounds.BoundingSphereR=5;
        bounds.WorldCoordinate=(gre_fvector4d){x,height(x,z)+.5f,z,1};
        int active=withinLoadingRange(x-camera->pos.x,z-camera->pos.z,camera,ACCENT_RADIUS+4) && !YMGRE_Object_FrustumCullingCal(&bounds,camera);
        if(active)activeAccentChunks++;
        for(int i=accentChunkHead[chunk];i>=0;i=accentNext[i]) {
            accent_instance *a=&accentsOnMap[i];float dx=a->x-camera->pos.x,dz=a->z-camera->pos.z;
            if(!active || !withinLoadingRange(dx,dz,camera,ACCENT_RADIUS)) { YMGRE_Free_Object(a->mesh);a->mesh=NULL;continue; }
            const grass_impostor *asset=&impostors[a->kind];
            float ca=cosf(a->angle),sa=sinf(a->angle);
            gre_fvector4d center={a->x+a->scale*(asset->center.x*ca-asset->center.z*sa),
                accentBaseY[i]+asset->center.y*a->scale,
                a->z+a->scale*(asset->center.x*sa+asset->center.z*ca),1};
            const YMGRE_LOD_Level *level=YMGRE_LOD_SelectCamera(&accentLod[a->kind],&a->lod,camera,
                center,asset->span*a->scale*.5f);
            if(!level||level->kind==YMGRE_LOD_IMAGE) {
                YMGRE_Free_Object(a->mesh);a->mesh=NULL;impostorInstances[impostorCount++]=i;continue;
            }
            if(!a->mesh)a->mesh=plant((GRE_Object4d)level->resource,a->x,a->z,a->scale,a->angle);
            accentNodes[n]=(gre_listnode){sizeof(gre_object4d),a->mesh,NULL};
            if(n)accentNodes[n-1].next=&accentNodes[n];
            n++;
        }
    }
    draw->listhead=n?accentNodes:NULL;draw->len=n;return n+impostorCount;
}
static void drawAccentImpostors(GRE_Camera4d camera) {
    for(int j=0;j<impostorCount;j++) {
        int i=impostorInstances[j];accent_instance *a=&accentsOnMap[i];
        const YMGRE_LOD_Level *level=&accentLod[a->kind].levels[a->lod.level];
        grass_impostor_draw((const grass_impostor*)level->resource,camera,a->x,accentBaseY[i],a->z,a->scale,a->angle);
    }
}
static void freeAccents(void) { for(int i=0;i<ACCENT_COUNT;i++) { YMGRE_Free_Object(accentsOnMap[i].mesh);accentsOnMap[i].mesh=NULL; } }
static gre_fvector4d cubeDirection(int face,float u,float v) {
    switch(face) {
        case 0:return normalized(1,v,-u); case 1:return normalized(-1,v,u);
        case 2:return normalized(u,1,-v); case 3:return normalized(u,-1,v);
        case 4:return normalized(u,v,1); default:return normalized(-u,v,-1);
    }
}
static void createSky(void) {
    gre_fvector4d sun=normalized(-.48f,.42f,.78f);
    for(int f=0;f<6;f++) for(int y=0;y<SKY;y++) for(int x=0;x<SKY;x++) {
        gre_fvector4d d=cubeDirection(f,2.f*x/(SKY-1)-1,2.f*y/(SKY-1)-1);
        float t=powf(clamp(d.y,0,1),.45f);
        GRErgb24 color=mix((GRErgb24){211,226,225},(GRErgb24){48,121,200},t);
        float dot=d.x*sun.x+d.y*sun.y+d.z*sun.z;
        float halo=powf(fmaxf(dot,0),50)*.55f;
        color=mix(color,(GRErgb24){255,239,187},halo);
        if(d.y>.1f) {
            float u=d.x/(d.y+.3f),v=d.z/(d.y+.3f);
            float clouds=sinf(u*3+v*1.7f)+.5f*sinf(u*8-v*4)+.22f*cosf(v*17+u*11);
            color=mix(color,(GRErgb24){249,248,231},clamp((clouds-.8f)*.5f,0,.65f));
        }
        if(dot>.99965f) color=(GRErgb24){255,255,237};
        skybox[f][y*SKY+x]=color;
    }
}
static GRErgb24 skyAt(gre_fvector4d d) {
    float ax=fabsf(d.x),ay=fabsf(d.y),az=fabsf(d.z),u,v; int f;
    if(ax>=ay && ax>=az) { f=d.x>0?0:1;u=(d.x>0?-d.z:d.z)/ax;v=d.y/ax; }
    else if(ay>=az) { f=d.y>0?2:3;u=d.x/ay;v=(d.y>0?-d.z:d.z)/ay; }
    else { f=d.z>0?4:5;u=(d.z>0?d.x:-d.x)/az;v=d.y/az; }
    float px=clamp((u+1)*.5f*(SKY-1),0,SKY-1.001f),py=clamp((v+1)*.5f*(SKY-1),0,SKY-1.001f);
    int x=(int)px,y=(int)py; px-=x;py-=y;
    return mix(mix(skybox[f][y*SKY+x],skybox[f][y*SKY+x+1],px),mix(skybox[f][(y+1)*SKY+x],skybox[f][(y+1)*SKY+x+1],px),py);
}
static void atmosphere(GRE_Camera4d cam) {
    GRE_RenderTarget t=YMGRE_Camera_GetRenderTarget(cam);
    for(int y=0;y<t->height;y++) for(int x=0;x<t->width;x++) {
        int i=y*t->width+x;
        float u=((x+.5f)/t->width*2-1)*cam->perspectPlane.kr,v=(1-(y+.5f)/t->height*2)*cam->perspectPlane.ku;
        gre_fvector4d d={cam->move.cn.x+u*cam->move.cu.x+v*cam->move.cv.x,cam->move.cn.y+u*cam->move.cu.y+v*cam->move.cv.y,cam->move.cn.z+u*cam->move.cu.z+v*cam->move.cv.z,0};
        GRErgb24 c;
        if(t->zbuff[i]>=cam->frustum.Zfar) c=skyAt(d);
        else { float distance=t->zbuff[i]*sqrtf(1+u*u+v*v); float fog=1-expf(-distance*distance*.000025f); c=mix(GRE_FramePixel_To_RGB24(t->data[i]),(GRErgb24){196,215,211},fog*.92f); }
        t->data[i]=GRE_FramePixel_From_RGB24(c);
    }
}
int main(int argc,char **argv) {
    int smoke=argc>1 && !strcmp(argv[1],"--smoke-test");
    int benchmark=argc>1 && !strcmp(argv[1],"--benchmark");
    if(smoke && !testLoadingBounds()) { fprintf(stderr,"Directional loading bounds failed\n");return 1; }
    if(!readHeight(argc>1 && !smoke && !benchmark?argv[1]:NATURE_ROOT "/assets/heightmap.pgm")) return 1;
    gre_scence scene={0}; int result=1;
    GRE_Object4d trees[TREE_TYPES]={0},grasses[GRASS_TYPES]={0},litter[LITTER_TYPES]={0};
    for(int i=0;i<TREE_TYPES;i++) if(!(trees[i]=load(&scene,"trees",treeNames[i]))) goto cleanup;
    for(int i=0;i<LITTER_TYPES;i++) if(!(litter[i]=load(&scene,"trees",litterNames[i]))) goto cleanup;
    for(int i=0;i<GRASS_TYPES;i++) {
        if(!(grasses[i]=load(&scene,"Grasses",grassNames[i]))) goto cleanup;
        float maxHeight=0;
        for(GRE_Object4d p=grasses[i];p;p=p->nextObject) { finishMesh(p);for(int j=0;j<p->pointNum;j++)maxHeight=fmaxf(maxHeight,p->pointList[j].pos.y); }
        /* Detailed imported plants are accent vegetation, not the grass carpet. */
        grassScale[i]=clamp(.4f/fmaxf(maxHeight,.1f),.5f,2.f);
    }
    float tx[TREES],tz[TREES],radii[TREES],baseRadius[TREE_TYPES]={0}; int count=0;
    for(int k=0;k<TREE_TYPES;k++) for(GRE_Object4d p=trees[k];p;p=p->nextObject) for(int i=0;i<p->pointNum;i++) { gre_fvector4d v=p->pointList[i].pos; baseRadius[k]=fmaxf(baseRadius[k],hypotf(v.x,v.z)); }
    for(int attempt=0;count<TREES && attempt<300000;attempt++) {
        float x=(random01()-.5f)*82,z=(random01()-.5f)*82,scale=.32f+random01()*.28f;
        int k=count%TREE_TYPES;float radius=baseRadius[k]*scale;
        if(fabsf(x)<8 && z<4 && z>-36) continue;
        int fits=1;for(int i=0;i<count;i++)if(hypotf(x-tx[i],z-tz[i])<radius+radii[i]+TREE_CLEARANCE)fits=0;
        if(!fits)continue;
        tx[count]=x;tz[count]=z;radii[count]=radius;count++;
        YMGRE_List_Append(&scene.ObjList,sizeof(gre_object4d),plant(trees[k],x,z,scale,random01()*6.283f));
        /* Use all branch/leaf models as forest-floor litter, not full-size trees. */
        for(int j=0;j<LITTER_TYPES;j++) {
            float angle=random01()*6.283f,r=.8f+random01()*radius;
            GRE_Object4d leaf=plant(litter[j],x+cosf(angle)*r,z+sinf(angle)*r,.35f+random01()*.6f,random01()*6.283f);
            for(GRE_Object4d p=leaf;p;p=p->nextObject) {
                for(int v=0;v<p->pointNum;v++) { gre_fvector4d *q=&p->pointList[v].pos;q->y=height(q->x,q->z)+.025f; }
                finishMesh(p);
            }
            YMGRE_List_Append(&scene.ObjList,sizeof(gre_object4d),leaf);
        }
    }
    if(count!=TREES) { fprintf(stderr,"Could not place requested trees: %d/%d\n",count,TREES);goto cleanup; }
    for(int i=0;i<count;i++)for(int j=0;j<i;j++)if(hypotf(tx[i]-tx[j],tz[i]-tz[j])+1e-4f<radii[i]+radii[j]+TREE_CLEARANCE)goto cleanup;
    GRE_Material ground=YMGRE_Creat_Material("meadow"); ground->width=ground->height=GROUND_TEX; ground->pixel=GRE_ImageBuff_Malloc(sizeof groundSoil);
    for(int z=0;z<1024;z++) for(int x=0;x<1024;x++) {
        float wx=x*(WORLD_SIZE/1024)-HALF_WORLD,wz=z*(WORLD_SIZE/1024)-HALF_WORLD;
        float patch=.5f+.22f*sinf(wx*.09f+wz*.055f)+.15f*cosf(wz*.18f-wx*.04f)+(random01()-.5f)*.13f;
        GRErgb24 c=mix((GRErgb24){158,104,48},(GRErgb24){221,179,104},clamp(patch,0,1));
        float shadow=1;
        for(int i=0;i<count;i++) { float dx=(wx-tx[i]-radii[i]*.25f)/radii[i],dz=(wz-tz[i]+radii[i]*.4f)/radii[i]; shadow=fminf(shadow,1-.28f*expf(-(dx*dx+dz*dz)*1.6f)); }
        /* Keep the original random sequence so vegetation placements stay fixed. */
        if(x%4==0 && z%4==0) {
            int i=(z/4)*GROUND_TEX+x/4;
            groundSoil[i]=mix((GRErgb24){0,0,0},c,shadow);
            groundGreen[i]=mix((GRErgb24){0,0,0},mix((GRErgb24){55,86,25},(GRErgb24){111,135,44},clamp(patch,0,1)),shadow);
        }
    }
    ground->ambient=ground->diffuse=(GRErgb24){255,255,255};
    YMGRE_List_Append(&scene.MaterialList,sizeof(gre_material),ground);
    YMGRE_List_Append(&scene.ObjList,sizeof(gre_object4d),terrain());
    if(!addSceneProps(&scene))goto cleanup;
    int accents=0,accentSpecies[GRASS_TYPES]={0};
    for(int k=0;k<GRASS_TYPES;k++) {
        int vertices=0;for(GRE_Object4d p=grasses[k];p;p=p->nextObject)vertices+=p->pointNum;
        /* Five times the previous imported-grass density. Loading coverage is
         * independent; it does not change the map's total placement count. */
        int copies=50*(vertices<200?16:3);
        for(int j=0;j<copies;j++) {
            if(accents>=ACCENT_COUNT)goto cleanup;
            float x=(random01()-.5f)*86,z=(random01()-.5f)*86;
            accentsOnMap[accents]=(accent_instance){x,z,grassScale[k]*(.7f+random01()*.6f),random01()*6.283f,k,NULL,{0}};
            accents++;accentSpecies[k]++;
        }
    }
    if(accents!=ACCENT_COUNT)goto cleanup;
    for(int k=0;k<GRASS_TYPES;k++)if(!accentSpecies[k])goto cleanup;
    indexAccentChunks();
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){205,224,240},.67f));
    GRE_Light4d sun=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,240,205},.65f);
    sun->pos=(gre_fvector4d){-4800,4200,7800,1};sun->proper.kc0=1;sun->proper.kc1=sun->proper.kc2=0;
    YMGRE_List_Append(&scene.LightList,sizeof(gre_light4d),sun);
    double bakeStart=YMGRE_DemoHost_Time();
    for(int k=0;k<GRASS_TYPES;k++)if(!grass_impostor_bake(&impostors[k],grasses[k],&scene.MaterialList,&scene.LightList))goto cleanup;
    if(!loadAccentLod(grasses)) { fprintf(stderr,"Cannot load grass LOD description\n");goto cleanup; }
    printf("Baked %d species x %d views in %.1f ms\n",GRASS_TYPES,GRASS_VIEWS,(YMGRE_DemoHost_Time()-bakeStart)*1000);fflush(stdout);
    createSky();
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,800,500,43,43,30,30); YMGRE_Camera_Frustum_Init(camera,.15f,220);
    GRE_RenderWorkspace workspace=YMGRE_Creat_RenderWorkspace(); YMGRE_DemoHost host={0};
    YMGRE_DemoHost_DefaultScale(1);
    if(!YMGRE_DemoHost_Init(&host,800,500,40)) { YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);goto cleanup; }
    char windowTitle[160];
    snprintf(windowTitle,sizeof windowTitle,"YMGRE - Dense Grass / index%zu | WASD move / arrows look / Q,E height / R reset",sizeof(GRE_Index)*8);
    YMGRE_DemoHost_SetTitle(&host,windowTitle);
    const YMGRE_DemoKeyBinding keys[]={{'w',1,0,0},{'s',2,0,0},{'a',4,0,0},{'d',8,0,0},{YMGRE_KEY_LEFT,16,0,0},{YMGRE_KEY_RIGHT,32,0,0},{YMGRE_KEY_UP,64,0,0},{YMGRE_KEY_DOWN,128,0,0},{'q',256,0,0},{'e',512,0,0},{'r',0,1,0},{YMGRE_KEY_ESCAPE,0,2,0}};
    YMGRE_DemoHost_BindKeys(&host,keys,sizeof keys/sizeof keys[0]);
    if(smoke) { host.frame_limit=4;YMGRE_DemoHost_InjectKey(&host,'w',1);YMGRE_DemoHost_InjectKey(&host,YMGRE_KEY_LEFT,1); }
    if(benchmark) { host.frame_limit=40;YMGRE_DemoHost_InjectKey(&host,'w',1); }
    GYOBJ image=YMGRE_DemoHost_AddTarget(&host,YMGRE_Camera_GetRenderTarget(camera),0,0,800,500);
    float x=0,z=-28,lift=1.8f,yaw=0,pitch=.035f; int first=1,frames=0; double last=YMGRE_DemoHost_Time(),total=0,steady=0;
    double loadingTime=0,materialTime=0,bladeTime=0,skyTime=0,impostorTime=0;
    printf("Index %zu bits; terrain %.3f x %.3f (area %.0f); %d trees / %d species (clearance %.1f); %d detailed accents / %d species; %.0f short blades per square unit; %u total blades, %u vertices per patch\n",sizeof(GRE_Index)*8,WORLD_SIZE,WORLD_SIZE,WORLD_SIZE*WORLD_SIZE,count,TREE_TYPES,TREE_CLEARANCE,accents,GRASS_TYPES,(float)BLADES_PER_PATCH/(PATCH_SIZE*PATCH_SIZE),PATCH_COUNT*BLADES_PER_PATCH,BLADES_PER_PATCH*3);fflush(stdout);
    GRE_ListNode sceneTail=scene.ObjList.listhead;while(sceneTail && sceneTail->next)sceneTail=sceneTail->next;
    printf("Grass loading: forward %.1f -> %.1f, lateral limit unchanged at %.1f; follows camera heading\n",17.f+PATCH_SIZE,(17.f+PATCH_SIZE)*FORWARD_LOAD_SCALE,17.f+PATCH_SIZE);fflush(stdout);
    int failed=0;unsigned long long initialFrameHash=0;
    do {
        double now=YMGRE_DemoHost_Time();float dt=clamp(now-last,0,.1f);last=now;
        uint32 held=YMGRE_DemoHost_HeldKeys(&host),actions=YMGRE_DemoHost_TakeActions(&host);if(actions&2)break;
        if(smoke && frames==0)held=0;
        if(smoke && frames==2) { x=30;z=24;yaw=1.4f;pitch=0;held=0;actions=4; }
        if(smoke && frames==3) { held=0;actions=1; }
        if(actions&1) {
            x=0;z=-28;lift=1.8f;yaw=0;pitch=.035f;
            for(int i=0;i<ACCENT_COUNT;i++)accentsOnMap[i].lod.initialized=0;
        }
        if(first||held||actions) {
            yaw+=((held&16?1:0)-(held&32?1:0))*dt*.8f;
            pitch=clamp(pitch+((held&64?1:0)-(held&128?1:0))*dt*.6f,-1,1);
            float forward=(held&1?1:0)-(held&2?1:0),right=(held&8?1:0)-(held&4?1:0);
            x=clamp(x+(sinf(yaw)*forward-cosf(yaw)*right)*dt*7,-HALF_WORLD+7,HALF_WORLD-7);
            z=clamp(z+(cosf(yaw)*forward+sinf(yaw)*right)*dt*7,-HALF_WORLD+7,HALF_WORLD-7);
            lift=clamp(lift+((held&512?1:0)-(held&256?1:0))*dt*5,1.3f,24);
            gre_fvector4d eye={x,height(x,z)+lift,z,1},target={x+sinf(yaw)*cosf(pitch),eye.y+sinf(pitch),z+cosf(yaw)*cosf(pitch),1};
            YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
            double start=YMGRE_DemoHost_Time();
            gre_list grassDraw={0};
            int visible=updateGrass(camera,&grassDraw);
            updateGround(ground,camera);
            gre_list accentDraw={0};int visibleAccents=updateAccents(camera,&accentDraw);
            if(first) { printf("Active accent chunks %d; full meshes %d; cached views %d\n",activeAccentChunks,accentDraw.len,impostorCount);printf("Visible short blades %d in %d batches; %d detailed accents; all species placed; tree spacing verified\n",visible,grassDraw.len,visibleAccents);fflush(stdout); }
            double loadedAt=YMGRE_DemoHost_Time();
            sceneTail->next=accentDraw.listhead;
            YMGRE_Camera_TanglePipline_wN(camera,&scene.LightList,&scene.ObjList,&scene.MaterialList,workspace);
            sceneTail->next=NULL;
            double materialAt=YMGRE_DemoHost_Time();
            drawAccentImpostors(camera);
            double impostorAt=YMGRE_DemoHost_Time();
            drawBlades(camera,&grassDraw,workspace);
            double bladesAt=YMGRE_DemoHost_Time();
            atmosphere(camera);
            double atmosphereAt=YMGRE_DemoHost_Time();
            if(frames>0) { loadingTime+=loadedAt-start;materialTime+=materialAt-loadedAt;impostorTime+=impostorAt-materialAt;bladeTime+=bladesAt-impostorAt;skyTime+=atmosphereAt-bladesAt; }
            if(smoke) {
                GRE_RenderTarget t=YMGRE_Camera_GetRenderTarget(camera);
                const unsigned char *bytes=(const unsigned char *)t->data;
                unsigned long long checksum=1469598103934665603ull;
                for(size_t i=0;i<(size_t)t->width*t->height*sizeof(*t->data);i++)checksum=(checksum^bytes[i])*1099511628211ull;
                if(frames==0)initialFrameHash=checksum;
                if(frames==3 && checksum!=initialFrameHash) { fprintf(stderr,"Grass reload changed the initial view\n");failed=1; }
                printf("Smoke frame %d: %d blades, %d batches, hash %llx\n",frames,visible,grassDraw.len,checksum);
            }
            double elapsed=YMGRE_DemoHost_Time()-start;total+=elapsed;if(frames>0)steady+=elapsed;frames++; YMGUI_Obj_Invalidate(image); first=0;
        }
    } while(YMGRE_DemoHost_Step(&host,60));
    printf("Rendered %d frames, mean %.1f ms, subsequent-frame mean %.1f ms\n",frames,frames?total*1000/frames:0,frames>1?steady*1000/(frames-1):0);
    if(benchmark && frames>1)printf("Subsequent frame breakdown: load %.1f ms, trees/terrain/imported grass %.1f ms, short blades %.1f ms, atmosphere %.1f ms, distant cached views %.1f ms\n",loadingTime*1000/(frames-1),materialTime*1000/(frames-1),bladeTime*1000/(frames-1),skyTime*1000/(frames-1),impostorTime*1000/(frames-1));
    YMGRE_DemoHost_Destroy(&host);YMGRE_Free_RenderWorkspace(workspace);YMGRE_Free_Camera(camera);result=failed || (smoke && frames<3) ? 1 : 0;
cleanup:
    freeGrass();freeAccents();
    for(int i=0;i<TREE_TYPES;i++)YMGRE_Free_Object(trees[i]);
    for(int i=0;i<GRASS_TYPES;i++)YMGRE_Free_Object(grasses[i]);
    for(int i=0;i<LITTER_TYPES;i++)YMGRE_Free_Object(litter[i]);
    YMGRE_List_Clear(&scene.ObjList,YMGRE_Free_Object);YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);YMGRE_List_Clear(&scene.LightList,YMGRE_Free_Light);
    return result;
}
