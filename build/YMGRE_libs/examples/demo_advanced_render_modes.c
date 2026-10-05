#include "YMGRE_Material.h"
#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"

#define COUNT 3
#define PANEL_W 300
#define PANEL_H 360
typedef struct { gre_list objects, lights, materials; GRE_Camera4d camera; GRE_RenderWorkspace workspace; } Stage;

static GRE_Object4d makeObject(int mode)
{
	GRE_Object4d o;
	if (mode < 2) o = YMGRE_MeshGener_Cube(105, (GRErgb24){ 215, 215, 215 }, "render_mode_cube", "mode_mat");
	else
	{
		o = YMGRE_Creat_Object(4, 2, "render_mode_pixel", "mode_mat");
		o->pointList[0] = (gre_vertex4d){{-125,-105,180,1},0,1}; o->pointList[1]=(gre_vertex4d){{125,-105,180,1},1,1};
		o->pointList[2]=(gre_vertex4d){{125,105,180,1},1,0}; o->pointList[3]=(gre_vertex4d){{-125,105,180,1},0,0};
		GRE_Index idx[6]={0,3,2,0,2,1}; for(int i=0;i<2;i++){GRE_Polygon4d p=&o->polygonList[i];p->num=3;p->index=GRE_PolyIndex_Malloc(3*sizeof(GRE_Index));for(int j=0;j<3;j++)p->index[j]=idx[i*3+j];p->pN=(gre_fvector4d){0,0,-1,0};p->planeColor=(GRErgb24){215,215,215};}
		o->BoundingSphereR=165;o->boundType=GRE_Bounding_Sphere_R;
	}
	o->WorldCoordinate=(gre_fvector4d){0,0,180,1}; YMGRE_Object_LocalToWorld(o); YMGRE_Object_GenerateVertexAttributes(o); o->renderMode=mode==0?GRE_RenderMode_Face:(mode==1?GRE_RenderMode_Vertex:GRE_RenderMode_Pixel); return o;
}

static GRE_Material makeMaterial(int mode)
{
	GRE_Material m=YMGRE_Creat_Material("mode_mat");m->ambient=(GRErgb24){90,90,90};m->diffuse=(GRErgb24){215,215,215};m->width=m->height=1;m->pixel=GRE_ImageBuff_Malloc(sizeof(GRErgb24));m->pixel[0]=m->diffuse;
	if(mode==2){YMGRE_Material_EnsureAdvanced(m);m->advanced->normalWidth=m->advanced->normalHeight=16;m->advanced->normalPixel=GRE_ImageBuff_Malloc(256*sizeof(GRErgb24));for(int y=0;y<16;y++)for(int x=0;x<16;x++){float32 nx=(((x/4)&1)?-.38f:.38f),ny=(((y/4)&1)?-.28f:.28f);gre_fvector4d n={nx,ny,.88f,0};YMGRE_Fvector4d_Normalize(&n);m->advanced->normalPixel[y*16+x]=(GRErgb24){(uint8)((n.x*.5f+.5f)*255),(uint8)((n.y*.5f+.5f)*255),(uint8)((n.z*.5f+.5f)*255)};}}
	return m;
}

static void init(Stage* s,int mode)
{
	YMGRE_List_Append(&s->objects,sizeof(gre_object4d),makeObject(mode));YMGRE_List_Append(&s->materials,sizeof(gre_material),makeMaterial(mode));
	GRE_Light4d l=YMGRE_Creat_Light(0,GRE_PointLight,(GRErgb24){255,245,225},1.5f);l->pos=(gre_fvector4d){-70,100,45,1};l->proper.kc1=.001f;l->proper.shadowK=0;YMGRE_List_Append(&s->lights,sizeof(gre_light4d),l);
	s->camera=YMGRE_Creat_Camera(mode,PANEL_W,PANEL_H,42,42,40,40);YMGRE_Camera_Frustum_Init(s->camera,1,600);gre_fvector4d e={0,65,-95,1},t={0,0,180,1};YMGRE_UVNCamera_PositionInit(s->camera,&e,&t,NULL,0);s->workspace=YMGRE_Creat_RenderWorkspace();YMGRE_Camera_TanglePipline_wN(s->camera,&s->lights,&s->objects,&s->materials,s->workspace);
}
static void destroy(Stage*s){YMGRE_Free_RenderWorkspace(s->workspace);YMGRE_Free_Camera(s->camera);YMGRE_List_Clear(&s->lights,YMGRE_Free_Light);YMGRE_List_Clear(&s->objects,YMGRE_Free_Object);YMGRE_List_Clear(&s->materials,YMGRE_Free_Material);}
int main(void){Stage s[COUNT]={0};YMGRE_DemoView v[COUNT];for(int i=0;i<COUNT;i++){init(&s[i],i);v[i]=(YMGRE_DemoView){YMGRE_Camera_GetRenderTarget(s[i].camera),i*PANEL_W,0,PANEL_W,PANEL_H};}YMGRE_DemoHost_Show(COUNT*PANEL_W,PANEL_H,v,COUNT,60);for(int i=0;i<COUNT;i++)destroy(&s[i]);return 0;}
