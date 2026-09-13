#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

#define STAGE_COUNT 6
#define PANEL_COLUMNS 3
#define PANEL_W 240
#define PANEL_H 220

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} PointLightStage;

static GRE_Object4d createTriangle(int stage, const char* materialName)
{
	GRE_Object4d object = YMGRE_Creat_Object(3, 1,
		"point_light_triangle", (char*)materialName);
	object->pointList[0] = (gre_vertex4d){ { -145, -100, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 145, -100, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 0, 140, 180, 1 }, 0.5f, 0 };
	object->polygonList[0].num = 3;
	object->polygonList[0].index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
	object->polygonList[0].index[0] = 0;
	object->polygonList[0].index[1] = 2;
	object->polygonList[0].index[2] = 1;
	object->polygonList[0].pN = (gre_fvector4d){ 0, 0, -1, 0 };
	object->polygonList[0].planeColor = (GRErgb24){ 255, 255, 255 };
	object->BoundingSphereR = 190;
	object->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	for (int i = 0; i < 3; i++)
		object->pointList_wN[i].color = (GRErgb24){ 255, 255, 255 };
	if (stage == 6)
	{
		object->pointList_wN[0].color = (GRErgb24){ 255, 45, 45 };
		object->pointList_wN[1].color = (GRErgb24){ 45, 255, 70 };
		object->pointList_wN[2].color = (GRErgb24){ 45, 90, 255 };
	}
	return object;
}

static GRE_Material createMaterial(int stage, const char* name)
{
	GRE_Material material = YMGRE_Creat_Material((char*)name);
	material->ambient = material->diffuse = (GRErgb24){ 255, 255, 255 };
	material->width = material->height = (stage >= 5) ? 16 : 1;
	uint32 count = (uint32)material->width * material->height;
	material->pixel = GRE_ImageBuff_Malloc(count * sizeof(GRErgb24));
	for (uint16 y = 0; y < material->height; y++)
	{
		for (uint16 x = 0; x < material->width; x++)
		{
			uint8 white = stage < 5 || !(((x / 4) + (y / 4)) & 1);
			material->pixel[y * material->width + x] = white ?
				(GRErgb24){ 255, 255, 255 } : (GRErgb24){ 0, 0, 0 };
		}
	}
	return material;
}

static GRE_Light4d createLight(int stage)
{
	if (stage == 1)
		return YMGRE_Creat_Light(0, GRE_GlobalLight,
			(GRErgb24){ 255, 255, 255 }, 1.0f);

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_PointLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	light->pos = stage == 2 ?
		(gre_fvector4d){ 0, 0, 65, 1 } :
		(gre_fvector4d){ -110, 80, 65, 1 };
	light->proper.shadowK = 0.0f;
	light->proper.kc0 = 1.0f;
	light->proper.kc1 = stage >= 4 ? 0.003f : 0.0f;
	light->proper.kc2 = 0.0f;
	return light;
}

static void initStage(PointLightStage* stageData, int stage)
{
	char materialName[24];
	snprintf(materialName, sizeof(materialName), "point_light_%d", stage);
	YMGRE_List_Append(&stageData->objects, sizeof(gre_object4d),
		createTriangle(stage, materialName));
	YMGRE_List_Append(&stageData->materials, sizeof(gre_material),
		createMaterial(stage, materialName));
	YMGRE_List_Append(&stageData->lights, sizeof(gre_light4d), createLight(stage));

	stageData->camera = YMGRE_Creat_Camera(stage, PANEL_W, PANEL_H,
		42, 42, 42, 42);
	YMGRE_Camera_Frustum_Init(stageData->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 };
	gre_fvector4d target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stageData->camera, &eye, &target, NULL, 0);
	stageData->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stageData->camera, &stageData->lights,
		&stageData->objects, &stageData->materials, stageData->workspace);
}

static void destroyStage(PointLightStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	PointLightStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i + 1);
		views[i] = (YMGRE_DemoView){
			YMGRE_Camera_GetRenderTarget(stages[i].camera),
			(i % PANEL_COLUMNS) * PANEL_W,
			(i / PANEL_COLUMNS) * PANEL_H,
			PANEL_W, PANEL_H
		};
	}
	YMGRE_DemoHost_Show(PANEL_COLUMNS * PANEL_W,
		(STAGE_COUNT / PANEL_COLUMNS) * PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return 0;
}
