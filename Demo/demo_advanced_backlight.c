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

static const GRErgb24 kDiffuse = { 180, 110, 45 };
static const gre_fvector4d kBackLightPosition = { 0, 0, 260, 1 };
static const float32 kShadowK = 0.30f;

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} BacklightStage;

static GRE_Object4d createPanel(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2, "backlight_panel", "backlight_material");
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
		polygon->planeColor = kDiffuse;
	}
	object->BoundingSphereR = 170;
	object->boundType = GRE_Bounding_Sphere_R;
	object->mirrorKs = 1.0f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createMaterial(void)
{
	GRE_Material material = YMGRE_Creat_Material("backlight_material");
	material->ambient = (GRErgb24){ 65, 65, 65 };
	material->diffuse = kDiffuse;
	material->specular = (GRErgb24){ 40, 80, 255 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->specularPower = 36;
	return material;
}

static GRE_Light4d createPoint(int stageIndex)
{
	GRE_Light4d light = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	light->pos = stageIndex == 0 ? (gre_fvector4d){ 0, 0, 55, 1 } : kBackLightPosition;
	light->proper.kc0 = 1.0f;
	light->proper.kc1 = 0.002f;
	light->proper.kc2 = 0.0f;
	light->proper.shadowK = stageIndex == 2 ? kShadowK : 0.0f;
	return light;
}

static void initStage(BacklightStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel());
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial());
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 0.25f));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), createPoint(index));
	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static int verifyBacklight(const BacklightStage* stages)
{
	int checked = 0, ambientMismatch = 0, shadowMismatch = 0, brighter = 0;
	int maxAmbientError = 0, maxShadowError = 0, blueDominant = 0;
	GRE_Camera4d ambientCamera = stages[1].camera;
	GRE_Camera4d shadowCamera = stages[2].camera;
	float32 viewW = shadowCamera->perspectPlane.pR - shadowCamera->perspectPlane.pL;
	float32 viewH = shadowCamera->perspectPlane.pU - shadowCamera->perspectPlane.pD;
	for (int y = 0; y < PANEL_H; y++) for (int x = 0; x < PANEL_W; x++)
	{
		int index = y * PANEL_W + x;
		if (ambientCamera->img.zbuff[index] >= ambientCamera->frustum.Zfar ||
			shadowCamera->img.zbuff[index] >= shadowCamera->frustum.Zfar) continue;
		GRErgb24 ambient = GRE_FramePixel_To_RGB24(ambientCamera->img.data[index]);
		GRErgb24 shadow = GRE_FramePixel_To_RGB24(shadowCamera->img.data[index]);
		int ambientError = GREMax(YMGRE_Abs((int)ambient.R - ambient.G),
			YMGRE_Abs((int)ambient.G - ambient.B));
		if (ambientError > 1) ambientMismatch++;
		maxAmbientError = GREMax(maxAmbientError, ambientError);

		float32 z = shadowCamera->img.zbuff[index];
		gre_fvector4d fragment = {
			((x - PANEL_W * 0.5f) * viewW / PANEL_W) * z / shadowCamera->perspectPlane.Dis,
			((y - PANEL_H * 0.5f) * -viewH / PANEL_H) * z / shadowCamera->perspectPlane.Dis,
			z, 1
		};
		gre_fvector4d lightVector;
		YMGRE_Fvector4d_SubToResult((GRE_Fvector4d)&kBackLightPosition, &fragment, &lightVector);
		float32 distance = YMGRE_Fvector4d_Len1(&lightVector);
		float32 attenuation = 1.0f / (1.0f + 0.002f * distance);
		int addR = (int)(255.0f * attenuation * kDiffuse.R * kShadowK / 255.0f);
		int addG = (int)(255.0f * attenuation * kDiffuse.G * kShadowK / 255.0f);
		int addB = (int)(255.0f * attenuation * kDiffuse.B * kShadowK / 255.0f);
		GRErgb24 expected = {
			(uint8)GREMin((int)ambient.R + addR, 255),
			(uint8)GREMin((int)ambient.G + addG, 255),
			(uint8)GREMin((int)ambient.B + addB, 255)
		};
		int shadowError = GREMax(YMGRE_Abs((int)shadow.R - expected.R),
			GREMax(YMGRE_Abs((int)shadow.G - expected.G),
				YMGRE_Abs((int)shadow.B - expected.B)));
		if (shadowError > 1) shadowMismatch++;
		maxShadowError = GREMax(maxShadowError, shadowError);
		if (shadow.R > ambient.R + 5) brighter++;
		if (shadow.B > shadow.R + 2) blueDominant++;
		checked++;
	}
	int passed = checked > 0 && ambientMismatch == 0 && shadowMismatch == 0 &&
		brighter > 1000 && blueDominant == 0;
	printf("backlight/shadowK: checked=%d, ambient mismatch=%d (max=%d), "
		"shadow mismatch=%d (max=%d), brighter=%d, blue dominant=%d: %s\n",
		checked, ambientMismatch, maxAmbientError, shadowMismatch, maxShadowError,
		brighter, blueDominant, passed ? "PASS" : "FAIL");
	return passed;
}

static void destroyStage(BacklightStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	BacklightStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifyBacklight(stages);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
