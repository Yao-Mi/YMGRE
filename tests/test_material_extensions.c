#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Camera.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Material.h"
#include "YMGRE_Color.h"
#include "YMGRE_TriangleRaster.h"
#include "YMCS_File_IO.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static void put32(unsigned char* p,unsigned v){for(int i=0;i<4;i++)p[i]=(v>>(8*i))&255;}
static void materialFiles(const char* root)
{
 char path[1024],map[1024];snprintf(map,sizeof(map),"%s/extensions_mrs.bmp",root);
 GRErgb24 parameter={0,200,120};YMGRE_Image_LoadTo_Bmp_File(map,&parameter,1,1);
 snprintf(path,sizeof(path),"%s/extensions.material",root);
 FILE* f=fopen(path,"w");assert(f);
 fputs("material card\n{\n cull_hardware none\n pbr_ior 1.5\n normal_strength 0.4\n pbr_parameters extensions_mrs.bmp\n}\nmaterial closed\n{\n cull_hardware clockwise\n specular_power 25\n pbr_ior 1.7\n}\n",f);fclose(f);
 gre_scence scene={0};YMGRE_ParseMaterialScript(&scene,path);
 GRE_Material card=YMGRE_Material_Find(&scene.MaterialList,"card"),closed=YMGRE_Material_Find(&scene.MaterialList,"closed");
 assert(card&&closed&&card->doubleSided&&!closed->doubleSided);
#if YMGRE_ENABLE_PBR
 assert(card->advanced&&card->advanced->pbrIOR==1.5f&&card->advanced->pbrNormalStrength==.4f);
 assert(card->advanced->pbrEnabled&&card->advanced->pbrParameters&&card->advanced->pbrParameters[0].G==200);
 assert(closed->advanced->pbrIOR==1.7f&&closed->advanced->pbrNormalStrength==1);
#endif
 YMGRE_List_Clear(&scene.MaterialList,YMGRE_Free_Material);remove(path);remove(map);
#if YMGRE_ENABLE_TRANSPARENCY
 GRE_Material m=YMGRE_Creat_Material("opacity");assert(m);
 uint8 odd[]={0,40,80,120,160,200,220,240,255};
 assert(YMGRE_Material_SetOpacity(m,odd,3,3,1));assert(m->advanced->opacityPixel!=odd);
 assert(!memcmp(odd,m->advanced->opacityPixel,sizeof(odd)));
#if YMGRE_ENABLE_OPACITY_MIPMAP
 assert(m->advanced->opacityMipCount==3);
 assert(m->advanced->opacityMipPixels[1][0]==80);
 assert(m->advanced->opacityMipPixels[1][3]==255);
 assert(m->advanced->opacityMipPixels[2][0]==176);
#endif
 snprintf(path,sizeof(path),"%s/gray8.bmp",root);
 unsigned char head[54]={0};head[0]='B';head[1]='M';put32(head+2,1086);put32(head+10,1078);put32(head+14,40);put32(head+18,3);put32(head+22,2);head[26]=1;head[28]=8;put32(head+46,256);
 f=fopen(path,"wb");assert(f);fwrite(head,1,54,f);
 for(int i=0;i<256;i++){unsigned char entry[]={i,i,i,0};fwrite(entry,1,4,f);}
 unsigned char rows[]={200,210,220,0,10,20,30,0};fwrite(rows,1,8,f);fclose(f);
 assert(YMGRE_Material_LoadOpacityBMP(m,path,0));unsigned char expected[]={10,20,30,200,210,220};assert(!memcmp(m->advanced->opacityPixel,expected,6));
 f=fopen(path,"wb");fwrite(head,1,54,f);fclose(f);assert(!YMGRE_Material_LoadOpacityBMP(m,path,1));assert(!memcmp(m->advanced->opacityPixel,expected,6));
 remove(path);YMGRE_Free_Material(m);
#endif
}
static void colors(void)
{
#if YMGRE_ENABLE_LINEAR_COLOR
 for(int i=0;i<256;i++){float x=i/255.f,ref=x<=.04045f?x/12.92f:powf((x+.055f)/1.055f,2.4f);assert(fabsf(YMGRE_Color_Decode(i)-ref)<2e-7f);}
 int maxError=0;
 for(int i=0;i<=1000000;i++){
  float x=i*64.f/1000000;float v=fminf(1,x*(2.51f*x+.03f)/(x*(2.43f*x+.59f)+.14f));v=v<=.0031308f?12.92f*v:1.055f*powf(v,1.f/2.4f)-.055f;
  int error=abs(YMGRE_Color_Output(x)-(int)(255*v+.5f));if(error>maxError)maxError=error;
 }
 assert(maxError<=1);assert(YMGRE_Color_Output(NAN)==0);assert(YMGRE_Color_Output(INFINITY)==255);
 GRE_Camera4d c=YMGRE_Creat_Camera(1,16,16,45,45,45,45);GRE_RenderWorkspace w=YMGRE_Creat_RenderWorkspace();c->linearColorEnabled=1;
 assert(YMGRE_Color_BeginFrame(c,w,(GRErgb24){0,0,0}));float *first=w->linearColor;
 assert(YMGRE_Color_BeginFrame(c,w,(GRErgb24){0,0,0})&&first==w->linearColor);
 c->linearColor[0]=2;c->exposure=.5f;YMGRE_Color_EndFrame(c);assert(GRE_FramePixel_To_RGB24(c->img.data[0]).R>=YMGRE_Color_Output(1)-7);
 gre_render_workspace fixed={0};assert(!YMGRE_Color_BeginFrame(c,&fixed,(GRErgb24){0,0,0}));
 float buffer[16*16*3];YMGRE_RenderWorkspace_BindMaterialBuffers(&fixed,NULL,0,buffer,16*16*3);assert(YMGRE_Color_BeginFrame(c,&fixed,(GRErgb24){0,0,0}));assert(c->linearColor==buffer);
 YMGRE_Free_RenderWorkspace(w);YMGRE_Free_Camera(c);
#endif
}
static void raster(void)
{
#if YMGRE_ENABLE_PBR
 GRE_Camera4d c=YMGRE_Creat_Camera(1,32,32,45,45,45,45);YMGRE_Camera_Frustum_Init(c,.025f,10);c->pbrEnabled=1;
 GRE_Material m=YMGRE_Creat_Material("raster");GRE_MaterialAdvanced a=YMGRE_Material_EnsureAdvanced(m);a->pbrEnabled=1;
 GRErgb24 color={255,255,255},mrs={0,255,0};m->pixel=&color;m->width=m->height=1;a->pbrParameters=&mrs;a->pbrWidth=a->pbrHeight=1;
 gre_light4d light={0};light.type=GRE_GlobalLight;light.proper.lightcolor=color;light.proper.strength=1;
 gre_listnode node={sizeof(light),&light,NULL};gre_list lights={&node,1};gre_fvector4d lp={0};
 GRE_Index index[]={0,1,2};gre_polygon4d poly={0};poly.index=index;poly.num=3;
 gre_vertex4d_wN vertices[3]={0};for(int j=0;j<3;j++){vertices[j].normal.z=-1;vertices[j].color=color;vertices[j].base.pos.z=2;}
 /* Optimized scanline bounds must preserve the original barycentric coverage. */
 unsigned state=12345;unsigned char mask[32*32];
 for(int n=0;n<300;n++){
  for(int j=0;j<3;j++){state=state*1664525+1013904223;vertices[j].base.pos.x=(state%30000)/1000.f;state=state*1664525+1013904223;vertices[j].base.pos.y=(state%30000)/1000.f;}
  YMGRE_CameraImage_Init(c,(GRErgb24){0,0,0});m->unlit=0;YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);
  for(int i=0;i<32*32;i++)mask[i]=c->img.zbuff[i]<3;
  YMGRE_CameraImage_Init(c,(GRErgb24){0,0,0});m->unlit=1;YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);
  for(int i=0;i<32*32;i++)assert(mask[i]==(c->img.zbuff[i]<3));
 }
#if YMGRE_ENABLE_TRANSPARENCY
 vertices[0].base.pos=(gre_fvector4d){1,1,2,1};vertices[1].base.pos=(gre_fvector4d){30,1,2,1};vertices[2].base.pos=(gre_fvector4d){1,30,2,1};
 uint8 alpha=128;assert(YMGRE_Material_SetOpacity(m,&alpha,1,1,1));m->unlit=0;
 YMGRE_CameraImage_Init(c,(GRErgb24){0,0,0});c->opacityPass=1;YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);assert(c->img.zbuff[4*32+4]>2);
 c->opacityPass=2;YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);assert(c->img.zbuff[4*32+4]>2);GRErgb24 p=GRE_FramePixel_To_RGB24(c->img.data[4*32+4]);assert(p.R>=123&&p.R<=132);
#if YMGRE_ENABLE_LINEAR_COLOR
 GRE_RenderWorkspace workspace=YMGRE_Creat_RenderWorkspace();c->linearColorEnabled=1;
 assert(YMGRE_Color_BeginFrame(c,workspace,(GRErgb24){0,0,0}));
 YMGRE_CameraImage_Init(c,(GRErgb24){0,0,0});YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);
 assert(fabsf(c->linearColor[(4*32+4)*3]-128/255.f)<1e-6f);assert(c->img.zbuff[4*32+4]>2);
 c->linearColorEnabled=0;c->linearColor=NULL;YMGRE_Free_RenderWorkspace(workspace);
#endif
 c->img.zbuff[4*32+4]=1;GRE_FramePixel ref=c->img.data[4*32+4];YMGRE_TriangleRaster_Fill_wN(vertices,&poly,m,&lights,&lp,NULL,0,c);assert(!memcmp(&ref,&c->img.data[4*32+4],sizeof(ref)));
#endif
 m->pixel=NULL;a->pbrParameters=NULL;YMGRE_Free_Material(m);YMGRE_Free_Camera(c);
#endif
}
#if YMGRE_ENABLE_RASTER_DISPATCH
static void reverseJobs(void* user,GRE_RasterJob job,void* context,uint32 count)
{
 (*(int*)user)++;
 while(count)job(context,--count);
}
static void parallelPipeline(void)
{
 enum {W=47,H=35,N=W*H};
 GRE_Camera4d cam=YMGRE_Creat_Camera(0,W,H,45,45,45,45);
 GRE_RenderWorkspace ws=YMGRE_Creat_RenderWorkspace();
 YMGRE_Camera_Frustum_Init(cam,.1f,30);
 memset(&cam->move.TMat,0,sizeof(cam->move.TMat));for(int i=0;i<4;i++)cam->move.TMat.val[i][i]=1;
 cam->pbrEnabled=1;
 gre_list objects={0},materials={0},lights={0};
 GRE_Material mats[3];GRE_Object4d objs[3];
 char* names[]={"opaqueA","opaqueB","masked"};
 for(int k=0;k<3;k++){
  GRE_Material m=mats[k]=YMGRE_Creat_Material(names[k]);m->doubleSided=1;
  YMGRE_Material_EnsureAdvanced(m)->pbrEnabled=1;
  m->diffuse=(GRErgb24){50+k*80,190-k*30,120};
  if(k==2){uint8 alpha[]={0,64,128,255};assert(YMGRE_Material_SetOpacity(m,alpha,2,2,1));}
  YMGRE_List_Append(&materials,sizeof(*m),m);
  GRE_Object4d o=objs[k]=YMGRE_Creat_Object(3,1,names[k],names[k]);
  o->isVisible=1;o->renderMode=GRE_RenderMode_Pixel;o->boundType=GRE_Bounding_Sphere_R;o->BoundingSphereR=10;
  float z=k==2?2:3;o->WorldCoordinate=(gre_fvector4d){0,0,z,1};
  o->pointList[0]=(gre_vertex4d){{-4,-3,z,1},0,0};
  o->pointList[1]=(gre_vertex4d){{0,4,z,1},.5f,1};
  o->pointList[2]=(gre_vertex4d){{4,-3,z,1},1,0};
  o->polygonList[0].num=3;o->polygonList[0].index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));
  for(int i=0;i<3;i++)o->polygonList[0].index[i]=i;
  o->polygonList[0].pN=(gre_fvector4d){0,0,-1,0};
  assert(YMGRE_Object_EnableVertexAttributes(o));
  for(int i=0;i<3;i++){o->pointList_wN[i].normal=(gre_fvector4d){0,0,-1,0};o->pointList_wN[i].color=(GRErgb24){255,255,255};}
  YMGRE_List_Append(&objects,sizeof(*o),o);
 }
 GRE_Light4d light=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},1);
 YMGRE_List_Append(&lights,sizeof(*light),light);
 GRE_FramePixel reference[N];float depth[N];int dispatches=0;
 for(int mode=0;mode<4;mode++){
#if YMGRE_ENABLE_LINEAR_COLOR
  cam->linearColorEnabled=mode&1;
#endif
  mats[0]->unlit=mode>=2; /* Mixed modes must take the original serial path. */
  YMGRE_RenderWorkspace_SetRasterDispatcher(ws,NULL,NULL);
  YMGRE_Camera_TanglePipline_wN(cam,&lights,&objects,&materials,ws);assert(!ws->materialStatus);
  memcpy(reference,cam->img.data,sizeof(reference));memcpy(depth,cam->img.zbuff,sizeof(depth));
  int visible=0;for(int i=0;i<N;i++)if(depth[i]<10)visible++;assert(visible>100);
  YMGRE_RenderWorkspace_SetRasterDispatcher(ws,reverseJobs,&dispatches);dispatches=0;
  YMGRE_Camera_TanglePipline_wN(cam,&lights,&objects,&materials,ws);
  assert(!ws->materialStatus&&dispatches==(mode<2?3:0));
  assert(!memcmp(reference,cam->img.data,sizeof(reference)));assert(!memcmp(depth,cam->img.zbuff,sizeof(depth)));
 }
 /* A borrowed fixed workspace may not silently allocate packet storage. */
 ws->ownsMemory=0;size_t capacity=ws->transparentCapacity;ws->transparentCapacity=0;mats[0]->unlit=0;
 YMGRE_Camera_TanglePipline_wN(cam,&lights,&objects,&materials,ws);assert(ws->materialStatus==-1);
 ws->transparentCapacity=capacity;ws->ownsMemory=1;
 YMGRE_List_Clear(&objects,YMGRE_Free_Object);YMGRE_List_Clear(&materials,YMGRE_Free_Material);
 YMGRE_List_Clear(&lights,YMGRE_Free_Light);YMGRE_Free_RenderWorkspace(ws);YMGRE_Free_Camera(cam);
}
#endif
int main(int argc,char** argv)
{
 assert(argc==2);materialFiles(argv[1]);colors();raster();
#if YMGRE_ENABLE_RASTER_DISPATCH
 parallelPipeline();
#endif
 puts("PASS: material flags, gray8 BMP, opacity ownership/mips, LUTs, workspace, raster coverage/depth, band dispatch");return 0;
}
