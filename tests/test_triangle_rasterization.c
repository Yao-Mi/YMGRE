#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_W 32
#define TEST_H 32

static float32 edgeValue(GRE_Fvector4d a, GRE_Fvector4d b, float32 x, float32 y)
{
	return (x - a->x) * (b->y - a->y) - (y - a->y) * (b->x - a->x);
}

static int pointInTriangle(GRE_Fvector4d a, GRE_Fvector4d b, GRE_Fvector4d c, float32 x, float32 y)
{
	float32 ab = edgeValue(a, b, x, y);
	float32 bc = edgeValue(b, c, x, y);
	float32 ca = edgeValue(c, a, x, y);
	return ((ab >= -0.001f) && (bc >= -0.001f) && (ca >= -0.001f)) ||
		((ab <= 0.001f) && (bc <= 0.001f) && (ca <= 0.001f));
}

int main(void)
{
	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	GRErgb24 background = { 0, 0, 0 };
	GRErgb24 color = { 200, 100, 50 };
	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_Img_Line(frame, TEST_W, TEST_H, 2, 2, 5, 2);
	GRE_FramePixel backgroundPixel = GRE_FramePixel_From_RGB24(background);
	int linePixelCount = 0;
	for (int i = 0; i < TEST_W * TEST_H; i++)
	{
		if (frame[i] != backgroundPixel)
			linePixelCount++;
	}
	if (linePixelCount != 4)
	{
		printf("test_triangle_rasterization: line pixels=%d FAILED\n", linePixelCount);
		return 1;
	}
	YMGRE_CameraImage_Init(&camera, background);

	gre_vertex4d points[3] = { 0 };
	points[0].pos = (gre_fvector4d){ 4.2f, 3.4f, 100.0f, 1.0f };
	points[1].pos = (gre_fvector4d){ 7.3f, 26.6f, 100.0f, 1.0f };
	points[2].pos = (gre_fvector4d){ 27.4f, 10.2f, 100.0f, 1.0f };
	uint16 index[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3;
	polygon.index = index;
	gre_object4d object = { 0 };
	object.pointNum = 3;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };

	YMGRE_TrangleObject_Primitive_RasterizationTo(&object, points, polygonHide, polygonColor, NULL, &camera);

	GRE_FramePixel fillPixel = GRE_FramePixel_From_RGB24(color);
	int fillCount = 0;
	int outsideCount = 0;
	for (int y = 0; y < TEST_H; y++)
	{
		for (int x = 0; x < TEST_W; x++)
		{
			if (frame[y * TEST_W + x] == fillPixel)
			{
				fillCount++;
				if (!pointInTriangle(&points[0].pos, &points[1].pos, &points[2].pos, x, y))
					outsideCount++;
			}
		}
	}

	if ((fillCount == 0) || (outsideCount != 0))
	{
		printf("test_triangle_rasterization: fill=%d outside=%d FAILED\n", fillCount, outsideCount);
		return 1;
	}

	//带线框路径必须保留片元覆盖范围，且确实写入线框颜色
	GRErgb24 wireColor = { 32, 220, 180 };
	GRE_FramePixel wirePixel = GRE_FramePixel_From_RGB24(wireColor);
	YMGRE_Img_SetBrushColor(wireColor);
	YMGRE_CameraImage_Init(&camera, background);
	object.wireFrame = 1;
	YMGRE_TrangleObject_Primitive_RasterizationTo(&object, points, polygonHide,
		polygonColor, NULL, &camera);
	int wireCount = 0;
	outsideCount = 0;
	for (int y = 0; y < TEST_H; y++)
	{
		for (int x = 0; x < TEST_W; x++)
		{
			if (frame[y * TEST_W + x] == wirePixel)
			{
				wireCount++;
				if (!pointInTriangle(&points[0].pos, &points[1].pos, &points[2].pos, x, y))
					outsideCount++;
			}
		}
	}
	if (wireCount == 0)
	{
		printf("test_triangle_rasterization: wire pixels missing FAILED\n");
		return 1;
	}
	printf("test_triangle_rasterization: fill=%d wire=%d PASS\n", fillCount, wireCount);
	return 0;
}
