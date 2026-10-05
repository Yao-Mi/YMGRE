#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Light.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_TriangleRaster.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr,"portable line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
static unsigned seed=20261005;
static float randomValue(float lo,float hi)
{
 seed=seed*1664525u+1013904223u;
 return lo+(hi-lo)*(float)(seed>>8)/16777216.0f;
}
static void noFree(void* p) { (void)p; }
static void clearAndPower(void)
{
 GRE_FramePixel pixels[1026];float depth[1026];gre_camera4d c={0};
 GRE_FramePixel guard=GRE_FramePixel_From_RGB24((GRErgb24){253,137,219});
 for(int w=1;w<=31;w+=2)for(int h=1;h<=31;h+=2)for(int external=0;external<2;external++) {
  int n=w*h;gre_render_target t={(uint16)w,(uint16)h,pixels+1,depth+1};
  c.img=t;c.target=external?&t:NULL;c.frustum.Zfar=123.75f;
  pixels[0]=pixels[n+1]=guard;depth[0]=depth[n+1]=-17;
  GRErgb24 color={(uint8)(w*7),(uint8)(h*5),91};
  YMGRE_CameraImage_Init(&c,color);
  for(int i=1;i<=n;i++) { CHECK(depth[i]==c.frustum.Zfar);CHECK(GRE_FramePixel_Equals(pixels[i],GRE_FramePixel_From_RGB24(color))); }
  CHECK(depth[0]==-17&&depth[n+1]==-17);CHECK(GRE_FramePixel_Equals(pixels[0],guard)&&GRE_FramePixel_Equals(pixels[n+1],guard));
 }
 for(int n=0;n<=255;n++)for(int i=0;i<=1000;i++) {
  float x=i/1000.0f,ref=powf(x,(float)n),got=YMGRE_Light_IntegerPower(x,(uint8)n);
  CHECK(isfinite(got));CHECK(fabsf(got-ref)<=8e-6f*ref+1e-7f);
 }
}
static void clipProperties(void)
{
 gre_camera4d c={0};c.frustum.Znear=1;c.frustum.Zfar=10;
 c.perspectPlane.kl=c.perspectPlane.kd=-1;c.perspectPlane.kr=c.perspectPlane.ku=1;
 gre_vertex4d_wN in[3]={0},out[YMGRE_FRUSTUM_CLIP_VERTEX_MAX+2];
 for(int i=0;i<3;i++) {
  in[i].base=(gre_vertex4d){{(float)(i-1),i==1?1.0f:-1.0f,3,1},i*.4f,i*.3f};
  in[i].normal=(gre_fvector4d){0,0,-1,0};in[i].tangent=(gre_fvector4d){1,0,0,0};in[i].tangentW=1;
  in[i].color=(GRErgb24){40,80,120};in[i].vertexLighting=(GRErgb24){25,55,85};in[i].vertexSpecular=(GRErgb24){11,22,33};
 }
 CHECK(YMGRE_Polygon_FrustumClip_wN(in,3,out,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&c)==3);
 for(int i=0;i<3;i++) { CHECK(!memcmp(&in[i].base,&out[i].base,sizeof(in[i].base)));CHECK(out[i].tangentW==1); }
 for(int plane=0;plane<6;plane++) {
  gre_vertex4d_wN rejected[3];memcpy(rejected,in,sizeof(in));
  for(int i=0;i<3;i++) {
   if(plane<2)rejected[i].base.pos.z=plane?11:.5f;
   else if(plane<4)rejected[i].base.pos.x=plane==2?-12:12;
   else rejected[i].base.pos.y=plane==4?-12:12;
  }
  CHECK(!YMGRE_Polygon_FrustumClip_wN(rejected,3,out,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&c));
 }
 for(int trial=0;trial<6000;trial++) {
  for(int i=0;i<3;i++)in[i].base.pos=(gre_fvector4d){randomValue(-15,15),randomValue(-15,15),randomValue(-2,15),1};
  uint16 n=YMGRE_Polygon_FrustumClip_wN(in,3,out+1,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&c);
  for(int i=1;i<=n;i++) {
   gre_vertex4d_wN* v=out+i;float x=v->base.pos.x,y=v->base.pos.y,z=v->base.pos.z;
   CHECK(isfinite(x)&&isfinite(y)&&isfinite(z));CHECK(z>=1-1e-4f&&z<=10+1e-4f&&fabsf(x)<=z+1e-4f&&fabsf(y)<=z+1e-4f);
   CHECK(v->vertexLighting.R==25&&v->vertexLighting.G==55&&v->vertexLighting.B==85);
   CHECK(v->vertexSpecular.R==11&&v->color.B==120&&v->normal.z==-1&&v->tangent.x==1&&v->tangentW==1);
  }
  gre_vertex4d_wN alias[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];memcpy(alias,in,sizeof(in));
  CHECK(YMGRE_Polygon_FrustumClip_wN(alias,3,alias,YMGRE_FRUSTUM_CLIP_VERTEX_MAX,&c)==n);
  for(int i=0;i<n;i++)CHECK(!memcmp(&alias[i].base,&out[i+1].base,sizeof(alias[i].base)));
  for(int capacity=0;capacity<=YMGRE_FRUSTUM_CLIP_VERTEX_MAX;capacity++) {
   gre_vertex4d_wN limited[YMGRE_FRUSTUM_CLIP_VERTEX_MAX+2];memset(limited,0x5a,sizeof(limited));
   unsigned char before[sizeof(limited[0])];memcpy(before,limited,sizeof(before));
   uint16 m=YMGRE_Polygon_FrustumClip_wN(in,3,limited+1,(uint16)capacity,&c);
   CHECK(m==(capacity<3?0:(n<capacity?n:capacity)));
   CHECK(!memcmp(before,limited,sizeof(before))&&!memcmp(before,limited+1+capacity,sizeof(before)));
  }
 }
}
static void floatWires(void)
{
 enum{W=47,H=35,N=W*H};GRE_FramePixel a[N],b[N];float da[N],db[N];
 GRErgb24 white={255,255,255};
 for(int k=0;k<600;k++) {
  float x0=randomValue(-20,65),y0=randomValue(-20,55),x1=randomValue(-20,65),y1=randomValue(-20,55);
  float z0=randomValue(1,30),z1=randomValue(1,30);
  if(k<40) { x0=x1=10.25f;y0=y1=12.25f; }
  for(int test=0;test<2;test++) {
   memset(a,0,sizeof(a));memset(b,0,sizeof(b));for(int i=0;i<N;i++)da[i]=db[i]=100;
   YMGRE_Img_LineDepthFloat(a,da,W,H,x0,y0,z0,x1,y1,z1,white,(uint8)test);
   YMGRE_Img_LineDepthFloat(b,db,W,H,x1,y1,z1,x0,y0,z0,white,(uint8)test);
   CHECK(!memcmp(a,b,sizeof(a))&&!memcmp(da,db,sizeof(da)));
   float dx=x1-x0,dy=y1-y0;
   for(int i=0;i<N;i++)if(GRE_FramePixel_Equals(a[i],GRE_FramePixel_From_RGB24(white))) {
    int x=i%W,y=i/W;float t=fabsf(dy)>fabsf(dx)?(y-y0)/dy:(dx!=0?(x-x0)/dx:.5f);
    t=fmaxf(0,fminf(1,t));float minor=fabsf(dy)>fabsf(dx)?x0+dx*t:y0+dy*t;
    CHECK(fabsf((fabsf(dy)>fabsf(dx)?x:y)-minor)<=.5001f);
    if(test)CHECK(fabsf(da[i]-1/(1/z0+(1/z1-1/z0)*t))<2e-4f);else CHECK(da[i]==100);
   }
  }
 }
 memset(a,0,sizeof(a));for(int i=0;i<N;i++)da[i]=100;
 YMGRE_Img_LineDepthFloat(a,da,W,H,NAN,0,1,20,20,2,white,1);
 YMGRE_Img_LineDepthFloat(a,da,W,H,0,0,INFINITY,20,20,2,white,1);
 for(int i=0;i<N;i++)CHECK(da[i]==100);
}
/* Duplicate shared vertices to force the original per-triangle lighting path.
   The extra unused vertices keep visible*3 below pointNum, without a test-only API. */
static GRE_Object4d mesh(int reference)
{
 GRE_Object4d o=YMGRE_Creat_Object(reference?7:4,2,"cache","mat");
 o->isVisible=1;o->renderMode=GRE_RenderMode_Vertex;o->boundType=GRE_Bounding_Sphere_R;o->BoundingSphereR=20;
 o->WorldCoordinate=(gre_fvector4d){0,0,3,1};o->mirrorKs=.5f;
 const gre_vertex4d points[4]={{{-3,-2,3,1},0,0},{{-3,2,3,1},0,1},{{3,2,3,1},1,1},{{3,-2,3,1},1,0}};
 int indices[6]={0,1,2,0,2,3};
 for(int i=0;i<o->pointNum;i++){
  o->pointList[i]=points[reference?(i<6?indices[i]:0):i];
  o->pointList[i].pos.z+=o->pointList[i].pos.x*.4f;
 }
 for(int p=0;p<2;p++) {
  o->polygonList[p].num=3;o->polygonList[p].index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
  for(int j=0;j<3;j++)o->polygonList[p].index[j]=(GRE_Index)(reference?p*3+j:indices[p*3+j]);
  o->polygonList[p].pN=(gre_fvector4d){0,0,-1,0};
 }
 CHECK(YMGRE_Object_EnableVertexAttributes(o));
 for(int i=0;i<o->pointNum;i++) { o->pointList_wN[i].normal=(gre_fvector4d){.05f*o->pointList[i].pos.x,0,-1,0};o->pointList_wN[i].color=(GRErgb24){190,210,230}; }
 return o;
}
static void sharedLighting(void)
{
 enum{W=47,H=35,N=W*H};GRE_Camera4d c=YMGRE_Creat_Camera(0,W,H,45,45,45,45);
 GRE_RenderWorkspace ws=YMGRE_Creat_RenderWorkspace();YMGRE_Camera_Frustum_Init(c,1,30);
 memset(&c->move.TMat,0,sizeof(c->move.TMat));for(int i=0;i<4;i++)c->move.TMat.val[i][i]=1;
 GRE_Material m=YMGRE_Creat_Material("mat");m->ambient=(GRErgb24){60,40,30};m->diffuse=(GRErgb24){160,200,130};
 GRE_Object4d cached=mesh(0),reference=mesh(1);gre_list lights={0},materials={0},objects={0};
 YMGRE_List_Append(&materials,sizeof(*m),m);YMGRE_List_Append(&objects,sizeof(*cached),cached);
 GRE_Light4d light[3];gre_light4d saved[3];gre_fvector4d positions[3];
 for(int i=0;i<3;i++) {
  light[i]=YMGRE_Creat_Light(i,i==0?GRE_GlobalLight:i==1?GRE_PointLight:GRE_SpotLight,(GRErgb24){190,220,250},.5f);
  light[i]->proper.pos_=(gre_fvector4d){i==1?-2:2,1,0,1};light[i]->proper.kc0=1;light[i]->proper.kc1=.01f;light[i]->proper.kc2=0;
  light[i]->proper.spot.direct=(gre_fvector4d){0,0,1,0};light[i]->proper.spot.cs_outer_angle=0;light[i]->proper.spot.cs_div_=1;
  light[i]->pos=light[i]->proper.pos_;positions[i]=light[i]->proper.pos_;saved[i]=*light[i];YMGRE_List_Append(&lights,sizeof(*light[i]),light[i]);
 }
 gre_vertex4d_wN batched[4],single[4];memcpy(batched,cached->pointList_wN,sizeof(batched));memcpy(single,batched,sizeof(single));
 YMGRE_TriangleRaster_ComputeVertexLighting_wN(batched,0,cached->polygonList,m,&lights,NULL,&c->move.TMat,.5f);
 YMGRE_TriangleRaster_ComputeVertexLighting_wN(batched,4,cached->polygonList,m,&lights,positions,&c->move.TMat,.5f);
 for(int i=0;i<4;i++) {
  YMGRE_TriangleRaster_ComputeVertexLighting_wN(single+i,1,cached->polygonList,m,&lights,positions,&c->move.TMat,.5f);
  CHECK(!memcmp(&batched[i].vertexLighting,&single[i].vertexLighting,sizeof(GRErgb24)));
  CHECK(!memcmp(&batched[i].vertexSpecular,&single[i].vertexSpecular,sizeof(GRErgb24)));
 }
 GRE_FramePixel frame[N];float depth[N];gre_vertex4d_wN source[4];memcpy(source,cached->pointList_wN,sizeof(source));
 for(int mode=0;mode<24;mode++) {
  m->doubleSided=(mode%3)==1;m->unlit=(mode%3)==2;
  c->frustum.Znear=mode<12?1:mode<18?3.1f:8; /* Visible, clipped and fully rejected. */
  float angle=mode*.15f;
  c->move.TMat.val[0][0]=c->move.TMat.val[1][1]=cosf(angle);
  c->move.TMat.val[0][1]=-sinf(angle);c->move.TMat.val[1][0]=sinf(angle);
  c->move.TMat.val[0][3]=mode*.05f;
  for(int p=0;p<2;p++)cached->polygonList[p].pN.z=reference->polygonList[p].pN.z=mode%2?1:-1;
  objects.listhead->data=reference;YMGRE_Camera_TanglePipline_wN(c,&lights,&objects,&materials,ws);
  memcpy(frame,c->img.data,sizeof(frame));memcpy(depth,c->img.zbuff,sizeof(depth));
  if(mode==0) { int visible=0;for(int i=0;i<N;i++)visible+=depth[i]<30;CHECK(visible>100); }
  objects.listhead->data=cached;YMGRE_Camera_TanglePipline_wN(c,&lights,&objects,&materials,ws);
  CHECK(!memcmp(frame,c->img.data,sizeof(frame))&&!memcmp(depth,c->img.zbuff,sizeof(depth)));
  CHECK(!memcmp(source,cached->pointList_wN,sizeof(source)));
 }
 for(int i=0;i<3;i++)CHECK(!memcmp(saved+i,light[i],sizeof(saved[i])));
 YMGRE_List_Clear(&objects,noFree);YMGRE_List_Clear(&materials,YMGRE_Free_Material);YMGRE_List_Clear(&lights,YMGRE_Free_Light);
 YMGRE_Free_Object(cached);YMGRE_Free_Object(reference);YMGRE_Free_RenderWorkspace(ws);YMGRE_Free_Camera(c);
}
static void projectionAndTopology(void)
{
 GRE_Object4d object=YMGRE_Creat_Object(4,2,"compact","mat");
 for(int i=0;i<4;i++)object->pointList[i].pos=(gre_fvector4d){i*.25f,i*.5f,2+i,1};
 const GRE_Index original[6]={0,1,2,0,2,3};
 for(int pi=0;pi<2;pi++){
  GRE_Polygon4d p=&object->polygonList[pi];p->num=3;p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
  memcpy(p->index,original+pi*3,3*sizeof(GRE_Index));
  p->pN=(gre_fvector4d){0,0,-1,0};
 }
 CHECK(YMGRE_Object_EnableVertexAttributes(object));
 gre_fmat4x4 matrix={0};for(int i=0;i<4;i++)matrix.val[i][i]=1;
 matrix.val[0][0]=2;matrix.val[1][1]=3;matrix.val[2][2]=.5f;
 matrix.val[0][3]=1;matrix.val[1][3]=-2;
 const uint8 active[4]={1,0,1,0};
 for(int form=0;form<2;form++){
  if(form){matrix.val[0][1]=.4f;matrix.val[1][0]=-.4f;}
  gre_vertex4d_wN full[4],masked[4];
  memset(full,0x5a,sizeof(full));memset(masked,0x5a,sizeof(masked));
  YMGRE_Object_WorldToCameraTo_wN(object,&matrix,full);
  YMGRE_Object_WorldToCameraMaskedTo(object,&matrix,masked,1,active);
  for(int i=0;i<4;i++){
   if(active[i]){
    CHECK(!memcmp(&full[i].base,&masked[i].base,sizeof(full[i].base)));
    CHECK(!memcmp(&full[i].normal,&masked[i].normal,sizeof(full[i].normal)));
    CHECK(!memcmp(&full[i].tangent,&masked[i].tangent,sizeof(full[i].tangent)));
    CHECK(full[i].tangentW==masked[i].tangentW&&full[i].lightmapU==masked[i].lightmapU&&full[i].lightmapV==masked[i].lightmapV);
    CHECK(!memcmp(&full[i].color,&masked[i].color,sizeof(full[i].color)));
   }
   else{const unsigned char* bytes=(const unsigned char*)&masked[i];for(size_t j=0;j<sizeof(masked[i]);j++)CHECK(bytes[j]==0x5a);}
  }
 }
 GRE_RenderWorkspace ws=YMGRE_Creat_RenderWorkspace();
 CHECK(YMGRE_RenderWorkspace_EnableProjectionCache(ws,4));
 CHECK(ws->projectedMax>=4&&ws->projectedPoints&&ws->clipCodes);
 CHECK(YMGRE_RenderWorkspace_EnableProjectionCache(ws,8));
 CHECK(ws->projectedMax>=8);
 YMGRE_Free_RenderWorkspace(ws);
 CHECK(YMGRE_Object_CompactTopology(object));
 CHECK(object->topologyIndexCount==6);
 CHECK(object->polygonList[0].index==object->topologyStorage);
 CHECK(object->polygonList[1].index==object->topologyStorage+3);
 CHECK(!memcmp(object->topologyStorage,original,sizeof(original)));
 CHECK(!YMGRE_Object_CompactTopology(object));
 YMGRE_Free_Object(object);
}
int main(void)
{
 clearAndPower();clipProperties();floatWires();sharedLighting();projectionAndTopology();
 puts("PASS: clear guards, integer powers, 6000 clips/capacities, 600 reversible float edges, shared lighting, projection and topology");return 0;
}
