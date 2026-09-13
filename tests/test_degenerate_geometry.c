#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_WIDTH 64
#define TEST_HEIGHT 64
#define TEST_PIXEL_NUM (TEST_WIDTH * TEST_HEIGHT)
#define GUARD_NUM 8
#define GUARD_VALUE 0x6A5C39E1u

typedef struct frame_guard_
{
	uint32 before[GUARD_NUM];
	GRE_FramePixel data[TEST_PIXEL_NUM];
	uint32 after[GUARD_NUM];
}frame_guard;

typedef struct depth_guard_
{
	uint32 before[GUARD_NUM];
	float32 data[TEST_PIXEL_NUM];
	uint32 after[GUARD_NUM];
}depth_guard;

static frame_guard frameBuffer;
static depth_guard depthBuffer;
static gre_camera4d camera;
static GRErgb24 background = { 0, 0, 0 };

static void initGuard(void)
{
	for (uint16 i = 0; i < GUARD_NUM; i++)
	{
		frameBuffer.before[i] = GUARD_VALUE;
		frameBuffer.after[i] = GUARD_VALUE;
		depthBuffer.before[i] = GUARD_VALUE;
		depthBuffer.after[i] = GUARD_VALUE;
	}
	camera.img.width = TEST_WIDTH;
	camera.img.height = TEST_HEIGHT;
	camera.img.data = frameBuffer.data;
	camera.img.zbuff = depthBuffer.data;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	YMGRE_CameraImage_Init(&camera, background);
}

static int guardIsValid(void)
{
	for (uint16 i = 0; i < GUARD_NUM; i++)
	{
		if ((frameBuffer.before[i] != GUARD_VALUE) ||
			(frameBuffer.after[i] != GUARD_VALUE) ||
			(depthBuffer.before[i] != GUARD_VALUE) ||
			(depthBuffer.after[i] != GUARD_VALUE))
			return 0;
	}
	return 1;
}

static int depthIsValid(void)
{
	for (uint16 i = 0; i < TEST_PIXEL_NUM; i++)
	{
		//NaN 同样无法同时满足下面两个范围条件
		if (!((depthBuffer.data[i] >= camera.frustum.Znear) &&
			(depthBuffer.data[i] <= camera.frustum.Zfar)))
			return 0;
	}
	return 1;
}

static int countColor(GRErgb24 color)
{
	GRE_FramePixel pixel = GRE_FramePixel_From_RGB24(color);
	int count = 0;
	for (uint16 i = 0; i < TEST_PIXEL_NUM; i++)
	{
			if (GRE_FramePixel_Equals(frameBuffer.data[i], pixel))
			count++;
	}
	return count;
}

static void drawPolygon(const gre_fvector4d point[], uint16 pointNum, GRErgb24 color)
{
	gre_vertex4d vertex[8] = { 0 };
	GRE_Index index[8] = { 0 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };
	for (uint16 i = 0; i < pointNum; i++)
	{
		vertex[i].pos = point[i];
		index[i] = i;
	}
	polygon.num = pointNum;
	polygon.index = index;
	object.pointNum = pointNum;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, &camera, 0);
}

static void drawTriangle(const gre_fvector4d point[3], GRErgb24 color)
{
	gre_vertex4d vertex[3] = { 0 };
	GRE_Index index[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };
	for (uint16 i = 0; i < 3; i++)
		vertex[i].pos = point[i];
	polygon.num = 3;
	polygon.index = index;
	object.pointNum = 3;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_TrangleObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, NULL, &camera);
}

int main(void)
{
	const GRErgb24 validColor = { 68, 166, 126 };
	const GRErgb24 invalidColor = { 220, 86, 74 };
	const GRErgb24 thinColor = { 232, 186, 62 };
	const GRErgb24 normalColor = { 72, 128, 210 };
	const gre_fvector4d repeatedPoint[] = {
		{ 8, 8, 100, 1 }, { 8, 8, 100, 1 }, { 48, 9, 100, 1 },
		{ 52, 45, 100, 1 }, { 10, 50, 100, 1 }
	};
	const gre_fvector4d samePoint[] = {
		{ 20, 20, 100, 1 }, { 20, 20, 100, 1 }, { 20, 20, 100, 1 }
	};
	const gre_fvector4d collinear[] = {
		{ 7, 9, 100, 1 }, { 27, 29, 100, 1 }, { 47, 49, 100, 1 }
	};
	const gre_fvector4d horizontal[] = {
		{ 8, 30, 100, 1 }, { 28, 30, 100, 1 }, { 51, 30, 100, 1 }
	};
	const gre_fvector4d nearZero[] = {
		{ 10, 20, 100, 1 }, { 40, 20.0000001f, 100, 1 },
		{ 50, 20.0000002f, 100, 1 }, { 18, 20.0000001f, 100, 1 }
	};
	const gre_fvector4d outside[] = {
		{ -80, -70, 100, 1 }, { -25, -60, 100, 1 }, { -45, -15, 100, 1 }
	};
	const gre_fvector4d thinTriangle[] = {
		{ 6.2f, 5.2f, 100, 1 }, { 30.2f, 56.2f, 100, 1 },
		{ 31.2f, 56.2f, 100, 1 }
	};
	const gre_fvector4d normal[] = {
		{ 12, 12, 90, 1 }, { 52, 18, 90, 1 }, { 30, 53, 90, 1 }
	};

	initGuard();
	drawPolygon(repeatedPoint, 5, validColor);
	if (countColor(validColor) == 0)
	{
		printf("test_degenerate_geometry: repeated vertex polygon FAILED\n");
		return 1;
	}

	YMGRE_CameraImage_Init(&camera, background);
	drawPolygon(samePoint, 3, invalidColor);
	drawPolygon(collinear, 3, invalidColor);
	drawPolygon(nearZero, 4, invalidColor);
	drawPolygon(outside, 3, invalidColor);
	drawTriangle(samePoint, invalidColor);
	drawTriangle(collinear, invalidColor);
	drawTriangle(horizontal, invalidColor);
	if (countColor(invalidColor) != 0)
	{
		printf("test_degenerate_geometry: degenerate primitive produced pixels FAILED\n");
		return 1;
	}
	drawTriangle(thinTriangle, thinColor);
	if (countColor(thinColor) == 0)
	{
		printf("test_degenerate_geometry: valid thin triangle FAILED\n");
		return 1;
	}

	//退化图元之后绘制正常三角形，验证光栅化状态仍可继续使用
	drawTriangle(normal, normalColor);
	if (countColor(normalColor) == 0)
	{
		printf("test_degenerate_geometry: normal primitive after degenerate input FAILED\n");
		return 1;
	}
	if (!guardIsValid())
	{
		printf("test_degenerate_geometry: buffer guard FAILED\n");
		return 1;
	}
	if (!depthIsValid())
	{
		printf("test_degenerate_geometry: depth range FAILED\n");
		return 1;
	}

	printf("test_degenerate_geometry: PASS\n");
	return 0;
}
