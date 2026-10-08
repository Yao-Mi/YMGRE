/* YMGRE_CompactMesh.c: compact instance subsystem adapted from ymgre_roam_perf320.
 * Borrowed geometry/cache storage is immutable during a draw; no frame allocations. */
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_TriangleRaster.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#ifndef YMGRE_PREVALIDATED_INDICES
#define YMGRE_PREVALIDATED_INDICES 0
#endif
static float32 *GRE_InstanceDepthRow(GRE_Camera4d camera,unsigned y) {
 return camera->img.zbuff+(size_t)y*camera->img.width;
}
static uint8 pointFrustumCode(GRE_Fvector4d p,GRE_Camera4d camera) {
 uint8 code=0;
 if(p->z-camera->frustum.Znear<0)code|=1;
 if(camera->frustum.Zfar-p->z<0)code|=2;
 if(p->x-camera->perspectPlane.kl*p->z<0)code|=4;
 if(camera->perspectPlane.kr*p->z-p->x<0)code|=8;
 if(p->y-camera->perspectPlane.kd*p->z<0)code|=16;
 if(camera->perspectPlane.ku*p->z-p->y<0)code|=32;
 return code;
}
int YMGRE_CompactMesh_PrepareIndexed(const GRE_CompactMesh *mesh,GRE_CompactVertex *vertices,uint32 capacity,
 GRE_Index *indices,GRE_Index *remap,uint32 *hashTable,uint32 hashCapacity,uint32 *vertexCount)
{
 if(!mesh||mesh->triangleCount>UINT32_MAX/3||!mesh->vertices||!vertices||!indices||!remap||!hashTable||!vertexCount||!mesh->vertexCount||
    capacity<mesh->vertexCount||mesh->vertexCount>UINT32_MAX/2||(uint64_t)mesh->vertexCount>(uint64_t)YMGRE_INDEX_MAX+1||
    hashCapacity<mesh->vertexCount*2||(hashCapacity&(hashCapacity-1))||(!mesh->triangleList&&!mesh->indices)||
    (mesh->triangleList&&mesh->triangleCount>mesh->vertexCount/3))return 0;
 for(uint32 h=0;h<hashCapacity;h++)hashTable[h]=UINT32_MAX;
 uint32 count=0;
 for(uint32 v=0;v<mesh->vertexCount;v++) {
  const GRE_CompactVertex *source=&mesh->vertices[v];uint32 bits[10];memcpy(bits,source,sizeof(bits));
  uint32 hash=2166136261u;for(unsigned b=0;b<10;b++)hash=(hash^bits[b])*16777619u;
  uint32 slot=hash&(hashCapacity-1),id;
  for(;;) {
   id=hashTable[slot];
   if(id==UINT32_MAX){id=count++;vertices[id]=*source;hashTable[slot]=id;break;}
   if(!memcmp(&vertices[id],source,sizeof(*source)))break;
   slot=(slot+1)&(hashCapacity-1);
  }
  remap[v]=(GRE_Index)id;
 }
 for(uint32 face=0;face<mesh->triangleCount;face++)for(unsigned k=0;k<3;k++) {
  uint32 index=mesh->triangleList?face*3+k:mesh->indices[face*3+k];if(index>=mesh->vertexCount)return 0;
  indices[face*3+k]=remap[index];
 }
 *vertexCount=count;return 1;
}
int YMGRE_CompactMesh_PrepareSharedPositions(const GRE_CompactMesh *mesh,gre_fvector4d *positions,uint32 capacity,
 GRE_Index *positionIndices,uint32 *hashTable,uint32 hashCapacity,uint32 *positionCount)
{
 if(!mesh||!mesh->vertices||!positions||!positionIndices||!hashTable||!positionCount||!mesh->vertexCount||
    capacity<mesh->vertexCount||mesh->vertexCount>UINT32_MAX/2||(uint64_t)mesh->vertexCount>(uint64_t)YMGRE_INDEX_MAX+1||hashCapacity<mesh->vertexCount*2||
    (hashCapacity&(hashCapacity-1)))return 0;
 for(uint32 h=0;h<hashCapacity;h++)hashTable[h]=UINT32_MAX;
 uint32 count=0;
 for(uint32 v=0;v<mesh->vertexCount;v++) {
  gre_fvector4d position=mesh->vertices[v].base.pos;uint32 bits[4];memcpy(bits,&position,sizeof(bits));
  uint32 hash=2166136261u;for(unsigned b=0;b<4;b++)hash=(hash^bits[b])*16777619u;
  uint32 slot=hash&(hashCapacity-1),id;
  for(;;) {
   id=hashTable[slot];
   if(id==UINT32_MAX){id=count++;positions[id]=position;hashTable[slot]=id;break;}
   if(!memcmp(&positions[id],&position,sizeof(position)))break;
   slot=(slot+1)&(hashCapacity-1);
  }
  positionIndices[v]=(GRE_Index)id;
 }
 *positionCount=count;return 1;
}
int YMGRE_CompactMesh_PrepareBounds(const GRE_CompactMesh *mesh,GRE_MeshBounds *bounds)
{
 if(!mesh||!bounds||!mesh->vertices||!mesh->vertexCount)return 0;
 bounds->min=bounds->max=mesh->vertices[0].base.pos;
 for(uint32 i=0;i<mesh->vertexCount;i++) {
  gre_fvector4d p=mesh->vertices[i].base.pos;
  if(p.w!=1||!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))return 0;
  bounds->min.x=GREMin(bounds->min.x,p.x);bounds->max.x=GREMax(bounds->max.x,p.x);
  bounds->min.y=GREMin(bounds->min.y,p.y);bounds->max.y=GREMax(bounds->max.y,p.y);
  bounds->min.z=GREMin(bounds->min.z,p.z);bounds->max.z=GREMax(bounds->max.z,p.z);
 }
 return 1;
}
int YMGRE_CompactMesh_PrepareClusters(const GRE_CompactMesh *mesh,uint32 size,GRE_MeshCluster *clusters,uint32 capacity)
{
 if(!mesh||mesh->triangleCount>UINT32_MAX/3||!size||!clusters||!mesh->vertices||!mesh->triangleCount||(!mesh->triangleList&&!mesh->indices)||
    (mesh->triangleList && mesh->triangleCount>mesh->vertexCount/3))return 0;
 uint32 count=mesh->triangleCount/size+(mesh->triangleCount%size!=0);if(capacity<count)return 0;
 for(uint32 n=0;n<count;n++) {
  GRE_MeshCluster *cluster=&clusters[n];cluster->firstFace=n*size;cluster->faceCount=GREMin(size,mesh->triangleCount-cluster->firstFace);
  uint8 first=1;
  for(uint32 face=cluster->firstFace;face<cluster->firstFace+cluster->faceCount;face++)for(unsigned k=0;k<3;k++) {
   uint32 index=mesh->triangleList?face*3+k:mesh->indices[face*3+k];if(index>=mesh->vertexCount)return 0;
   gre_fvector4d p=mesh->vertices[index].base.pos;
   if(p.w!=1||!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))return 0;
   if(first){cluster->bounds.min=cluster->bounds.max=p;first=0;}
   else {
    cluster->bounds.min.x=GREMin(cluster->bounds.min.x,p.x);cluster->bounds.max.x=GREMax(cluster->bounds.max.x,p.x);
    cluster->bounds.min.y=GREMin(cluster->bounds.min.y,p.y);cluster->bounds.max.y=GREMax(cluster->bounds.max.y,p.y);
    cluster->bounds.min.z=GREMin(cluster->bounds.min.z,p.z);cluster->bounds.max.z=GREMax(cluster->bounds.max.z,p.z);
   }
  }
 }
 return 1;
}
/* Camera-space enclosing AABB: conservative for a rotated local box. */
static int GRE_MeshBoundsInvisible(const GRE_MeshBounds *bounds,GRE_FMat4x4 matrix,GRE_Camera4d cam)
{
 if(!bounds||matrix->val[3][0]!=0||matrix->val[3][1]!=0||matrix->val[3][2]!=0||matrix->val[3][3]!=1)return 0;
 gre_fvector4d center={(bounds->min.x+bounds->max.x)*.5f,(bounds->min.y+bounds->max.y)*.5f,(bounds->min.z+bounds->max.z)*.5f,1},p;
 float32 ex=(bounds->max.x-bounds->min.x)*.5f,ey=(bounds->max.y-bounds->min.y)*.5f,ez=(bounds->max.z-bounds->min.z)*.5f;
 YMGRE_Fvector4d_MatMultTo(matrix,&center,&p);
 float32 rx=YMGRE_Fabs(matrix->val[0][0])*ex+YMGRE_Fabs(matrix->val[0][1])*ey+YMGRE_Fabs(matrix->val[0][2])*ez;
 float32 ry=YMGRE_Fabs(matrix->val[1][0])*ex+YMGRE_Fabs(matrix->val[1][1])*ey+YMGRE_Fabs(matrix->val[1][2])*ez;
 float32 rz=YMGRE_Fabs(matrix->val[2][0])*ex+YMGRE_Fabs(matrix->val[2][1])*ey+YMGRE_Fabs(matrix->val[2][2])*ez;
 float32 epsilon=1e-4f*(YMGRE_Fabs(p.x)+YMGRE_Fabs(p.y)+YMGRE_Fabs(p.z)+rx+ry+rz+1);
 float32 zmin=p.z-rz-epsilon,zmax=p.z+rz+epsilon;
 if(zmax<cam->frustum.Znear||zmin>cam->frustum.Zfar)return 1;
 if(p.x-cam->perspectPlane.kl*p.z+rx+YMGRE_Fabs(cam->perspectPlane.kl)*rz < -epsilon)return 1;
 if(cam->perspectPlane.kr*p.z-p.x+rx+YMGRE_Fabs(cam->perspectPlane.kr)*rz < -epsilon)return 1;
 if(p.y-cam->perspectPlane.kd*p.z+ry+YMGRE_Fabs(cam->perspectPlane.kd)*rz < -epsilon)return 1;
 if(cam->perspectPlane.ku*p.z-p.y+ry+YMGRE_Fabs(cam->perspectPlane.ku)*rz < -epsilon)return 1;
 if(zmin<=cam->frustum.Znear)return 0;
 float32 qmin=cam->perspectPlane.Dis/zmin,qmax=cam->perspectPlane.Dis/zmax;
 float32 sx=cam->img.width/(cam->perspectPlane.pR-cam->perspectPlane.pL),sy=cam->img.height/(cam->perspectPlane.pU-cam->perspectPlane.pD);
 float32 xmin=p.x-rx-epsilon,xmax=p.x+rx+epsilon,ymin=p.y-ry-epsilon,ymax=p.y+ry+epsilon;
 float32 left=GREMin(xmin*qmin,xmin*qmax)*sx+cam->img.width*.5f;
 float32 right=GREMax(xmax*qmin,xmax*qmax)*sx+cam->img.width*.5f;
 float32 top=-GREMax(ymax*qmin,ymax*qmax)*sy+cam->img.height*.5f;
 float32 bottom=-GREMin(ymin*qmin,ymin*qmax)*sy+cam->img.height*.5f;
 float32 guard=1e-4f*(YMGRE_Fabs(left)+YMGRE_Fabs(right)+YMGRE_Fabs(top)+YMGRE_Fabs(bottom)+1);
 left-=guard;right+=guard;top-=guard;bottom+=guard;
 if(right<0||bottom<0||left>cam->img.width-1||top>cam->img.height-1)return 1;
 left=GREMax(left,0);right=GREMin(right,cam->img.width-1);top=GREMax(top,0);bottom=GREMin(bottom,cam->img.height-1);
 int l=(int)left+(left>(int)left),rr=(int)right-(right<(int)right);
 int t=(int)top+(top>(int)top),bb=(int)bottom-(bottom<(int)bottom);
 if(l>rr||t>bb)return 1;
 if((rr-l+1)*(bb-t+1)>4096)return 0;
 for(int y=t;y<=bb;y++) {
  const float32 *depth=GRE_InstanceDepthRow(cam,y);
  for(int x=l;x<=rr;x++)if(depth[x]>=zmin)return 0;
 }
 return 1;
}
void YMGRE_InstanceVertexCache_Init(GRE_InstanceVertexCache *cache,void *scratch,size_t bytes) {
 if(!cache)return;memset(cache,0,sizeof(*cache));cache->meshScratch=scratch;cache->meshScratchBytes=bytes;
}
int YMGRE_CompactMesh_ValidateIndices(const GRE_CompactMesh *mesh)
{
 if(!mesh||!mesh->vertices||!mesh->vertexCount||mesh->triangleCount>UINT32_MAX/3)return 0;
 if(mesh->triangleList)return mesh->triangleCount<=mesh->vertexCount/3&&mesh->triangleCount<=(uint32)((GRE_Index)~0)/3;
 if(!mesh->indices)return 0;
 for(uint32 i=0;i<mesh->triangleCount;i++)for(unsigned k=0;k<3;k++)if(mesh->indices[i*3+k]>=mesh->vertexCount)return 0;
 return 1;
}
#ifndef YMGRE_CACHE_FACE_INDICES
#define YMGRE_CACHE_FACE_INDICES 1
#endif
#ifndef YMGRE_PREVALIDATED_INDICES
#define YMGRE_PREVALIDATED_INDICES 0
#endif
int YMGRE_CompactMesh_PrepareFaceCenters(const GRE_CompactMesh *m,GRE_MeshFaceCenter *centers,uint32 capacity){
 if(!m||m->triangleCount>UINT32_MAX/3||(m->triangleList&&m->triangleCount>m->vertexCount/3)||!centers||!m->vertices||capacity<m->triangleCount||(!m->triangleList&&!m->indices))return 0;
 for(uint32 i=0;i<m->triangleCount;i++){
  GRE_MeshFaceCenter center={0,0,0};
  for(unsigned k=0;k<3;k++){uint32 idx=m->triangleList?i*3+k:m->indices[i*3+k];if(idx>=m->vertexCount)return 0;gre_fvector4d v=m->vertices[idx].base.pos;center.x+=v.x;center.y+=v.y;center.z+=v.z;}
  center.x/=3.f;center.y/=3.f;center.z/=3.f;
  if(!isfinite(center.x)||!isfinite(center.y)||!isfinite(center.z))return 0;
  centers[i]=center;
 }
 return 1;
}
#ifndef YMGRE_INSTANCE_LAZY_RESET
#define YMGRE_INSTANCE_LAZY_RESET 1
#endif
#ifndef YMGRE_OPAQUE_FACE_BUCKETS
#define YMGRE_OPAQUE_FACE_BUCKETS 0
#endif
#ifndef YMGRE_OPAQUE_FACE_MIN_TRIANGLES
#define YMGRE_OPAQUE_FACE_MIN_TRIANGLES 512
#endif
#ifndef YMGRE_OPAQUE_FACE_MIN_AREA
#define YMGRE_OPAQUE_FACE_MIN_AREA 4096
#endif
#if YMGRE_OPAQUE_FACE_BUCKETS
static int GRE_OpaqueOrderWorthwhile(const GRE_CompactMesh *m,GRE_FMat4x4 matrix,GRE_Camera4d cam,float32 sx,float32 sy){
 if(m->triangleCount<YMGRE_OPAQUE_FACE_MIN_TRIANGLES)return 0;
 if(!m->bounds)return 1;
 float32 minx=1e30f,miny=1e30f,maxx=-1e30f,maxy=-1e30f;
 for(unsigned i=0;i<8;i++){
  gre_fvector4d p={(i&1)?m->bounds->max.x:m->bounds->min.x,(i&2)?m->bounds->max.y:m->bounds->min.y,(i&4)?m->bounds->max.z:m->bounds->min.z,1},v;
  YMGRE_Fvector4d_MatMultTo(matrix,&p,&v);
  if(v.z<=cam->frustum.Znear)return 1;
  float32 factor=cam->perspectPlane.Dis/v.z,x=v.x*factor*sx+cam->img.width*.5f,y=-v.y*factor*sy+cam->img.height*.5f;
  minx=GREMin(minx,x);maxx=GREMax(maxx,x);miny=GREMin(miny,y);maxy=GREMax(maxy,y);
 }
 float32 width=GREMax(0,GREMin(maxx,cam->img.width)-GREMax(minx,0));
 float32 height=GREMax(0,GREMin(maxy,cam->img.height)-GREMax(miny,0));
 return width*height>=YMGRE_OPAQUE_FACE_MIN_AREA;
}
static const uint32 *GRE_OpaqueFaceOrder(const GRE_CompactMesh *m,const float32 *zr,GRE_RenderWorkspace w){
 if(m->triangleCount<YMGRE_OPAQUE_FACE_MIN_TRIANGLES||!w->faceOrderScratch||w->faceOrderCapacity<m->triangleCount)return NULL;
 float32 *keys=w->faceOrderScratch;uint32 *order=(uint32*)(keys+w->faceOrderCapacity);
 uint32 offsets[9]={0};float32 lo=1e30f,hi=-1e30f;
 for(uint32 i=0;i<m->triangleCount;i++){
  float32 z=0;
  if(m->faceCenters){GRE_MeshFaceCenter v=m->faceCenters[i];z=zr[0]*v.x+zr[1]*v.y+zr[2]*v.z+zr[3];}
  else {for(unsigned k=0;k<3;k++){uint32 idx=m->triangleList?i*3+k:m->indices[i*3+k];if(idx>=m->vertexCount)return NULL;gre_fvector4d v=m->vertices[idx].base.pos;z+=zr[0]*v.x+zr[1]*v.y+zr[2]*v.z+zr[3];}}
  if(!isfinite(z))return NULL;
  keys[i]=z;if(z<lo)lo=z;if(z>hi)hi=z;
 }
 float32 factor=hi>lo?7.f/(hi-lo):0;
 for(uint32 i=0;i<m->triangleCount;i++){unsigned b=(unsigned)((keys[i]-lo)*factor);if(b>7)b=7;keys[i]=(float32)b;offsets[b+1]++;}
 for(unsigned b=1;b<=8;b++)offsets[b]+=offsets[b-1];
 for(uint32 i=0;i<m->triangleCount;i++)order[offsets[(unsigned)keys[i]]++]=i;
 return order;
}
#endif
#ifndef YMGRE_INSTANCE_SKIP_CACHED_NORMAL
#define YMGRE_INSTANCE_SKIP_CACHED_NORMAL 1
#endif
#ifndef YMGRE_INSTANCE_AFFINE_TRANSFORM
#define YMGRE_INSTANCE_AFFINE_TRANSFORM 1
#endif
static inline void GRE_InstanceTransform(GRE_FMat4x4 matrix,GRE_Fvector4d position,GRE_Fvector4d out,uint8 affine)
{
#if YMGRE_INSTANCE_AFFINE_TRANSFORM
 if(affine&&position->w==1){
  float32 x=matrix->val[0][0]*position->x+matrix->val[0][1]*position->y+matrix->val[0][2]*position->z+matrix->val[0][3];
  float32 y=matrix->val[1][0]*position->x+matrix->val[1][1]*position->y+matrix->val[1][2]*position->z+matrix->val[1][3];
  float32 z=matrix->val[2][0]*position->x+matrix->val[2][1]*position->y+matrix->val[2][2]*position->z+matrix->val[2][3];
  *out=(gre_fvector4d){x,y,z,1};return;
 }
#else
 (void)affine;
#endif
 YMGRE_Fvector4d_MatMultTo(matrix,position,out);
}
int YMGRE_Camera_AppendMeshInstance_wN(GRE_Camera4d camera,GRE_List lights,
 GRE_Material material,const GRE_CompactMesh *mesh,GRE_FMat4x4 model,
 float32 scale,GRE_InstanceVertexCache *cache,GRE_RenderWorkspace workspace)
{
 if(!camera||!lights||!material||!mesh||!model||!cache||!workspace||!isfinite(scale)||scale<=0)return 0;
 if(camera->wireFrame!=GRE_Render_Solid||!isfinite(scale*scale)||scale*scale<=0)return 0;
#if YMGRE_PREVALIDATED_INDICES
 if(!mesh->indicesValidated && !YMGRE_CompactMesh_ValidateIndices(mesh))return 0;
#else
 if(!YMGRE_CompactMesh_ValidateIndices(mesh))return 0;
#endif
 if(model->val[3][0]!=0||model->val[3][1]!=0||model->val[3][2]!=0||model->val[3][3]!=1)return 0;
 for(unsigned i=0;i<4;i++)for(unsigned j=0;j<4;j++)if(!isfinite(model->val[i][j]))return 0;
 /* Backface and normal transforms require rotation times positive uniform scale. */
 for(unsigned i=0;i<3;i++)for(unsigned j=i;j<3;j++){
  float32 dot=0;for(unsigned k=0;k<3;k++)dot+=model->val[k][i]*model->val[k][j];
  float32 expected=i==j?scale*scale:0;
  if(!isfinite(dot)||fabsf(dot-expected)>1e-4f*scale*scale)return 0;
 }
 float32 determinant=model->val[0][0]*(model->val[1][1]*model->val[2][2]-model->val[1][2]*model->val[2][1])
  -model->val[0][1]*(model->val[1][0]*model->val[2][2]-model->val[1][2]*model->val[2][0])
  +model->val[0][2]*(model->val[1][0]*model->val[2][1]-model->val[1][1]*model->val[2][0]);
 if(!isfinite(determinant)||determinant<=0)return 0;
 if(!mesh->vertices||!mesh->faceNormals||(!mesh->triangleList&&!mesh->indices))return 0;
 if(mesh->triangleList&&(mesh->triangleCount>mesh->vertexCount/3 || mesh->triangleCount>(uint32)((GRE_Index)~0)/3))return 0;
#if YMGRE_ENABLE_PBR
 if(camera->pbrEnabled && material->advanced && material->advanced->pbrEnabled)return 0;
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 if(camera->linearColorEnabled)return 0;
#endif
#if YMGRE_ENABLE_TRANSPARENCY
 if(material->advanced&&material->advanced->opacityPixel)return 0;
#endif
 if(material->doubleSided||material->unlit)return 0;
 if(material->advanced && material->advanced->normalPixel)return 0;
 GRE_RenderTarget target=YMGRE_Camera_GetRenderTarget(camera);if(!target)return 0;

 gre_camera4d cam=*camera;cam.img=*target;cam.target=target;
#if YMGRE_ENABLE_TRANSPARENCY
 cam.opacityPass=1;
#endif
 uint32 lightCount=0;for(GRE_ListNode n=lights->listhead;n;n=n->next)lightCount++;
 if(lightCount>workspace->lightMax || (lightCount&&!workspace->lightPos))return 0;
 uint32 li=0;for(GRE_ListNode n=lights->listhead;n;n=n->next)
  YMGRE_Point_WorldToCamera(&((GRE_Light4d)n->data)->pos,&workspace->lightPos[li++],&cam.move.TMat);
 gre_fvector4d relative={cam.pos.x-model->val[0][3],cam.pos.y-model->val[1][3],cam.pos.z-model->val[2][3],0};
 gre_fvector4d localCamera={0,0,0,1};float32 inverseSquare=1.0f/(scale*scale);
 localCamera.x=(model->val[0][0]*relative.x+model->val[1][0]*relative.y+model->val[2][0]*relative.z)*inverseSquare;
 localCamera.y=(model->val[0][1]*relative.x+model->val[1][1]*relative.y+model->val[2][1]*relative.z)*inverseSquare;
 localCamera.z=(model->val[0][2]*relative.x+model->val[1][2]*relative.y+model->val[2][2]*relative.z)*inverseSquare;
 gre_fmat4x4 combined;
 for(unsigned row=0;row<4;row++)for(unsigned col=0;col<4;col++) {
  float32 value=0;for(unsigned j=0;j<4;j++)value+=cam.move.TMat.val[row][j]*model->val[j][col];
  combined.val[row][col]=value;
 }
 if(cam.meshBoundsEnabled && GRE_MeshBoundsInvisible(mesh->bounds,&combined,&cam)){return 1;}
 uint8 affine=0;
#if YMGRE_INSTANCE_AFFINE_TRANSFORM
 affine=combined.val[3][0]==0&&combined.val[3][1]==0&&combined.val[3][2]==0&&combined.val[3][3]==1;
#endif
 uint8 sharedPositions=mesh->positions && mesh->vertexPositionIndices && mesh->positionCount;
 uint8 fullVertices=cache->fullVertices && mesh->vertexCount<=cache->fullCapacity && !mesh->triangleList && !sharedPositions;
 uint8 fullPositions=cache->fullVertices && sharedPositions && mesh->positionCount<=cache->fullCapacity;
 GRE_CompactMesh staged;
 if(cache->meshScratch && !((uintptr_t)cache->meshScratch&3) && mesh->vertexCount<=65535 && mesh->triangleCount<=65535) {
  size_t vertexBytes=sharedPositions?mesh->positionCount*sizeof(gre_fvector4d):mesh->vertexCount*sizeof(GRE_CompactVertex);
  size_t mapBytes=sharedPositions?mesh->vertexCount*sizeof(GRE_Index):0,alignedMapBytes=(mapBytes+3)&~(size_t)3;
  size_t indexBytes=mesh->triangleList?0:mesh->triangleCount*3*sizeof(GRE_Index),alignedIndexBytes=(indexBytes+3)&~(size_t)3;
  size_t normalBytes=mesh->triangleCount*sizeof(gre_fvector4d);
  if(vertexBytes+alignedMapBytes+alignedIndexBytes+normalBytes<=cache->meshScratchBytes) {
   uint8 *buffer=cache->meshScratch;staged=*mesh;
   if(sharedPositions){memcpy(buffer,mesh->positions,vertexBytes);staged.positions=(gre_fvector4d*)buffer;buffer+=vertexBytes;
    memcpy(buffer,mesh->vertexPositionIndices,mapBytes);staged.vertexPositionIndices=(GRE_Index*)buffer;buffer+=alignedMapBytes;
   } else {memcpy(buffer,mesh->vertices,vertexBytes);staged.vertices=(GRE_CompactVertex*)buffer;buffer+=vertexBytes;}
   if(indexBytes)memcpy(buffer,mesh->indices,indexBytes);staged.indices=(GRE_Index*)buffer;buffer+=alignedIndexBytes;
   memcpy(buffer,mesh->faceNormals,normalBytes);staged.faceNormals=(gre_fvector4d*)buffer;
   mesh=&staged;
  }
 }
 float32 inverseScale=1.0f/scale;
 if(fullVertices||fullPositions){
#if YMGRE_INSTANCE_LAZY_RESET
  uint8 next=(uint8)((cache->fullEpoch+2)&0xfe);
  if(!cache->fullEpoch||!next||cache->epochStorage!=cache->fullVertices||cache->epochCapacity!=cache->fullCapacity){
   for(uint32 i=0;i<cache->fullCapacity;i++)cache->fullVertices[i].lit=0;
   next=2;cache->epochStorage=cache->fullVertices;cache->epochCapacity=cache->fullCapacity;
  }
  cache->fullEpoch=next;
#else
  for(uint32 i=0;i<(fullVertices?mesh->vertexCount:mesh->positionCount);i++){cache->fullVertices[i].code=0x80;cache->fullVertices[i].lit=0;}
#endif
 }
 else if(sharedPositions||!mesh->triangleList)for(unsigned i=0;i<YMGRE_INSTANCE_CACHE_SIZE;i++)cache->entries[i].tag=UINT32_MAX;
 GRE_InstanceVertexCacheEntry transient[3];
 gre_vertex4d_wN input[3],clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];gre_fvector4d screen[3];uint8 codes[3];
 memset(input,0,sizeof(input));
 float32 sx=cam.img.width/(cam.perspectPlane.pR-cam.perspectPlane.pL),sy=cam.img.height/(cam.perspectPlane.pU-cam.perspectPlane.pD);
 gre_polygon4d polygon;memset(&polygon,0,sizeof polygon);polygon.num=3;polygon.planeColor=(GRErgb24){255,255,255};
 uint32 nextCluster=0;

#if YMGRE_OPAQUE_FACE_BUCKETS
 const uint32 *faceOrder=GRE_OpaqueOrderWorthwhile(mesh,&combined,&cam,sx,sy)?GRE_OpaqueFaceOrder(mesh,combined.val[2],workspace):NULL;
 for(uint32 ordinal=0;ordinal<mesh->triangleCount;ordinal++) {
  uint32 face=faceOrder?faceOrder[ordinal]:ordinal;
  if(!faceOrder && cam.meshBoundsEnabled && mesh->clusters
#else
 for(uint32 face=0;face<mesh->triangleCount;face++) {
  if(cam.meshBoundsEnabled && mesh->clusters
#endif
   && nextCluster<mesh->clusterCount && face==mesh->clusters[nextCluster].firstFace) {
   const GRE_MeshCluster *cluster=&mesh->clusters[nextCluster++];
   if(!cluster->faceCount||cluster->faceCount>mesh->triangleCount-face)return 0;
   if(GRE_MeshBoundsInvisible(&cluster->bounds,&combined,&cam)){
#if YMGRE_OPAQUE_FACE_BUCKETS
    ordinal+=cluster->faceCount-1;
#else
    face+=cluster->faceCount-1;
#endif
    continue;}
  }
#if YMGRE_CACHE_FACE_INDICES
  GRE_Index directIndices[3];
  if(mesh->triangleList){directIndices[0]=(GRE_Index)(face*3);directIndices[1]=(GRE_Index)(face*3+1);directIndices[2]=(GRE_Index)(face*3+2);}
#else
  GRE_Index directIndices[3]={(GRE_Index)(face*3),(GRE_Index)(face*3+1),(GRE_Index)(face*3+2)};
#endif
  const GRE_Index *indices=mesh->triangleList?directIndices:mesh->indices+face*3;
  if(
#if YMGRE_PREVALIDATED_INDICES
   !mesh->indicesValidated &&
#endif
   !mesh->triangleList && (indices[0]>=mesh->vertexCount||indices[1]>=mesh->vertexCount||indices[2]>=mesh->vertexCount))return 0;
  gre_fvector4d faceNormal=mesh->faceNormals[face];float32 facing;
  if(mesh->facePlaneOffsetsInW) {
   facing=YMGRE_Fvector4d_Dot(&faceNormal,&localCamera);
   float32 tolerance=1e-6f*(YMGRE_Fabs(faceNormal.x*localCamera.x)+YMGRE_Fabs(faceNormal.y*localCamera.y)+YMGRE_Fabs(faceNormal.z*localCamera.z)+YMGRE_Fabs(faceNormal.w)+1);
   if(YMGRE_Fabs(facing)<=tolerance) {
    gre_fvector4d first=mesh->vertices[indices[0]].base.pos;
    facing=faceNormal.x*(localCamera.x-first.x)+faceNormal.y*(localCamera.y-first.y)+faceNormal.z*(localCamera.z-first.z);
   }
  } else {
   gre_fvector4d first=mesh->vertices[indices[0]].base.pos;
   gre_fvector4d view={localCamera.x-first.x,localCamera.y-first.y,localCamera.z-first.z,0};
   facing=YMGRE_Fvector4d_Dot(&faceNormal,&view);
  }
  if(facing<=0){continue;}
#if YMGRE_CACHE_FACE_INDICES
  if(!mesh->triangleList){directIndices[0]=indices[0];directIndices[1]=indices[1];directIndices[2]=indices[2];indices=directIndices;}
#endif
  for(unsigned k=0;k<3;k++) {
   uint32 index=indices[k];
   if(fullVertices) {
    GRE_InstanceFullVertex *state=&cache->fullVertices[index];
#if YMGRE_INSTANCE_LAZY_RESET
    if((state->lit&0xfe)!=cache->fullEpoch){state->lit=cache->fullEpoch;state->code=0x80;}
#endif
    if(state->code&0x80) {
     gre_fvector4d position=mesh->vertices[index].base.pos;
     GRE_InstanceTransform(&combined,&position,&state->camera,affine);state->code=pointFrustumCode(&state->camera,&cam);
     if(!state->code){float32 factor=cam.perspectPlane.Dis/state->camera.z;state->screenX=state->camera.x*factor*sx+cam.img.width*.5f;state->screenY=state->camera.y*factor*-sy+cam.img.height*.5f;}
    }
    input[k].base.pos=state->camera;screen[k]=state->camera;if(!state->code){screen[k].x=state->screenX;screen[k].y=state->screenY;}codes[k]=state->code;continue;
   }
   if(sharedPositions) {
    uint32 positionIndex=mesh->vertexPositionIndices[index];if(positionIndex>=mesh->positionCount)return 0;
    if(fullPositions) {
     GRE_InstanceFullVertex *state=&cache->fullVertices[positionIndex];
#if YMGRE_INSTANCE_LAZY_RESET
    if((state->lit&0xfe)!=cache->fullEpoch){state->lit=cache->fullEpoch;state->code=0x80;}
#endif
     if(state->code&0x80) {
      gre_fvector4d position=mesh->positions[positionIndex];GRE_InstanceTransform(&combined,&position,&state->camera,affine);state->code=pointFrustumCode(&state->camera,&cam);
      if(!state->code){float32 factor=cam.perspectPlane.Dis/state->camera.z;state->screenX=state->camera.x*factor*sx+cam.img.width*.5f;state->screenY=state->camera.y*factor*-sy+cam.img.height*.5f;}
     }
     input[k].base.pos=state->camera;screen[k]=state->camera;if(!state->code){screen[k].x=state->screenX;screen[k].y=state->screenY;}codes[k]=state->code;
     transient[k].tag=UINT32_MAX;transient[k].lit=0;continue;
    }
    GRE_InstanceVertexCacheEntry *positionEntry=&cache->entries[positionIndex&(YMGRE_INSTANCE_CACHE_SIZE-1)];
    if(positionEntry->tag!=positionIndex) {
     gre_fvector4d position=mesh->positions[positionIndex];
     GRE_InstanceTransform(&combined,&position,&positionEntry->vertex.base.pos,affine);
     positionEntry->code=pointFrustumCode(&positionEntry->vertex.base.pos,&cam);
     if(!positionEntry->code){positionEntry->projected=positionEntry->vertex.base.pos;float32 factor=cam.perspectPlane.Dis/positionEntry->projected.z;positionEntry->projected.x=positionEntry->projected.x*factor*sx+cam.img.width*.5f;positionEntry->projected.y=positionEntry->projected.y*factor*-sy+cam.img.height*.5f;}
     positionEntry->tag=positionIndex;
    }
    input[k].base.pos=positionEntry->vertex.base.pos;screen[k]=positionEntry->vertex.base.pos;if(!positionEntry->code)screen[k]=positionEntry->projected;codes[k]=positionEntry->code;
    transient[k].tag=UINT32_MAX;transient[k].lit=0;continue;
   }
   GRE_InstanceVertexCacheEntry *entry=mesh->triangleList?&transient[k]:&cache->entries[index&(YMGRE_INSTANCE_CACHE_SIZE-1)];
   if(mesh->triangleList)entry->tag=UINT32_MAX;
   if(entry->tag!=index) {
    gre_fvector4d position=mesh->vertices[index].base.pos;
    gre_vertex4d_wN *v=mesh->triangleList?&input[k]:&entry->vertex;
    GRE_InstanceTransform(&combined,&position,&v->base.pos,affine);
    entry->lit=0; /* UV/normal reads are postponed until sample visibility survives. */
    entry->code=pointFrustumCode(&v->base.pos,&cam);
    if(!entry->code){entry->projected=v->base.pos;float32 factor=cam.perspectPlane.Dis/entry->projected.z;entry->projected.x=entry->projected.x*factor*sx+cam.img.width*.5f;entry->projected.y=entry->projected.y*factor*-sy+cam.img.height*.5f;}
    entry->tag=index;
   }
   if(!mesh->triangleList)input[k]=entry->vertex;screen[k]=input[k].base.pos;if(!entry->code)screen[k]=entry->projected;codes[k]=entry->code;
  }
  if(codes[0]&codes[1]&codes[2]){continue;}
  uint8 activeLighting[3]={0,0,0};
  for(unsigned k=0;k<3;k++) {
   if(fullVertices) {
    GRE_InstanceFullVertex *state=&cache->fullVertices[indices[k]];input[k].color=(GRErgb24){255,255,255};
    if(state->lit&1) {
     input[k].base.u=state->u;input[k].base.v=state->v;input[k].vertexLighting=state->lighting;input[k].vertexSpecular=state->specular;
#if !YMGRE_INSTANCE_SKIP_CACHED_NORMAL
     input[k].normal=(gre_fvector4d){0,0,0,0};
#endif
     /* Cached vertices retain initialized normals; masked lighting skips
      * them and the final vertex-lit raster does not consume normals. */
    } else {
     const GRE_CompactVertex *source=&mesh->vertices[indices[k]];input[k].base.u=source->base.u;input[k].base.v=source->base.v;
     gre_fvector4d n=source->normal;
     input[k].normal.x=(combined.val[0][0]*n.x+combined.val[0][1]*n.y+combined.val[0][2]*n.z)*inverseScale;
     input[k].normal.y=(combined.val[1][0]*n.x+combined.val[1][1]*n.y+combined.val[1][2]*n.z)*inverseScale;
     input[k].normal.z=(combined.val[2][0]*n.x+combined.val[2][1]*n.y+combined.val[2][2]*n.z)*inverseScale;input[k].normal.w=0;
     activeLighting[k]=1;
    }
    continue;
   }
   GRE_InstanceVertexCacheEntry *entry=(mesh->triangleList||sharedPositions)?&transient[k]:&cache->entries[indices[k]&(YMGRE_INSTANCE_CACHE_SIZE-1)];
   if(entry->tag==indices[k]&&entry->lit){input[k].vertexLighting=entry->vertex.vertexLighting;input[k].vertexSpecular=entry->vertex.vertexSpecular;}
   else {
    const GRE_CompactVertex *source=&mesh->vertices[indices[k]];
    input[k].base.u=source->base.u;input[k].base.v=source->base.v;
    input[k].color=(GRErgb24){255,255,255};
    gre_fvector4d normal=source->normal;
    input[k].normal.x=(combined.val[0][0]*normal.x+combined.val[0][1]*normal.y+combined.val[0][2]*normal.z)*inverseScale;
    input[k].normal.y=(combined.val[1][0]*normal.x+combined.val[1][1]*normal.y+combined.val[1][2]*normal.z)*inverseScale;
    input[k].normal.z=(combined.val[2][0]*normal.x+combined.val[2][1]*normal.y+combined.val[2][2]*normal.z)*inverseScale;input[k].normal.w=0;
    if(!mesh->triangleList&&!sharedPositions) {
     YMGRE_TriangleRaster_ComputeVertexLighting_wN(&input[k],1,&polygon,material,lights,workspace->lightPos,&cam.move.TMat,0);
     if(entry->tag==indices[k]){entry->vertex=input[k];entry->lit=1;}
    }
   }
  }
  if(fullVertices && (activeLighting[0]||activeLighting[1]||activeLighting[2])) {
   YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN(input,3,&polygon,material,lights,workspace->lightPos,&cam.move.TMat,0,activeLighting);
   for(unsigned k=0;k<3;k++)if(activeLighting[k]) {
    GRE_InstanceFullVertex *state=&cache->fullVertices[indices[k]];state->u=input[k].base.u;state->v=input[k].base.v;
    state->lighting=input[k].vertexLighting;state->specular=input[k].vertexSpecular;state->lit|=1;
   }
  } else if(mesh->triangleList||sharedPositions)YMGRE_TriangleRaster_ComputeVertexLighting_wN(input,3,&polygon,material,lights,workspace->lightPos,&cam.move.TMat,0);

  uint16 count;GRE_Vertex4d_wN rasterVertices=clipped;
  if(!(codes[0]|codes[1]|codes[2])){count=3;rasterVertices=input;for(unsigned k=0;k<3;k++)input[k].base.pos=screen[k];}
  else {
   count=YMGRE_Polygon_FrustumClip_wN(input,3,clipped,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&cam);
if(count<3){continue;}YMGRE_VertexList_CameraToViewPlane_wN(clipped,count,cam.perspectPlane.Dis);YMGRE_VertexList_ViewPlaneToWindows_wN(clipped,count,&cam);}

  for(uint16 k=1;k+1<count;k++){GRE_Index triangle[3]={0,k,k+1};polygon.index=triangle;YMGRE_TriangleRaster_FillVertexLit_wN(rasterVertices,&polygon,material,&cam);}

 }

 return 1;
}
