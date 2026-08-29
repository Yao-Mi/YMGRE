#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_HeightMap.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Rendering_Pipeline.h"
#include <stdio.h>

#define W 300
#define H 300

static GRE_Material material(void){GRE_Material m=YMGRE_Creat_Material("height_white");m->width=m->height=1;m->pixel=GRE_ImageBuff_Malloc(sizeof(GRErgb24));m->pixel[0]=(GRErgb24){220,225,235};return m;}
static GRE_Camera4d camera(int id,int kind){float32 f=kind==2?36.0f:58.0f;GRE_Camera4d c=YMGRE_Creat_Camera(id,W,H,f,f,f,f);YMGRE_Camera_Frustum_Init(c,1,600);gre_fvector4d e=kind==2?(gre_fvector4d){0,55,-70,1}:(gre_fvector4d){0,120,160,1},t={0,0,160,1};YMGRE_UVNCamera_PositionInit(c,&e,&t,NULL,0);return c;}
static void lights(gre_list* ls){GRE_Light4d a=YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){180,190,210},.42f);GRE_Light4d p=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,220,180},2.2f);p->pos=(gre_fvector4d){-120,150,80,1};p->proper.kc1=.0008f;YMGRE_List_Append(ls,sizeof(gre_light4d),a);YMGRE_List_Append(ls,sizeof(gre_light4d),p);}
static GRE_Object4d plane(int displaced){GRE_Object4d o=YMGRE_MeshGener_RectPlane(220,170,16,16,(GRErgb24){220,225,235},"height_plane","height_white");if(displaced){GRErgb24 map[17*17];for(int y=0;y<17;y++)for(int x=0;x<17;x++){float32 u=(float32)x/16.0f;uint8 c=(uint8)(128.0f+110.0f*YMGRE_Sin(u*YMGRE_2Pai*2.0f));map[y*17+x]=(GRErgb24){c,c,c};}YMGRE_Object_ApplyHeightMap(o,map,17,17,10,.50f);}o->WorldCoordinate=(gre_fvector4d){0,0,160,1};YMGRE_Object_LocalToWorld_wN(o);o->renderMode=GRE_RenderMode_Pixel;o->mirrorKs=.15f;return o;}
static GRE_Object4d sphere(void){GRE_Object4d o=YMGRE_MeshGener_Sphere(82,192,256,(GRErgb24){220,225,235},"height_sphere","height_white");GRErgb24* map=GRE_ImageBuff_Malloc(128*128*sizeof(GRErgb24));if(map==NULL)return o;for(int y=0;y<128;y++)for(int x=0;x<128;x++){float32 v=(float32)y/127.0f;uint8 c=(uint8)(128.0f+110.0f*YMGRE_Sin(v*YMGRE_2Pai*10.0f));map[y*128+x]=(GRErgb24){c,c,c};}YMGRE_Object_ApplyHeightMap(o,map,128,128,12,.50f);GRE_ImageBuff_Free(map);o->WorldCoordinate=(gre_fvector4d){0,0,160,1};YMGRE_Object_LocalToWorld_wN(o);o->renderMode=GRE_RenderMode_Pixel;o->mirrorKs=.25f;return o;}
static GRE_Camera4d renderCase(int id,int kind){gre_list os={0},ls={0},ms={0};GRE_Object4d o=kind==0?plane(0):kind==1?plane(1):sphere();YMGRE_List_Append(&os,sizeof(gre_object4d),o);YMGRE_List_Append(&ms,sizeof(gre_material),material());lights(&ls);GRE_Camera4d c=camera(id,kind);GRE_RenderWorkspace w=YMGRE_Creat_RenderWorkspace();YMGRE_Camera_TanglePipline_wN(c,&ls,&os,&ms,w);YMGRE_Free_RenderWorkspace(w);YMGRE_List_Clear(&ls,YMGRE_Free_Light);YMGRE_List_Clear(&os,YMGRE_Free_Object);YMGRE_List_Clear(&ms,YMGRE_Free_Material);return c;}
int main(void){GRE_Camera4d c[3]={renderCase(0,0),renderCase(1,1),renderCase(2,2)};YMGRE_DemoView v[3];for(int i=0;i<3;i++)v[i]=(YMGRE_DemoView){YMGRE_Camera_GetRenderTarget(c[i]),(i%2)*W,(i/2)*H,W,H};int changed=0;for(int i=0;i<W*H;i++)if(YMGRE_Fabs(c[1]->img.zbuff[i]-c[0]->img.zbuff[i])>.01f)changed++;int pass=changed>100;printf("height map cases: changed_depth=%d: %s\n",changed,pass?"PASS":"FAIL");YMGRE_DemoHost_Show(W*2,H*2,v,3,60);for(int i=0;i<3;i++)YMGRE_Free_Camera(c[i]);return pass?0:1;}
