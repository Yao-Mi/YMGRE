#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
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
} NormalSpecularStage;

static GRE_Object4d createPanel(int stage)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2,
		"normal_specular_panel", "normal_specular_material");
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
	object->mirrorKs = stage == 0 ? 0.0f : 1.0f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRErgb24 encodeNormal(float32 x, float32 y, float32 z)
{
	gre_fvector4d normal = { x, y, z, 0 };
	YMGRE_Fvector4d_Normalize(&normal);
	return (GRErgb24){
		(uint8)((normal.x * 0.5f + 0.5f) * 255.0f),
		(uint8)((normal.y * 0.5f + 0.5f) * 255.0f),
		(uint8)((normal.z * 0.5f + 0.5f) * 255.0f)
	};
}

static GRE_Material createMaterial(int stage)
{
	GRE_Material material = YMGRE_Creat_Material("normal_specular_material");
	material->ambient = (GRErgb24){ 85, 85, 85 };
	material->diffuse = (GRErgb24){ 0, 0, 0 };
	material->specular = (GRErgb24){ 90, 165, 255 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->specularPower = 28;
	material->advanced->normalWidth = material->advanced->normalHeight = 16;
	material->advanced->normalPixel = GRE_ImageBuff_Malloc(256 * sizeof(GRErgb24));
	for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
	{
		GRErgb24 normal = { 128, 128, 255 };
		if (stage == 2)
		{
			float32 nx = ((x / 4) & 1) ? -0.32f : 0.32f;
			float32 ny = ((y / 4) & 1) ? -0.24f : 0.24f;
			normal = encodeNormal(nx, ny, 0.92f);
		}
		material->advanced->normalPixel[y * 16 + x] = normal;
	}
	return material;
}

static void initStage(NormalSpecularStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel(index));
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial(index));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 0.22f));
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	point->pos = (gre_fvector4d){ 0, 0, 55, 1 };
	point->proper.kc0 = 1.0f;
	point->proper.kc1 = 0.001f;
	point->proper.kc2 = 0.0f;
	point->proper.shadowK = 0.0f;
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), point);
	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static int verifyNormalSpecular(const NormalSpecularStage* stages)
{
	int checked = 0, flatHighlightPixels = 0, changedPixels = 0;
	for (int i = 0; i < PANEL_W * PANEL_H; i++)
	{
		if (stages[1].camera->img.zbuff[i] >= stages[1].camera->frustum.Zfar) continue;
		GRErgb24 baseline = GRE_FramePixel_To_RGB24(stages[0].camera->img.data[i]);
		GRErgb24 flat = GRE_FramePixel_To_RGB24(stages[1].camera->img.data[i]);
		GRErgb24 perturbed = GRE_FramePixel_To_RGB24(stages[2].camera->img.data[i]);
		if (flat.B > baseline.B + 10) flatHighlightPixels++;
		if (YMGRE_Abs((int)flat.R - perturbed.R) > 2 ||
			YMGRE_Abs((int)flat.G - perturbed.G) > 2 ||
			YMGRE_Abs((int)flat.B - perturbed.B) > 2) changedPixels++;
		checked++;
	}
	int passed = checked > 0 && flatHighlightPixels > 100 && changedPixels > 1000;
	printf("normal-map specular: checked=%d, flat highlight=%d, perturbed difference=%d: %s\n",
		checked, flatHighlightPixels, changedPixels, passed ? "PASS" : "FAIL");
	return passed;
}

static void destroyStage(NormalSpecularStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	NormalSpecularStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifyNormalSpecular(stages);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
