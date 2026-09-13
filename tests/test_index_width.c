#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_TriangleRaster.h"
#include "YMCS_File_IO.h"
#include "scene_model_export.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"index width line %d: %s\n",__LINE__,#c); exit(1); } } while(0)

static void roundtrip(int count)
{
    GRE_Object4d object=YMGRE_Creat_Object(count,1,"indices","IndexMat");
    CHECK(object);
    memset(object->pointList,0,(size_t)count*sizeof(*object->pointList));
    GRE_Index base=(GRE_Index)(count-3);
    object->pointList[base].pos=(gre_fvector4d){8,8,100,1};
    object->pointList[base+1].pos=(gre_fvector4d){56,8,100,1};
    object->pointList[base+2].pos=(gre_fvector4d){32,56,100,1};
    gre_polygon4d* p=object->polygonList;
    memset(p,0,sizeof(*p));p->num=3;p->index=GRE_PolyIndex_Malloc(3*sizeof(*p->index));
    p->index[0]=base;p->index[1]=base+1;p->index[2]=base+2;
    p->planeColor=p->planeColor_=(GRErgb24){255,255,255};p->pN=(gre_fvector4d){0,0,1,0};
    CHECK(YMGRE_Object_GenerateVertexAttributes(object));
    CHECK(fabsf(object->pointList_wN[base].normal.z)>.9f);
    gre_ray ray={{32,20,0,1},{0,0,1,0}};gre_ray_scene_hit hit;
    object->boundType=0;
    CHECK(YMGRE_Ray_IntersectObject(&ray,object,1,200,&hit));
    CHECK(fabsf(hit.hit.distance-100)<.001f);
    GRE_Camera4d camera=YMGRE_Creat_Camera(0,64,64,45,45,45,45);
    YMGRE_Camera_Frustum_Init(camera,1,200);
    YMGRE_CameraImage_Init(camera,(GRErgb24){0,0,0});
    YMGRE_TriangleRaster_Fill(object->pointList,p,p->planeColor,NULL,camera);
    CHECK(fabsf(camera->img.zbuff[20*64+32]-100)<.001f);
    YMGRE_Free_Camera(camera);
    char root[]="/tmp/ymgre-indices-XXXXXX",path[4096],error[256];
    CHECK(mkdtemp(root));gre_list materials={0};gre_material material={0};
    material.valid=1;material.diffuse=material.ambient=(GRErgb24){255,255,255};
    gre_fvector4d origin={0};
    CHECK(SceneModel_Export(object,&materials,&material,&origin,root,"indices",path,sizeof(path),error,sizeof(error)));
    // Check the on-disk flag independently of the in-memory type.
    FILE* f=fopen(path,"rb");CHECK(f);char line[128];
    CHECK(fseek(f,2,SEEK_SET)==0);CHECK(fgets(line,sizeof(line),f));
    CHECK(fseek(f,7+6,SEEK_CUR)==0);CHECK(fgets(line,sizeof(line),f));
    CHECK(fseek(f,5,SEEK_CUR)==0);CHECK(fgetc(f)==(count>65536));fclose(f);
    gre_scence scene={0};GRE_Object4d restored=YMGRE_LoadOgreMeshAndMaterial(&scene,path);
    CHECK(restored && restored->pointNum==count && restored->polygonNum==1);
    CHECK(!memcmp(restored->polygonList[0].index,p->index,3*sizeof(GRE_Index)));
    CHECK(restored->pointList[base+2].pos.y==56);
    YMGRE_Free_Object(restored);YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);
    YMGRE_Free_Object(object);
    unlink(path);char* dot=strrchr(path,'.');CHECK(dot);strcpy(dot,".material");unlink(path);
    char* slash=strrchr(path,'/');CHECK(slash);*slash=0;rmdir(path);rmdir(root);
}
int main(int argc,char** argv)
{
    if(argc==2) {gre_scence scene={0};GRE_Object4d o=YMGRE_LoadOgreMeshAndMaterial(&scene,argv[1]);
        CHECK(o);YMGRE_Free_Object(o);YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);return 0;}
    CHECK(sizeof(GRE_Index)*8==YMGRE_INDEX_BITS);
    roundtrip(3);
#if YMGRE_INDEX_BITS == 32
    roundtrip(70003);
    GRE_Object4d plane=YMGRE_MeshGener_RectPlane(10,10,256,256,(GRErgb24){255,255,255},"large","m");
    CHECK(plane && plane->pointNum==66049 && plane->polygonNum==131072);
    GRE_Index max=0;
    for(int i=0;i<plane->polygonNum;i++)for(int j=0;j<3;j++) {
        GRE_Index index=plane->polygonList[i].index[j];CHECK(index<(uint32)plane->pointNum);if(index>max)max=index;
    }
    CHECK(max==66048);YMGRE_Free_Object(plane);
#else
    roundtrip(65535);
#endif
    printf("Index width %d PASS: attributes, raster, ray and mesh roundtrip\n",YMGRE_INDEX_BITS);
    return 0;
}
