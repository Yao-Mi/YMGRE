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
} LightOrderStage;

static GRE_Object4d createPanel(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2,
		"light_order_panel", "light_order_material");
	object->pointList[0] = (gre_vertex4d){ { -125, -105, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 125, -105, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 125, 105, 180, 1 }, 1, 0 };
	object->pointList[3] = (gre_vertex4d){ { -125, 105, 180, 1 }, 0, 0 };
	uint16 indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
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
	GRE_Material material = YMGRE_Creat_Material("light_order_material");
	material->ambient = (GRErgb24){ 0, 0, 0 };
	material->diffuse = (GRErgb24){ 255, 255, 255 };
	material->specular = (GRErgb24){ 0, 0, 0 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	return material;
}

static GRE_Light4d createPoint(int id, int red)
{
	GRErgb24 color = red ? (GRErgb24){ 255, 0, 0 } : (GRErgb24){ 0, 0, 255 };
	GRE_Light4d light = YMGRE_Creat_Light(id, GRE_PointLight, color, 2.2f);
	light->pos = red ? (gre_fvector4d){ -80, 40, 55, 1 } :
		(gre_fvector4d){ 80, -40, 55, 1 };
	light->proper.kc0 = 1.0f;
	light->proper.kc1 = 0.001f;
	light->proper.kc2 = 0.0001f;
	light->proper.shadowK = 0.0f;
	return light;
}

static void appendLightPair(LightOrderStage* stage, int blueFirst, int* id)
{
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d),
		createPoint((*id)++, blueFirst ? 0 : 1));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d),
		createPoint((*id)++, blueFirst ? 1 : 0));
}

static void initStage(LightOrderStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel());
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial());
	int id = 0;
	appendLightPair(stage, index == 1, &id);
	if (index == 2) appendLightPair(stage, 1, &id);

	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static int verifyLightOrder(const LightOrderStage* stages)
{
	int checked = 0, orderMismatch = 0, clampMismatch = 0;
	int saturatedPixels = 0, maxOrderError = 0, maxClampError = 0;
	for (int i = 0; i < PANEL_W * PANEL_H; i++)
	{
		if (stages[0].camera->img.zbuff[i] >= stages[0].camera->frustum.Zfar ||
			stages[1].camera->img.zbuff[i] >= stages[1].camera->frustum.Zfar ||
			stages[2].camera->img.zbuff[i] >= stages[2].camera->frustum.Zfar) continue;
		GRErgb24 redBlue = GRE_FramePixel_To_RGB24(stages[0].camera->img.data[i]);
		GRErgb24 blueRed = GRE_FramePixel_To_RGB24(stages[1].camera->img.data[i]);
		GRErgb24 doubled = GRE_FramePixel_To_RGB24(stages[2].camera->img.data[i]);
		GRErgb24 expected = {
			(uint8)GREMin((int)redBlue.R * 2, 255),
			(uint8)GREMin((int)redBlue.G * 2, 255),
			(uint8)GREMin((int)redBlue.B * 2, 255)
		};
		int orderError = GREMax(YMGRE_Abs((int)redBlue.R - blueRed.R),
			GREMax(YMGRE_Abs((int)redBlue.G - blueRed.G),
				YMGRE_Abs((int)redBlue.B - blueRed.B)));
		int clampError = GREMax(YMGRE_Abs((int)doubled.R - expected.R),
			GREMax(YMGRE_Abs((int)doubled.G - expected.G),
				YMGRE_Abs((int)doubled.B - expected.B)));
		if (orderError > 1) orderMismatch++;
		if (clampError > 1) clampMismatch++;
		if (expected.R == 255 || expected.G == 255 || expected.B == 255) saturatedPixels++;
		maxOrderError = GREMax(maxOrderError, orderError);
		maxClampError = GREMax(maxClampError, clampError);
		checked++;
	}
	int passed = checked > 0 && orderMismatch == 0 && clampMismatch == 0 &&
		saturatedPixels > 1000;
	printf("light order/clamp: checked=%d, order mismatch=%d (max=%d), "
		"clamp mismatch=%d (max=%d), saturated=%d: %s\n",
		checked, orderMismatch, maxOrderError, clampMismatch, maxClampError,
		saturatedPixels, passed ? "PASS" : "FAIL");
	return passed;
}

static void destroyStage(LightOrderStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	LightOrderStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifyLightOrder(stages);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
