#include "YMGRE_LOD.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <math.h>
#include <stdio.h>

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"LOD check failed at line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void *resolve(YMGRE_LOD_Kind kind,const char *name,void *context)
{
    int complete=*(int*)context;
    if(kind==YMGRE_LOD_MESH && name[0]=='o')return (void*)1;
    if(complete && kind==YMGRE_LOD_IMAGE && name[0]=='b')return (void*)2;
    return NULL;
}

int main(void)
{
    const char *description="YMGRE_LOD 1\nhysteresis 0.10\nlevel mesh 90 original\nlevel image 0 baked8\n";
    YMGRE_LOD_Object lod;
    CHECK(YMGRE_LOD_Parse(&lod,description));
    CHECK(lod.count==2 && lod.levels[0].kind==YMGRE_LOD_MESH &&
          lod.levels[1].kind==YMGRE_LOD_IMAGE);
    int complete=0;
    CHECK(!YMGRE_LOD_Resolve(&lod,resolve,&complete));
    CHECK(!lod.levels[0].resource && !lod.levels[1].resource);
    complete=1;
    CHECK(YMGRE_LOD_Resolve(&lod,resolve,&complete));
    CHECK(lod.levels[0].resource==(void*)1 && lod.levels[1].resource==(void*)2);
    YMGRE_LOD_Instance instance={0};
    CHECK(YMGRE_LOD_Select(&lod,&instance,100)==&lod.levels[0]);
    CHECK(YMGRE_LOD_Select(&lod,&instance,86)==&lod.levels[0]);
    CHECK(YMGRE_LOD_Select(&lod,&instance,80)==&lod.levels[1]);
    CHECK(YMGRE_LOD_Select(&lod,&instance,95)==&lod.levels[1]);
    CHECK(YMGRE_LOD_Select(&lod,&instance,100)==&lod.levels[0]);
    CHECK(!YMGRE_LOD_Parse(&lod,"YMGRE_LOD 2\nlevel mesh 0 x\n"));
    CHECK(lod.count==2); /* a bad file cannot replace a working object */
    CHECK(!YMGRE_LOD_Parse(&lod,"YMGRE_LOD 1\nlevel mesh 90 a\nlevel image 90 b\n"));
    CHECK(lod.count==2);
    YMGRE_LOD_Init(&lod,.15f);
    CHECK(YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,50,NULL,(void*)1));
    CHECK(YMGRE_LOD_Add(&lod,YMGRE_LOD_IMAGE,0,NULL,(void*)2));
    CHECK(YMGRE_LOD_Select(&lod,&instance,0)->resource==(void*)2);
    YMGRE_LOD_Init(&lod,0);
    CHECK(YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,90,NULL,(void*)1));
    CHECK(YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,40,NULL,(void*)2));
    CHECK(YMGRE_LOD_Add(&lod,YMGRE_LOD_MESH,0,NULL,(void*)3));
    instance.initialized=0;
    CHECK(YMGRE_LOD_Select(&lod,&instance,100)->resource==(void*)1);
    CHECK(YMGRE_LOD_Select(&lod,&instance,60)->resource==(void*)2);
    CHECK(YMGRE_LOD_Select(&lod,&instance,10)->resource==(void*)3);

    GRE_Camera4d camera=YMGRE_Creat_Camera(0,640,480,35,35,30,30);
    CHECK(camera);
    gre_fvector4d eye={0,0,0,1},target={0,0,1,1};
    YMGRE_UVNCamera_PositionInit(camera,&eye,&target,NULL,0);
    gre_fvector4d center={0,0,10,1};
    float pixels=YMGRE_LOD_ProjectedDiameter(camera,center,1);
    CHECK(pixels>80 && pixels<90);
    center.z=20;
    CHECK(fabsf(YMGRE_LOD_ProjectedDiameter(camera,center,1)*2-pixels)<.001f);
    center.z=-10;
    CHECK(YMGRE_LOD_ProjectedDiameter(camera,center,1)==0);
    center.z=-.5f;
    CHECK(YMGRE_LOD_ProjectedDiameter(camera,center,1)>camera->img.height);
    YMGRE_Free_Camera(camera);
    puts("PASS: LOD descriptor, programmatic levels, projected size and hysteresis");
    return 0;
}
