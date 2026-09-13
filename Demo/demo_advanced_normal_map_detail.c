#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
#include <stdio.h>

#define PANEL_COUNT 3
#define PANEL_W 240
#define SCREEN_H 300

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} NormalMapStage;

static GRE_Object4d createPanel(int stage, const char* materialName)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2, "normal_map_detail", (char*)materialName);
	/* One UV span per panel keeps the normal pattern readable instead of repeating it four times. */
	object->pointList[0] = (gre_vertex4d){ { -92, -92, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 92, -92, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 92, 92, 180, 1 }, 1, 0 };
	object->pointList[3] = (gre_vertex4d){ { -92, 92, 180, 1 }, 0, 0 };
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
		for (int j = 0; j < 3; j++) polygon->index[j] = indices[i * 3 + j];
		polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
		polygon->planeColor = (GRErgb24){ 210, 210, 210 };
	}
	object->BoundingSphereR = 135;
	object->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	for (int i = 0; i < 4; i++) object->pointList_wN[i].color = (GRErgb24){ 255, 255, 255 };
	if (stage == 3)
	{
		object->pointList_wN[0].tangentW = 1;
		object->pointList_wN[1].tangentW = -1;
		object->pointList_wN[2].tangentW = -1;
		object->pointList_wN[3].tangentW = 1;
	}
	return object;
}

static GRE_Material createMaterial(int stage, const char* name)
{
	GRE_Material material = YMGRE_Creat_Material((char*)name);
	material->ambient = (GRErgb24){ 60, 60, 60 };
	material->diffuse = (GRErgb24){ 220, 220, 220 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 220, 220, 220 };
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->normalWidth = material->advanced->normalHeight = 16;
	material->advanced->normalPixel = GRE_ImageBuff_Malloc(256 * sizeof(GRErgb24));
	for (uint16 y = 0; y < 16; y++) for (uint16 x = 0; x < 16; x++)
	{
		gre_fvector4d normal = (gre_fvector4d){ 0, 0, 1, 0 };
		if (stage >= 2)
		{
			float32 sx = ((x / 4) & 1) ? -0.25f : 0.25f;
			float32 sy = ((y / 4) & 1) ? -0.18f : 0.18f;
			normal = (gre_fvector4d){ sx, sy, 0.95f, 0 };
			YMGRE_Fvector4d_Normalize(&normal);
		}
		material->advanced->normalPixel[y * 16 + x] = (GRErgb24){
			(uint8)((normal.x * 0.5f + 0.5f) * 255),
			(uint8)((normal.y * 0.5f + 0.5f) * 255),
			(uint8)((normal.z * 0.5f + 0.5f) * 255) };
	}
	return material;
}

static void initStage(NormalMapStage* stageData, int stage)
{
	char name[24];
	snprintf(name, sizeof(name), "normal_detail_%d", stage);
	YMGRE_List_Append(&stageData->objects, sizeof(gre_object4d), createPanel(stage, name));
	YMGRE_List_Append(&stageData->materials, sizeof(gre_material), createMaterial(stage, name));
	YMGRE_List_Append(&stageData->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 120, 120, 120 }, 0.32f));
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 245, 225 }, 1.7f);
	point->pos = (gre_fvector4d){ -30, 115, 120, 1 };
	point->proper.kc1 = 0.001f;
	point->proper.shadowK = 0.0f;
	YMGRE_List_Append(&stageData->lights, sizeof(gre_light4d), point);
	stageData->camera = YMGRE_Creat_Camera(stage, PANEL_W, SCREEN_H, 42, 42, 42, 42);
	YMGRE_Camera_Frustum_Init(stageData->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stageData->camera, &eye, &target, NULL, 0);
	stageData->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stageData->camera, &stageData->lights,
		&stageData->objects, &stageData->materials, stageData->workspace);
}

static void destroyStage(NormalMapStage* stageData)
{
	YMGRE_Free_RenderWorkspace(stageData->workspace);
	YMGRE_Free_Camera(stageData->camera);
	YMGRE_List_Clear(&stageData->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stageData->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stageData->materials, YMGRE_Free_Material);
}

int main(void)
{
	NormalMapStage stages[PANEL_COUNT] = { 0 };
	YMGRE_DemoView views[PANEL_COUNT];
	for (int i = 0; i < PANEL_COUNT; i++)
	{
		initStage(&stages[i], i + 1);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, SCREEN_H };
	}
	YMGRE_DemoHost_Show(PANEL_COUNT * PANEL_W, SCREEN_H, views, PANEL_COUNT, 60);
	for (int i = 0; i < PANEL_COUNT; i++) destroyStage(&stages[i]);
	return 0;
}
