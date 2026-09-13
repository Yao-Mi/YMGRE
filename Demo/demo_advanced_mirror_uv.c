#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"

#define PANEL_COUNT 2
#define PANEL_W 360
#define PANEL_H 360

typedef struct { gre_list objects, lights, materials; GRE_Camera4d camera; GRE_RenderWorkspace workspace; } Stage;

static GRE_Object4d createPanel(uint8 mirrored, const char* materialName)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2, "mirror_uv", (char*)materialName);
	float32 u0 = mirrored ? 1.0f : 0.0f, u1 = mirrored ? 0.0f : 1.0f;
	object->pointList[0] = (gre_vertex4d){ { -125, -125, 180, 1 }, u0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 125, -125, 180, 1 }, u1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 125, 125, 180, 1 }, u1, 0 };
	object->pointList[3] = (gre_vertex4d){ { -125, 125, 180, 1 }, u0, 0 };
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++) { GRE_Polygon4d p = &object->polygonList[i]; p->num = 3; p->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index)); for (int j = 0; j < 3; j++) p->index[j] = indices[i * 3 + j]; p->pN = (gre_fvector4d){ 0, 0, -1, 0 }; p->planeColor = (GRErgb24){ 220, 220, 220 }; }
	object->BoundingSphereR = 180; object->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	for (int i = 0; i < 4; i++) object->pointList_wN[i].color = (GRErgb24){ 255, 255, 255 };
	return object;
}

static GRE_Material createMaterial(const char* name)
{
	GRE_Material m = YMGRE_Creat_Material((char*)name); m->ambient = (GRErgb24){ 100, 100, 100 }; m->diffuse = (GRErgb24){ 220, 220, 220 }; m->width = m->height = 8; m->pixel = GRE_ImageBuff_Malloc(64 * sizeof(GRErgb24));
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) m->pixel[y * 8 + x] = ((x + y) & 1) ? (GRErgb24){ 210, 210, 210 } : (GRErgb24){ 55, 55, 55 };
	m->advanced = GRE_malloc0(sizeof(gre_material_advanced)); m->advanced->normalWidth = m->advanced->normalHeight = 16; m->advanced->normalPixel = GRE_ImageBuff_Malloc(256 * sizeof(GRErgb24));
	for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) { float32 nx = ((x / 4) & 1) ? -0.28f : 0.28f, ny = ((y / 4) & 1) ? -0.20f : 0.20f; gre_fvector4d n = { nx, ny, 0.92f, 0 }; YMGRE_Fvector4d_Normalize(&n); m->advanced->normalPixel[y * 16 + x] = (GRErgb24){ (uint8)((n.x * .5f + .5f) * 255), (uint8)((n.y * .5f + .5f) * 255), (uint8)((n.z * .5f + .5f) * 255) }; }
	return m;
}

static void initStage(Stage* s, uint8 mirrored, int id)
{
	const char* name = mirrored ? "mirror" : "normal";
	YMGRE_List_Append(&s->objects, sizeof(gre_object4d), createPanel(mirrored, name)); YMGRE_List_Append(&s->materials, sizeof(gre_material), createMaterial(name));
	YMGRE_List_Append(&s->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){ 100, 100, 100 }, .28f));
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight, (GRErgb24){ 255, 245, 225 }, 1.5f); point->pos = (gre_fvector4d){ -75, 90, 115, 1 }; point->proper.kc1 = .001f; point->proper.shadowK = 0; YMGRE_List_Append(&s->lights, sizeof(gre_light4d), point);
	s->camera = YMGRE_Creat_Camera(id, PANEL_W, PANEL_H, 42, 42, 42, 42); YMGRE_Camera_Frustum_Init(s->camera, 1, 500); gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 }; YMGRE_UVNCamera_PositionInit(s->camera, &eye, &target, NULL, 0); s->workspace = YMGRE_Creat_RenderWorkspace(); YMGRE_Camera_TanglePipline_wN(s->camera, &s->lights, &s->objects, &s->materials, s->workspace);
}

static void destroyStage(Stage* s) { YMGRE_Free_RenderWorkspace(s->workspace); YMGRE_Free_Camera(s->camera); YMGRE_List_Clear(&s->lights, YMGRE_Free_Light); YMGRE_List_Clear(&s->objects, YMGRE_Free_Object); YMGRE_List_Clear(&s->materials, YMGRE_Free_Material); }

int main(void)
{
	Stage stages[PANEL_COUNT] = { 0 }; YMGRE_DemoView views[PANEL_COUNT];
	for (int i = 0; i < PANEL_COUNT; i++) { initStage(&stages[i], (uint8)i, i); views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera), i * PANEL_W, 0, PANEL_W, PANEL_H }; }
	YMGRE_DemoHost_Show(PANEL_COUNT * PANEL_W, PANEL_H, views, PANEL_COUNT, 60);
	for (int i = 0; i < PANEL_COUNT; i++) destroyStage(&stages[i]); return 0;
}
