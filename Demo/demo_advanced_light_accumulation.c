#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

#define STAGE_COUNT 3
#define PANEL_W 300
#define PANEL_H 340

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} AccumulationStage;

static GRE_Object4d createPanel(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2,
		"accumulation_panel", "accumulation_material");
	object->pointList[0] = (gre_vertex4d){ { -125, -105, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 125, -105, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 125, 105, 180, 1 }, 1, 0 };
	object->pointList[3] = (gre_vertex4d){ { -125, 105, 180, 1 }, 0, 0 };
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
		for (int j = 0; j < 3; j++) polygon->index[j] = indices[i * 3 + j];
		polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
		polygon->planeColor = (GRErgb24){ 255, 255, 255 };
	}
	object->BoundingSphereR = 170;
	object->boundType = GRE_Bounding_Sphere_R;
	object->mirrorKs = 0.0f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createMaterial(void)
{
	GRE_Material material = YMGRE_Creat_Material("accumulation_material");
	material->ambient = (GRErgb24){ 0, 0, 0 };
	material->diffuse = (GRErgb24){ 210, 210, 210 };
	material->specular = (GRErgb24){ 0, 0, 0 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	return material;
}

static GRE_Light4d createPoint(int id, GRErgb24 color)
{
	GRE_Light4d light = YMGRE_Creat_Light(id, GRE_PointLight, color, 0.9f);
	light->pos = (gre_fvector4d){ 35, 25, 55, 1 };
	light->proper.kc0 = 1.0f;
	light->proper.kc1 = 0.001f;
	light->proper.kc2 = 0.0f;
	light->proper.shadowK = 0.0f;
	return light;
}

static void initStage(AccumulationStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel());
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial());
	if (index != 1)
		YMGRE_List_Append(&stage->lights, sizeof(gre_light4d),
			createPoint(0, (GRErgb24){ 235, 35, 25 }));
	if (index != 0)
		YMGRE_List_Append(&stage->lights, sizeof(gre_light4d),
			createPoint(1, (GRErgb24){ 25, 70, 235 }));
	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static int verifyAccumulation(const AccumulationStage* stages)
{
	int checked = 0, mismatched = 0, maxError = 0;
	GRE_Camera4d combined = stages[2].camera;
	for (int i = 0; i < PANEL_W * PANEL_H; i++)
	{
		if (combined->img.zbuff[i] >= combined->frustum.Zfar) continue;
		GRErgb24 red = GRE_FramePixel_To_RGB24(stages[0].camera->img.data[i]);
		GRErgb24 blue = GRE_FramePixel_To_RGB24(stages[1].camera->img.data[i]);
		GRErgb24 actual = GRE_FramePixel_To_RGB24(combined->img.data[i]);
		GRErgb24 expected = {
			(uint8)GREMin((int)red.R + blue.R, 255),
			(uint8)GREMin((int)red.G + blue.G, 255),
			(uint8)GREMin((int)red.B + blue.B, 255)
		};
		int er = YMGRE_Abs((int)actual.R - expected.R);
		int eg = YMGRE_Abs((int)actual.G - expected.G);
		int eb = YMGRE_Abs((int)actual.B - expected.B);
		int error = GREMax(er, GREMax(eg, eb));
		if (error > 1) mismatched++;
		maxError = GREMax(maxError, error);
		checked++;
	}
	int passed = checked > 0 && mismatched == 0;
	printf("light accumulation: checked=%d, mismatched=%d, max channel error=%d: %s\n",
		checked, mismatched, maxError, passed ? "PASS" : "FAIL");
	return passed;
}

static void destroyStage(AccumulationStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	AccumulationStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifyAccumulation(stages);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
