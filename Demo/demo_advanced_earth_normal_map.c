#include "demo_host.h"
#include "YMGUI_Invalidate.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_HeightMap.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
#include "YMCS_File_IO.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>

#define STAGES 4
#define W 300
#define H 340
#define LAT 24
#define LON 48
#define HEIGHT_LAT 48
#define HEIGHT_LON 96
#define TEX_W 640
#define TEX_H 320

typedef struct { gre_list objects, lights, materials; GRE_Camera4d camera; GRE_RenderWorkspace workspace; } Stage;

static GRE_Object4d makeEarth(int stage)
{
	int latitude = stage == 3 ? HEIGHT_LAT : LAT;
	int longitude = stage == 3 ? HEIGHT_LON : LON;
	int nw = longitude + 1, nh = latitude + 1;
	GRE_Object4d o = YMGRE_Creat_Object(nw * nh, latitude * longitude * 2, "earth", "earth_material");
	for (int y = 0; y <= latitude; y++) for (int x = 0; x <= longitude; x++)
	{
		float32 v = (float32)y / latitude, u = (float32)x / longitude;
		float32 phi = YMGRE_Pai * v, theta = YMGRE_2Pai * u;
			o->pointList[y * nw + x] = (gre_vertex4d){{ 82.0f * YMGRE_Sin(phi) * YMGRE_Cos(theta),
			82.0f * YMGRE_Cos(phi), 82.0f * YMGRE_Sin(phi) * YMGRE_Sin(theta), 1 }, u, v };
	}
	int p = 0;
	for (int y = 0; y < latitude; y++) for (int x = 0; x < longitude; x++)
	{
		uint16 a = y * nw + x, b = a + 1, c = a + nw, d = c + 1;
		uint16 ix[6] = { a, d, c, a, b, d };
		for (int t = 0; t < 2; t++) { GRE_Polygon4d q = &o->polygonList[p++]; q->num = 3; q->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index)); for (int j = 0; j < 3; j++) q->index[j] = ix[t * 3 + j]; q->planeColor = (GRErgb24){255,255,255}; gre_fvector4d* cp = &o->pointList[ix[t * 3]].pos; q->pN = (gre_fvector4d){-cp->x, -cp->y, -cp->z, 0}; }
	}
	if (stage == 3)
	{
		GRErgb24* heightMap = NULL;
		uint16 heightWidth = 0, heightHeight = 0;
		YMGRE_Bmp_File_LoadTo_Image("Resource/地球高度贴图.bmp", &heightMap,
			&heightWidth, &heightHeight);
		// The longitude seam and both pole rows represent shared sphere points.
		// Make their sampled heights identical before displacing the mesh.
		for (uint16 y = 0; y < heightHeight; y++)
			heightMap[y * heightWidth + heightWidth - 1] = heightMap[y * heightWidth];
		for (uint16 x = 0; x < heightWidth; x++)
		{
			heightMap[x] = (GRErgb24){ 128, 128, 128 };
			heightMap[(heightHeight - 1) * heightWidth + x] = (GRErgb24){ 128, 128, 128 };
		}
		// Positive height is always pushed outward by the height-map helper.
		// Mid-gray is the zero plane: dark oceans recess and bright land rises.
		YMGRE_Object_ApplyHeightMap(o, heightMap, heightWidth, heightHeight, 6.0f, 0.50f);
		GRE_ImageBuff_Free(heightMap);
	}
	o->WorldCoordinate = (gre_fvector4d){0,0,220,1};
	YMGRE_Object_LocalToWorld(o);
	if (stage != 3) o->BoundingSphereR = 82;
	o->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(o); o->renderMode = GRE_RenderMode_Pixel; o->mirrorKs = .35f;
	for (int i = 0; i < o->pointNum; i++)
	{
		if (stage == 0)
			o->pointList_wN[i].normal = (gre_fvector4d){ 0, 0, -1, 0 };
		else
		{
			o->pointList_wN[i].normal.x = -o->pointList_wN[i].normal.x;
			o->pointList_wN[i].normal.y = -o->pointList_wN[i].normal.y;
			o->pointList_wN[i].normal.z = -o->pointList_wN[i].normal.z;
		}
		o->pointList_wN_[i] = o->pointList_wN[i];
	}
	return o;
}

static GRE_Material makeMaterial(int stage)
{
	GRE_Material m = YMGRE_Creat_Material("earth_material");
	m->ambient = stage == 3 ? (GRErgb24){5,5,5} : (GRErgb24){30,30,30};
	m->diffuse = stage == 3 ? (GRErgb24){100,100,100} : (GRErgb24){255,255,255};
	m->specular = stage == 3 ? (GRErgb24){12,18,30} : (GRErgb24){70,110,255};
	m->width = TEX_W; m->height = TEX_H; m->pixel = NULL;
	YMGRE_Bmp_File_LoadTo_Image("Resource/地球贴图.bmp", &m->pixel, &m->width, &m->height);
	if (stage == 3)
	{
		GRE_ImageBuff_Free(m->pixel);
		m->width = m->height = 1;
		m->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
		m->pixel[0] = (GRErgb24){ 255, 255, 255 };
	}
	m->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	m->advanced->normalWidth = TEX_W; m->advanced->normalHeight = TEX_H;
	if (stage >= 2)
		YMGRE_Bmp_File_LoadTo_Image("Resource/地球法线贴图.bmp", &m->advanced->normalPixel,
			&m->advanced->normalWidth, &m->advanced->normalHeight);
	else
	{
		m->advanced->normalWidth = m->advanced->normalHeight = 1;
		m->advanced->normalPixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
		m->advanced->normalPixel[0] = (GRErgb24){ 128, 128, 255 };
	}
	return m;
}

static void init(Stage* s, int stage)
{
	YMGRE_List_Append(&s->objects,sizeof(gre_object4d),makeEarth(stage)); YMGRE_List_Append(&s->materials,sizeof(gre_material),makeMaterial(stage));
	YMGRE_List_Append(&s->lights,sizeof(gre_light4d),YMGRE_Creat_Light(0,GRE_GlobalLight,(GRErgb24){255,255,255},.18f));
	GRE_Light4d l=YMGRE_Creat_Light(1,GRE_PointLight,(GRErgb24){255,245,220},2.0f); l->pos=(gre_fvector4d){-95,65,65,1}; l->proper.kc1=.0003f; l->proper.shadowK=0; YMGRE_List_Append(&s->lights,sizeof(gre_light4d),l);
	s->camera=YMGRE_Creat_Camera(stage,W,H,42,42,40,40); YMGRE_Camera_Frustum_Init(s->camera,1,600); gre_fvector4d e={0,0,0,1},t={0,0,220,1}; YMGRE_UVNCamera_PositionInit(s->camera,&e,&t,NULL,0); s->workspace=YMGRE_Creat_RenderWorkspace(); YMGRE_Camera_TanglePipline_wN(s->camera,&s->lights,&s->objects,&s->materials,s->workspace);
}
static void destroy(Stage* s){YMGRE_Free_RenderWorkspace(s->workspace);YMGRE_Free_Camera(s->camera);YMGRE_List_Clear(&s->lights,YMGRE_Free_Light);YMGRE_List_Clear(&s->objects,YMGRE_Free_Object);YMGRE_List_Clear(&s->materials,YMGRE_Free_Material);}

static void renderAll(Stage* stages, float32 lightY)
{
	for (int i = 0; i < STAGES; i++)
	{
		GRE_Light4d point = (GRE_Light4d)stages[i].lights.listhead->next->data;
		point->pos.y = lightY;
		YMGRE_Camera_TanglePipline_wN(stages[i].camera, &stages[i].lights,
			&stages[i].objects, &stages[i].materials, stages[i].workspace);
	}
}

static void showInteractive(Stage* stages, YMGRE_DemoView* views)
{
	YMGRE_DemoHost host;
	if (!YMGRE_DemoHost_Init(&host, STAGES * W, H, 40)) return;
	for (int i = 0; i < STAGES; i++) YMGRE_DemoHost_AddTarget(&host, views[i].target, views[i].x, views[i].y, views[i].width, views[i].height);
	while (YMGRE_DemoHost_Step(&host, 60))
	{
		int mouseX = 0, mouseY = 0;
		SDL_GetMouseState(&mouseX, &mouseY);
		int scale = 2;
		const char* scaleText = getenv("YMGRE_WINDOW_SCALE");
		if (scaleText != NULL && atoi(scaleText) > 0) scale = atoi(scaleText);
		float32 y = (float32)mouseY / scale;
		if (mouseY == 0 && host.context->point_y != 0) y = host.context->point_y;
		if (y < 0) y = 0; if (y > H) y = H;
		renderAll(stages, (0.5f - y / H) * 150.0f);
		YMGUI_Obj_Invalidate(host.context->root);
	}
	YMGRE_DemoHost_Destroy(&host);
}

static int verify(const Stage* s)
{
	int checked=0, normalChanged=0, seamChanged=0;
	for(int i=0;i<W*H;i++){if(s[1].camera->img.zbuff[i]>=s[1].camera->frustum.Zfar)continue; GRErgb24 a=GRE_FramePixel_To_RGB24(s[1].camera->img.data[i]),b=GRE_FramePixel_To_RGB24(s[2].camera->img.data[i]); if(YMGRE_Abs((int)a.R-b.R)>3||YMGRE_Abs((int)a.G-b.G)>3||YMGRE_Abs((int)a.B-b.B)>3)normalChanged++; checked++;}
	for(int y=20;y<H-20;y++){GRErgb24 l=GRE_FramePixel_To_RGB24(s[2].camera->img.data[y*W+2]),r=GRE_FramePixel_To_RGB24(s[2].camera->img.data[y*W+W-3]); if(YMGRE_Abs((int)l.R-r.R)>245&&YMGRE_Abs((int)l.G-r.G)>245)seamChanged++;}
	int pass=checked>1000&&normalChanged>500&&seamChanged==0; printf("earth UV/normal: checked=%d, changed=%d, seam jumps=%d: %s\n",checked,normalChanged,seamChanged,pass?"PASS":"FAIL"); return pass;
}
int main(void){Stage s[STAGES]={0};YMGRE_DemoView v[STAGES];for(int i=0;i<STAGES;i++){init(&s[i],i);v[i]=(YMGRE_DemoView){YMGRE_Camera_GetRenderTarget(s[i].camera),i*W,0,W,H};}int ok=verify(s);showInteractive(s,v);for(int i=0;i<STAGES;i++)destroy(&s[i]);return ok?0:1;}
