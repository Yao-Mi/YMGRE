#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

#define COUNT 4
#define PANEL_W 300
#define PANEL_H 360

typedef struct { gre_list objects, lights, materials; GRE_Camera4d camera; GRE_RenderWorkspace workspace; } Stage;

static GRE_Material material(void)
{
	GRE_Material m = YMGRE_Creat_Material("advanced_light_mat");
	m->ambient = (GRErgb24){ 90, 90, 90 }; m->diffuse = (GRErgb24){ 205, 205, 205 };
	m->width = m->height = 1; m->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24)); m->pixel[0] = m->diffuse;
	return m;
}

static void initStage(Stage* s, int mode)
{
	GRE_Object4d cube = YMGRE_MeshGener_Cube(105, (GRErgb24){ 220, 220, 220 }, "advanced_light_cube", "advanced_light_mat");
	cube->WorldCoordinate = (gre_fvector4d){ 0, 0, 180, 1 }; YMGRE_Object_LocalToWorld(cube); YMGRE_Object_GenerateVertexAttributes(cube);
	cube->mirrorKs = mode == 1 ? 0.8f : (mode == 2 ? 0.45f : 0.0f);
	cube->renderMode = GRE_RenderMode_Face;
	YMGRE_List_Append(&s->objects, sizeof(gre_object4d), cube);
	GRE_Material stageMaterial = material();
	if (mode == 0) stageMaterial->ambient = stageMaterial->diffuse = (GRErgb24){ 220, 220, 220 };
	YMGRE_List_Append(&s->materials, sizeof(gre_material), stageMaterial);
	if (mode == 0 || mode == 3)
		YMGRE_List_Append(&s->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
			GRE_GlobalLight, mode == 3 ? (GRErgb24){ 165, 200, 245 } : (GRErgb24){ 255, 255, 255 },
			mode == 3 ? 1.0f : 1.0f));
	else
	{
		GRE_Light4d key = YMGRE_Creat_Light(0, GRE_PointLight, mode == 1 ? (GRErgb24){ 255, 245, 225 } : (GRErgb24){ 255, 90, 70 }, mode == 1 ? 1.7f : 1.15f);
		key->pos = (gre_fvector4d){ -95, 125, 35, 1 }; key->proper.kc1 = .001f; key->proper.shadowK = .03f; YMGRE_List_Append(&s->lights, sizeof(gre_light4d), key);
		if (mode == 2)
		{
			GRE_Light4d fill = YMGRE_Creat_Light(1, GRE_PointLight, (GRErgb24){ 70, 125, 255 }, 1.8f);
			fill->pos = (gre_fvector4d){ 105, -45, 35, 1 }; fill->proper.kc1 = .001f; fill->proper.shadowK = 0; YMGRE_List_Append(&s->lights, sizeof(gre_light4d), fill);
		}
	}
	s->camera = YMGRE_Creat_Camera(mode, PANEL_W, PANEL_H, 42, 42, 38, 38); YMGRE_Camera_Frustum_Init(s->camera, 1, 600);
	gre_fvector4d eye = { 0, 70, -95, 1 }, target = { 0, 0, 180, 1 }; YMGRE_UVNCamera_PositionInit(s->camera, &eye, &target, NULL, 0);
	s->workspace = YMGRE_Creat_RenderWorkspace(); YMGRE_Camera_TanglePipline_wN(s->camera, &s->lights, &s->objects, &s->materials, s->workspace);
}

static void destroyStage(Stage* s) { YMGRE_Free_RenderWorkspace(s->workspace); YMGRE_Free_Camera(s->camera); YMGRE_List_Clear(&s->lights, YMGRE_Free_Light); YMGRE_List_Clear(&s->objects, YMGRE_Free_Object); YMGRE_List_Clear(&s->materials, YMGRE_Free_Material); }

int main(void)
{
	Stage stages[COUNT] = { 0 }; YMGRE_DemoView views[COUNT];
	for (int i = 0; i < COUNT; i++) { initStage(&stages[i], i); views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera), i * PANEL_W, 0, PANEL_W, PANEL_H }; }
	YMGRE_DemoHost_Show(COUNT * PANEL_W, PANEL_H, views, COUNT, 60);
	for (int i = 0; i < COUNT; i++) destroyStage(&stages[i]); return 0;
}
