#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Material.h"
#include "YMGRE_Profile.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"compact line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static unsigned live,allocations;
void *GRE_malloc0(size_t n){void *p=malloc(n);if(p){live++;allocations++;}return p;}
void *GRE_malloc1(size_t n){return GRE_malloc0(n);}
void GRE_free0(void *p){if(p){CHECK(live);live--;free(p);}}
void GRE_free1(void *p){GRE_free0(p);}
void GRE_memset(void *p,int c,size_t n){memset(p,c,n);}
void GRE_memcpy(void *d,const void *s,size_t n){memcpy(d,s,n);}
static void keep(void *p){(void)p;}
static void identity(gre_fmat4x4 *m){memset(m,0,sizeof(*m));for(int i=0;i<4;i++)m->val[i][i]=1;}
static GRE_FramePixel rowColor(uint16 y,uint16 h,void *user){
 CHECK(y<h);return GRE_FramePixel_From_RGB24((GRErgb24){y,*(uint8*)user,71});
}
static uint32 clockTick(void *user){return ++*(uint32*)user;}
int main(void){
 uint32 ticks=0;YMGRE_Profile_SetClock(clockTick,&ticks);YMGRE_Profile_Reset();
 enum {W=64,H=48,N=W*H,V=260};
 GRE_Camera4d c=YMGRE_Creat_Camera(0,W,H,45,45,45,45);
 YMGRE_Camera_Frustum_Init(c,1,30);identity(&c->move.TMat);c->pos=(gre_fvector4d){0,0,0,1};
 GRE_RenderWorkspace ws=YMGRE_Creat_RenderWorkspace();YMGRE_RenderWorkspace_Reserve(ws,0,0,2);
 GRE_Material mat=YMGRE_Creat_Material("mat");mat->ambient=(GRErgb24){90,70,120};
 GRE_Light4d a=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.7f);
 GRE_Light4d b=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){220,240,190},.8f);
 b->pos=(gre_fvector4d){0,0,0,1};b->proper.kc0=1;b->proper.kc1=.01f;b->proper.kc2=0;
 gre_list lights={0},objects={0},materials={0};YMGRE_List_Append(&lights,sizeof(*a),a);YMGRE_List_Append(&lights,sizeof(*b),b);
 YMGRE_List_Append(&materials,sizeof(*mat),mat);
 GRE_CompactVertex vertices[V];memset(vertices,0,sizeof(vertices));
 for(unsigned i=0;i<V;i++){vertices[i].base.pos=(gre_fvector4d){0,0,6,1};vertices[i].normal=(gre_fvector4d){0,0,-1,0};}
 const unsigned used[]={0,256,257,258};
 const float xy[4][2]={{-2,-2},{-2,2},{2,2},{2,-2}};
 for(unsigned i=0;i<4;i++){vertices[used[i]].base.pos.x=xy[i][0];vertices[used[i]].base.pos.y=xy[i][1];vertices[used[i]].base.u=i*.2f;vertices[used[i]].base.v=i*.1f;}
 GRE_Index indices[]={0,256,257,0,257,258};gre_fvector4d normals[2]={{0,0,-1,0},{0,0,-1,0}};
 GRE_CompactMesh mesh={0};mesh.vertices=vertices;mesh.indices=indices;mesh.faceNormals=normals;mesh.vertexCount=V;mesh.triangleCount=2;
 CHECK(YMGRE_CompactMesh_ValidateIndices(&mesh));mesh.indicesValidated=1;
 GRE_MeshBounds bounds;CHECK(YMGRE_CompactMesh_PrepareBounds(&mesh,&bounds));mesh.bounds=&bounds;
 GRE_MeshFaceCenter centers[2];CHECK(YMGRE_CompactMesh_PrepareFaceCenters(&mesh,centers,2));mesh.faceCenters=centers;
 gre_fvector4d positions[V];GRE_Index maps[V];uint32 hash[1024],positionCount;
 CHECK(YMGRE_CompactMesh_PrepareSharedPositions(&mesh,positions,V,maps,hash,1024,&positionCount));CHECK(positionCount==5);
 GRE_CompactVertex indexed[V];GRE_Index compactIds[6],remap[V];uint32 count;
 CHECK(YMGRE_CompactMesh_PrepareIndexed(&mesh,indexed,V,compactIds,remap,hash,1024,&count));CHECK(count==5);
 GRE_InstanceVertexCache cache;YMGRE_InstanceVertexCache_Init(&cache,NULL,0);
 GRE_InstanceFullVertex full[V];GRE_FramePixel frame[N];float depths[N];gre_fmat4x4 model;identity(&model);
 CHECK(YMGRE_RenderWorkspace_ReserveFaceOrder(ws,1024));
 for(unsigned pose=0;pose<200;pose++){
  model.val[0][3]=(pose%9-4.f)*.3f;model.val[1][3]=(pose%5-2.f)*.2f;model.val[2][3]=(pose%7)*.2f-1.5f;
  unsigned before=allocations;
  cache.fullVertices=NULL;cache.fullCapacity=0;mesh.positions=NULL;mesh.vertexPositionIndices=NULL;c->meshBoundsEnabled=0;
  YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));
  memcpy(frame,c->img.data,sizeof(frame));memcpy(depths,c->img.zbuff,sizeof(depths));
  unsigned visible=0;for(unsigned i=0;i<N;i++)visible+=depths[i]<30;CHECK(visible>80);
  for(unsigned mode=0;mode<4;mode++){
   cache.fullVertices=(mode&1)?full:NULL;cache.fullCapacity=(mode&1)?V:0;
   mesh.positions=(mode&2)?positions:NULL;mesh.vertexPositionIndices=(mode&2)?maps:NULL;mesh.positionCount=positionCount;c->meshBoundsEnabled=1;
   YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));
   CHECK(!memcmp(frame,c->img.data,sizeof(frame)));CHECK(!memcmp(depths,c->img.zbuff,sizeof(depths)));
  }
  CHECK(allocations==before);
 }
 /* Exercise independent triangles and sorting with overlapping near/far geometry. */
 GRE_CompactVertex listVertices[6];GRE_Index overlapIndices[1024*3];gre_fvector4d overlapNormals[1024];GRE_MeshFaceCenter overlapCenters[1024];
 for(unsigned i=0;i<6;i++){listVertices[i]=vertices[used[i%3]];if(i>=3)listVertices[i].base.pos.z=3;}
 GRE_CompactMesh overlap={0};overlap.vertices=listVertices;overlap.indices=overlapIndices;overlap.faceNormals=overlapNormals;overlap.vertexCount=6;overlap.triangleCount=1024;
 for(unsigned f=0;f<1024;f++){for(unsigned k=0;k<3;k++)overlapIndices[f*3+k]=(f&1)?k:k+3;overlapNormals[f]=(gre_fvector4d){0,0,-1,0};}
 CHECK(YMGRE_CompactMesh_PrepareFaceCenters(&overlap,overlapCenters,1024));overlap.faceCenters=overlapCenters;
 cache.fullVertices=NULL;cache.fullCapacity=0;identity(&model);unsigned beforeOverlap=allocations;
 uint32 capacity=ws->faceOrderCapacity;ws->faceOrderCapacity=0;
 YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&overlap,&model,1,&cache,ws));memcpy(frame,c->img.data,sizeof(frame));memcpy(depths,c->img.zbuff,sizeof(depths));
 ws->faceOrderCapacity=capacity;YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&overlap,&model,1,&cache,ws));CHECK(!memcmp(frame,c->img.data,sizeof(frame))&&!memcmp(depths,c->img.zbuff,sizeof(depths)));CHECK(allocations==beforeOverlap);
 overlap.triangleList=1;overlap.triangleCount=2;overlap.indices=NULL;overlap.faceCenters=NULL;
 YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&overlap,&model,1,&cache,ws));CHECK(!memcmp(frame,c->img.data,sizeof(frame))&&!memcmp(depths,c->img.zbuff,sizeof(depths)));
 mesh.positions=NULL;mesh.vertexPositionIndices=NULL;cache.fullVertices=NULL;cache.fullCapacity=0;
 identity(&model);c->meshBoundsEnabled=0;
 GRE_Object4d object=YMGRE_Creat_Object(4,2,"reference","mat");object->renderMode=GRE_RenderMode_Vertex;object->isVisible=1;
 object->boundType=GRE_Bounding_Sphere_R;object->BoundingSphereR=10;object->mirrorKs=0;object->WorldCoordinate=(gre_fvector4d){0,0,0,1};
 for(unsigned i=0;i<4;i++)object->pointList[i]=vertices[used[i]].base;
 for(unsigned f=0;f<2;f++){object->polygonList[f].num=3;object->polygonList[f].pN=normals[f];object->polygonList[f].index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));}
 const GRE_Index oi[6]={0,1,2,0,2,3};for(unsigned f=0;f<2;f++)memcpy(object->polygonList[f].index,oi+3*f,3*sizeof(GRE_Index));
 CHECK(YMGRE_Object_EnableVertexAttributes(object));for(unsigned i=0;i<4;i++){object->pointList_wN[i].normal=(gre_fvector4d){0,0,-1,0};object->pointList_wN[i].color=(GRErgb24){255,255,255};}
 YMGRE_List_Append(&objects,sizeof(*object),object);
 YMGRE_Camera_TanglePipline_wN(c,&lights,&objects,&materials,ws);memcpy(frame,c->img.data,sizeof(frame));memcpy(depths,c->img.zbuff,sizeof(depths));
 YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));
 CHECK(!memcmp(frame,c->img.data,sizeof(frame)));CHECK(!memcmp(depths,c->img.zbuff,sizeof(depths)));
 YMGRE_CameraImage_Init(c,(GRErgb24){50,50,50});CHECK(YMGRE_Camera_AppendOpaqueBatch_wN(c,&lights,&objects,&materials,ws));
 CHECK(!memcmp(frame,c->img.data,sizeof(frame)));CHECK(!memcmp(depths,c->img.zbuff,sizeof(depths)));
 CHECK(YMGRE_Camera_AppendOpaqueBatch_wN(c,&lights,&objects,&materials,ws));CHECK(!memcmp(frame,c->img.data,sizeof(frame)));
 #if YMGRE_ENABLE_LINEAR_COLOR
 c->linearColorEnabled=1;CHECK(!YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));CHECK(!YMGRE_Camera_AppendOpaqueBatch_wN(c,&lights,&objects,&materials,ws));c->linearColorEnabled=0;
 #endif
 #if YMGRE_ENABLE_TRANSPARENCY
 uint8 opacity=128;CHECK(YMGRE_Material_SetOpacity(mat,&opacity,1,1,0));CHECK(!YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));CHECK(!YMGRE_Camera_AppendOpaqueBatch_wN(c,&lights,&objects,&materials,ws));YMGRE_Material_ClearOpacity(mat);
 #endif
 CHECK(!YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,NAN,&cache,ws));model.val[0][0]=2;CHECK(!YMGRE_Camera_AppendMeshInstance_wN(c,&lights,mat,&mesh,&model,1,&cache,ws));identity(&model);
 uint32 old=mesh.triangleCount;mesh.triangleCount=UINT32_MAX;CHECK(!YMGRE_CompactMesh_ValidateIndices(&mesh));mesh.triangleCount=old;
 c->backgroundRow=rowColor;uint8 channel=83;c->backgroundUser=&channel;YMGRE_CameraImage_Init(c,(GRErgb24){0,0,0});
 for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++)CHECK(GRE_FramePixel_Equals(c->img.data[y*W+x],rowColor(y,H,&channel))&&c->img.zbuff[y*W+x]==30);
 YMGRE_List_Clear(&objects,keep);YMGRE_Free_Object(object);YMGRE_List_Clear(&lights,YMGRE_Free_Light);YMGRE_List_Clear(&materials,YMGRE_Free_Material);
 YMGRE_Free_RenderWorkspace(ws);YMGRE_Free_Camera(c);CHECK(live==0);
#if YMGRE_PROFILE_RENDER_STAGES
 CHECK(YMGRE_ProfileCycles[0]&&YMGRE_ProfileCycles[1]&&YMGRE_ProfileCycles[2]&&YMGRE_ProfileCycles[3]&&YMGRE_ProfileCycles[5]&&YMGRE_ProfileCycles[6]);
#else
 CHECK(ticks==0);
#endif
 YMGRE_Profile_Reset();for(unsigned i=0;i<8;i++)CHECK(YMGRE_ProfileCycles[i]==0);YMGRE_Profile_SetClock(NULL,NULL);
 puts("PASS compact caches/shared positions, legacy equivalence, opaque append, runtime fallback, zero frame allocation, no leaks");return 0;
}
