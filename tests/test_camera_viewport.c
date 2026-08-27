#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <math.h>
#include <stdio.h>

#define WIDE_WIDTH 80
#define WIDE_HEIGHT 48
#define TALL_WIDTH 36
#define TALL_HEIGHT 72
#define COLOR_GUARD ((GRE_FramePixel)0x5AA5)
#define DEPTH_GUARD 12345.0f

typedef struct guarded_color_
{
	GRE_FramePixel before;
	GRE_FramePixel data[WIDE_WIDTH * WIDE_HEIGHT];
	GRE_FramePixel after;
}guarded_color;

typedef struct guarded_depth_
{
	float32 before;
	float32 data[WIDE_WIDTH * WIDE_HEIGHT];
	float32 after;
}guarded_depth;

typedef struct guarded_tall_color_
{
	GRE_FramePixel before;
	GRE_FramePixel data[TALL_WIDTH * TALL_HEIGHT];
	GRE_FramePixel after;
}guarded_tall_color;

typedef struct guarded_tall_depth_
{
	float32 before;
	float32 data[TALL_WIDTH * TALL_HEIGHT];
	float32 after;
}guarded_tall_depth;

static void noFree(void* data)
{
	(void)data;
}

static uint32 frameHash(GRE_FrameBuffer frame, uint32 count)
{
	uint32 hash = 2166136261u;
	const uint8* bytes = (const uint8*)frame;
	for (uint32 i = 0; i < count * sizeof(GRE_FramePixel); i++)
	{
		hash ^= bytes[i];
		hash *= 16777619u;
	}
	return hash;
}

static void matrixIdentity(GRE_FMat4x4 matrix)
{
	GRE_memset(matrix, 0, sizeof(gre_fmat4x4));
	for (uint16 i = 0; i < 4; i++)
		matrix->val[i][i] = 1.0f;
}

static GRE_Object4d creatTriangle(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(3, 1, "triangle", "");
	object->pointList[0].pos = (gre_fvector4d){ -30, -20, 100, 1 };
	object->pointList[1].pos = (gre_fvector4d){ 0, 25, 100, 1 };
	object->pointList[2].pos = (gre_fvector4d){ 30, -20, 100, 1 };
	object->polygonList[0].num = 3;
	object->polygonList[0].index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
	object->polygonList[0].index[0] = 0;
	object->polygonList[0].index[1] = 1;
	object->polygonList[0].index[2] = 2;
	object->polygonList[0].pN = (gre_fvector4d){ 0, 0, -2700, 0 };
	object->polygonList[0].planeColor = (GRErgb24){ 72, 128, 210 };
	object->BoundingSphereR = 40.0f;
	object->boundType = GRE_Bounding_Sphere_R;
	object->WorldCoordinate = (gre_fvector4d){ 0, 0, 100, 1 };
	return object;
}

int main(void)
{
	static guarded_color wideColor;
	static guarded_depth wideDepth;
	static guarded_tall_color tallColor;
	static guarded_tall_depth tallDepth;
	wideColor.before = wideColor.after = COLOR_GUARD;
	wideDepth.before = wideDepth.after = DEPTH_GUARD;
	tallColor.before = tallColor.after = COLOR_GUARD;
	tallDepth.before = tallDepth.after = DEPTH_GUARD;
	gre_render_target wideTarget;
	gre_render_target tallTarget;
	YMGRE_RenderTarget_Init(&wideTarget, WIDE_WIDTH, WIDE_HEIGHT,
		wideColor.data, wideDepth.data);
	YMGRE_RenderTarget_Init(&tallTarget, TALL_WIDTH, TALL_HEIGHT,
		tallColor.data, tallDepth.data);
	GRE_Camera4d wide = YMGRE_Creat_CameraFromTarget(0, &wideTarget,
		55.0f, 55.0f, 28.0f, 28.0f);
	GRE_Camera4d tall = YMGRE_Creat_CameraFromTarget(1, &tallTarget,
		25.0f, 25.0f, 55.0f, 55.0f);
	YMGRE_Camera_Frustum_Init(wide, 1.0f, 500.0f);
	YMGRE_Camera_Frustum_Init(tall, 1.0f, 500.0f);
	gre_fvector4d cameraPosition = { 12.0f, 18.0f, -30.0f, 1.0f };
	gre_fvector4d cameraTarget = { 0.0f, 2.0f, 5.0f, 1.0f };
	YMGRE_UVNCamera_PositionInit(wide, &cameraPosition, &cameraTarget, NULL, 0.0f);
	float32 basisLengths = YMGRE_Fvector4d_Len1(&wide->move.cu) +
		YMGRE_Fvector4d_Len1(&wide->move.cv) + YMGRE_Fvector4d_Len1(&wide->move.cn);
	if (!isfinite(basisLengths) || fabsf(basisLengths - 3.0f) > 0.001f)
	{
		printf("test_camera_viewport: camera basis was not synchronized FAILED\n");
		return 1;
	}
	gre_fvector4d verticalPosition = { 0.0f, 10.0f, 0.0f, 1.0f };
	gre_fvector4d verticalTarget = { 0.0f, 0.0f, 0.0f, 1.0f };
	YMGRE_UVNCamera_PositionInit(wide, &verticalPosition, &verticalTarget, NULL, 0.0f);
	basisLengths = YMGRE_Fvector4d_Len1(&wide->move.cu) +
		YMGRE_Fvector4d_Len1(&wide->move.cv) + YMGRE_Fvector4d_Len1(&wide->move.cn);
	if (!isfinite(basisLengths) || fabsf(basisLengths - 3.0f) > 0.001f)
	{
		printf("test_camera_viewport: vertical camera basis degenerated FAILED\n");
		return 1;
	}
	matrixIdentity(&wide->move.TMat);
	matrixIdentity(&tall->move.TMat);
	GRE_Object4d object = creatTriangle();
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	YMGRE_List_Append(&objects, sizeof(gre_object4d), object);
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();

	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(wide,
		&lights, &objects, &materials, workspace);
	uint32 wideHash = frameHash(wideColor.data, WIDE_WIDTH * WIDE_HEIGHT);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(tall,
		&lights, &objects, &materials, workspace);
	if (frameHash(wideColor.data, WIDE_WIDTH * WIDE_HEIGHT) != wideHash)
	{
		printf("test_camera_viewport: second camera changed first target FAILED\n");
		return 1;
	}
	if ((wideHash == frameHash(tallColor.data, TALL_WIDTH * TALL_HEIGHT)) ||
		(wide->img.width != WIDE_WIDTH) || (wide->img.height != WIDE_HEIGHT) ||
		(tall->img.width != TALL_WIDTH) || (tall->img.height != TALL_HEIGHT))
	{
		printf("test_camera_viewport: viewport dimensions or views FAILED\n");
		return 1;
	}
	if ((wideColor.before != COLOR_GUARD) || (wideColor.after != COLOR_GUARD) ||
		(tallColor.before != COLOR_GUARD) || (tallColor.after != COLOR_GUARD) ||
		(wideDepth.before != DEPTH_GUARD) || (wideDepth.after != DEPTH_GUARD) ||
		(tallDepth.before != DEPTH_GUARD) || (tallDepth.after != DEPTH_GUARD))
	{
		printf("test_camera_viewport: target guard changed FAILED\n");
		return 1;
	}

	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_List_Clear(&objects, noFree);
	YMGRE_List_Clear(&lights, noFree);
	YMGRE_Free_Camera(wide);
	YMGRE_Free_Camera(tall);
	YMGRE_Free_Light(light);
	YMGRE_Free_Object(object);
	printf("test_camera_viewport: independent targets and guards PASS\n");
	return 0;
}
