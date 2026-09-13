#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Coordinates_Transform.h"
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
} SpotSpecularStage;

static GRE_Object4d createPanel(int stage)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2,
		"spot_specular_panel", "spot_specular_material");
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
	object->mirrorKs = stage == 0 ? 0.0f : 1.0f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createMaterial(void)
{
	GRE_Material material = YMGRE_Creat_Material("spot_specular_material");
	material->ambient = (GRErgb24){ 75, 75, 75 };
	material->diffuse = (GRErgb24){ 95, 95, 95 };
	material->specular = (GRErgb24){ 55, 125, 255 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 220, 220, 220 };
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->specularPower = 48;
	return material;
}

static GRE_Light4d createSpot(int stage)
{
	GRE_Light4d light = YMGRE_Creat_Light(1, GRE_SpotLight,
		(GRErgb24){ 255, 245, 225 }, 1.0f);
	light->pos = stage < 2 ? (gre_fvector4d){ 0, 0, 55, 1 } :
		(gre_fvector4d){ 50, 25, 55, 1 };
	light->proper.kc0 = 1.0f;
	light->proper.kc1 = 0.001f;
	light->proper.kc2 = 0.0f;
	light->proper.shadowK = 0.0f;
	light->proper.spot.direct = (gre_fvector4d){ 0, 0, 1, 0 };
	YMGRE_Fvector4d_Normalize(&light->proper.spot.direct);
	return light;
}

static void initStage(SpotSpecularStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel(index));
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial());
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 0.12f));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), createSpot(index));
	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static void destroyStage(SpotSpecularStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

static int verifySpecularPeak(GRE_Camera4d camera, gre_fvector4d lightPosition,
	int stageNumber)
{
	/* Reflect the camera across z=180, then intersect light-to-reflected-camera
	 * with the plane. This is the analytic mirror point for the flat panel. */
	const float32 planeZ = 180.0f;
	gre_fvector4d reflectedCamera = { 0, 0, 2.0f * planeZ, 1 };
	float32 t = (planeZ - lightPosition.z) /
		(reflectedCamera.z - lightPosition.z);
	gre_fvector4d expected = {
		lightPosition.x + (reflectedCamera.x - lightPosition.x) * t,
		lightPosition.y + (reflectedCamera.y - lightPosition.y) * t,
		planeZ, 1
	};
	YMGRE_Point_WorldToCamera(&expected, &expected, &camera->move.TMat);
	YMGRE_Point_CameraToViewPlane(&expected, camera->perspectPlane.Dis);
	YMGRE_Point_ViewPlaneToWindows(&expected, camera);

	int peakScore = -1000000;
	for (int y = 0; y < camera->img.height; y++) for (int x = 0; x < camera->img.width; x++)
	{
		GRErgb24 pixel = GRE_FramePixel_To_RGB24(
			camera->img.data[y * camera->img.width + x]);
		int score = pixel.B;
		if (score > peakScore) peakScore = score;
	}
	int expectedX = (int)(expected.x + 0.5f);
	int expectedY = (int)(expected.y + 0.5f);
	int peakX = 0, peakY = 0, nearestDistance = 0x7FFFFFFF;
	for (int y = 0; y < camera->img.height; y++) for (int x = 0; x < camera->img.width; x++)
	{
		GRErgb24 pixel = GRE_FramePixel_To_RGB24(
			camera->img.data[y * camera->img.width + x]);
		int score = pixel.B;
		if (score < peakScore - 2) continue;
		int px = x - expectedX, py = y - expectedY;
		int distance = px * px + py * py;
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			peakX = x;
			peakY = y;
		}
	}
	int dx = peakX - expectedX, dy = peakY - expectedY;
	int passed = dx * dx + dy * dy <= 9;
	printf("spot specular stage %d: expected=(%d,%d), framebuffer peak=(%d,%d), "
		"delta=(%d,%d): %s\n", stageNumber, expectedX, expectedY,
		peakX, peakY, dx, dy, passed ? "PASS" : "FAIL");
	return passed;
}

int main(void)
{
	SpotSpecularStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifySpecularPeak(stages[1].camera,
		(gre_fvector4d){ 0, 0, 55, 1 }, 2);
	verified &= verifySpecularPeak(stages[2].camera,
		(gre_fvector4d){ 50, 25, 55, 1 }, 3);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
