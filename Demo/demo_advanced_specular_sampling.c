#include "YMGRE_Material.h"
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
#define PANEL_HALF_W 125.0f
#define PANEL_HALF_H 105.0f

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} SpecularSamplingStage;

static GRE_Object4d createPanel(int columns, int rows, uint8 renderMode)
{
	int pointCount = (columns + 1) * (rows + 1);
	int polygonCount = columns * rows * 2;
	GRE_Object4d object = YMGRE_Creat_Object(pointCount, polygonCount,
		"specular_sampling_panel", "specular_sampling_material");

	for (int y = 0; y <= rows; y++)
	{
		float32 fy = (float32)y / rows;
		for (int x = 0; x <= columns; x++)
		{
			float32 fx = (float32)x / columns;
			object->pointList[y * (columns + 1) + x] = (gre_vertex4d){
				{ -PANEL_HALF_W + 2.0f * PANEL_HALF_W * fx,
				  -PANEL_HALF_H + 2.0f * PANEL_HALF_H * fy, 180, 1 }, fx, 1.0f - fy };
		}
	}

	int polygonIndex = 0;
	for (int y = 0; y < rows; y++) for (int x = 0; x < columns; x++)
	{
		uint16 lowerLeft = (uint16)(y * (columns + 1) + x);
		uint16 lowerRight = lowerLeft + 1;
		uint16 upperLeft = lowerLeft + (uint16)(columns + 1);
		uint16 upperRight = upperLeft + 1;
		GRE_Index indices[6] = { lowerLeft, upperLeft, upperRight,
			lowerLeft, upperRight, lowerRight };
		for (int triangle = 0; triangle < 2; triangle++)
		{
			GRE_Polygon4d polygon = &object->polygonList[polygonIndex++];
			polygon->num = 3;
			polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
			for (int i = 0; i < 3; i++) polygon->index[i] = indices[triangle * 3 + i];
			polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
			polygon->planeColor = (GRErgb24){ 255, 255, 255 };
		}
	}

	object->BoundingSphereR = 170;
	object->boundType = GRE_Bounding_Sphere_R;
	object->mirrorKs = 1.0f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = renderMode;
	return object;
}

static GRE_Material createMaterial(void)
{
	GRE_Material material = YMGRE_Creat_Material("specular_sampling_material");
	material->ambient = (GRErgb24){ 80, 80, 80 };
	material->diffuse = (GRErgb24){ 0, 0, 0 };
	material->specular = (GRErgb24){ 75, 155, 255 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	YMGRE_Material_EnsureAdvanced(material);
	material->advanced->specularPower = 42;
	return material;
}

static void initStage(SpecularSamplingStage* stage, int index)
{
	int fineMesh = index == 2;
	uint8 renderMode = index == 1 ? GRE_RenderMode_Pixel : GRE_RenderMode_Vertex;
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d),
		createPanel(fineMesh ? 16 : 1, fineMesh ? 12 : 1, renderMode));
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial());
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 0.20f));
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

static int verifySampling(const SpecularSamplingStage* stages)
{
	unsigned long long coarseError = 0, fineError = 0;
	int checked = 0;
	for (int i = 0; i < PANEL_W * PANEL_H; i++)
	{
		if (stages[0].camera->img.zbuff[i] >= stages[0].camera->frustum.Zfar ||
			stages[1].camera->img.zbuff[i] >= stages[1].camera->frustum.Zfar ||
			stages[2].camera->img.zbuff[i] >= stages[2].camera->frustum.Zfar) continue;
		GRErgb24 coarse = GRE_FramePixel_To_RGB24(stages[0].camera->img.data[i]);
		GRErgb24 pixel = GRE_FramePixel_To_RGB24(stages[1].camera->img.data[i]);
		GRErgb24 fine = GRE_FramePixel_To_RGB24(stages[2].camera->img.data[i]);
		coarseError += YMGRE_Abs((int)coarse.R - pixel.R) +
			YMGRE_Abs((int)coarse.G - pixel.G) + YMGRE_Abs((int)coarse.B - pixel.B);
		fineError += YMGRE_Abs((int)fine.R - pixel.R) +
			YMGRE_Abs((int)fine.G - pixel.G) + YMGRE_Abs((int)fine.B - pixel.B);
		checked++;
	}
	int center = (PANEL_H / 2) * PANEL_W + PANEL_W / 2;
	GRErgb24 coarseCenter = GRE_FramePixel_To_RGB24(stages[0].camera->img.data[center]);
	GRErgb24 pixelCenter = GRE_FramePixel_To_RGB24(stages[1].camera->img.data[center]);
	GRErgb24 fineCenter = GRE_FramePixel_To_RGB24(stages[2].camera->img.data[center]);
	int passed = checked > 0 && pixelCenter.B > coarseCenter.B + 80 &&
		fineCenter.B > coarseCenter.B + 80 && fineError * 2 < coarseError;
	printf("specular sampling: checked=%d, center B=%u/%u/%u, error coarse=%llu, fine=%llu: %s\n",
		checked, coarseCenter.B, pixelCenter.B, fineCenter.B,
		(unsigned long long)coarseError, (unsigned long long)fineError,
		passed ? "PASS" : "FAIL");
	return passed;
}

static void destroyStage(SpecularSamplingStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	SpecularSamplingStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	int verified = verifySampling(stages);
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return verified ? 0 : 1;
}
