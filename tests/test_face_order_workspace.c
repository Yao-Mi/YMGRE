#include "YMGRE_RenderContext.h"
#include "YMGRE_Rendering_Pipeline.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned live;static int failNext;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line %d\n",__LINE__);exit(1);}}while(0)
void *GRE_malloc0(size_t n){if(failNext){failNext=0;return NULL;}void *p=malloc(n);if(p)live++;return p;}
void *GRE_malloc1(size_t n){return GRE_malloc0(n);}
void GRE_free0(void *p){if(p){CHECK(live);live--;free(p);}}void GRE_free1(void *p){GRE_free0(p);}
void GRE_memset(void *p,int c,size_t n){memset(p,c,n);}void GRE_memcpy(void*d,const void*s,size_t n){memcpy(d,s,n);}
int main(void){
 GRE_CompactVertex vertices[3]={0};GRE_Index ids[3]={0,1,2};GRE_CompactMesh mesh={0};GRE_MeshFaceCenter center;
 vertices[0].base.pos.x=1;vertices[1].base.pos.x=2;vertices[2].base.pos.x=3;
 mesh.vertices=vertices;mesh.indices=ids;mesh.vertexCount=3;mesh.triangleCount=1;
 CHECK(YMGRE_CompactMesh_PrepareFaceCenters(&mesh,&center,1));CHECK(center.x==2&&center.y==0&&center.z==0);
 CHECK(!YMGRE_CompactMesh_PrepareFaceCenters(&mesh,&center,0));CHECK(!YMGRE_CompactMesh_PrepareFaceCenters(NULL,&center,1));
 ids[2]=3;CHECK(!YMGRE_CompactMesh_PrepareFaceCenters(&mesh,&center,1));ids[2]=2;
 vertices[1].base.pos.x=NAN;CHECK(!YMGRE_CompactMesh_PrepareFaceCenters(&mesh,&center,1));vertices[1].base.pos.x=2;
 mesh.triangleList=1;mesh.indices=NULL;CHECK(YMGRE_CompactMesh_PrepareFaceCenters(&mesh,&center,1));CHECK(center.x==2);

 mesh.triangleList=0;mesh.indices=ids;mesh.triangleCount=1;
 CHECK(YMGRE_CompactMesh_ValidateIndices(&mesh));ids[2]=3;CHECK(!YMGRE_CompactMesh_ValidateIndices(&mesh));ids[2]=2;
 CHECK(!YMGRE_CompactMesh_ValidateIndices(NULL));mesh.indices=NULL;CHECK(!YMGRE_CompactMesh_ValidateIndices(&mesh));
 mesh.triangleCount=UINT32_MAX;CHECK(!YMGRE_CompactMesh_ValidateIndices(&mesh));mesh.triangleCount=1;
 mesh.triangleList=1;CHECK(YMGRE_CompactMesh_ValidateIndices(&mesh));mesh.triangleCount=2;CHECK(!YMGRE_CompactMesh_ValidateIndices(&mesh));
 CHECK(!YMGRE_RenderWorkspace_ReserveFaceOrder(NULL,20));
 GRE_RenderWorkspace w=YMGRE_Creat_RenderWorkspace();
 CHECK(YMGRE_RenderWorkspace_ReserveFaceOrder(w,32));void *saved=w->faceOrderScratch;unsigned before=live;
 CHECK(YMGRE_RenderWorkspace_ReserveFaceOrder(w,16));CHECK(saved==w->faceOrderScratch&&live==before);
 failNext=1;CHECK(!YMGRE_RenderWorkspace_ReserveFaceOrder(w,64));CHECK(saved==w->faceOrderScratch&&w->faceOrderCapacity==32&&live==before);
 CHECK(!YMGRE_RenderWorkspace_ReserveFaceOrder(w,65537));
 YMGRE_RenderWorkspace_Reserve(w,3,2,0);CHECK(saved==w->faceOrderScratch);
 CHECK(YMGRE_RenderWorkspace_ReserveFaceOrder(w,64));CHECK(w->faceOrderCapacity==64);
 YMGRE_Free_RenderWorkspace(w);CHECK(live==0);
 gre_render_workspace external;memset(&external,0,sizeof external);CHECK(!YMGRE_RenderWorkspace_ReserveFaceOrder(&external,32));CHECK(live==0);
 puts("PASS face-order ownership, capacity reuse, failed growth preservation, unrelated vertex growth, external rejection, no leaks");return 0;
}
