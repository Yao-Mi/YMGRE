#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_W 68
#define TEST_H 68
#define GRID_SIZE 8
#define POINT_NUM 81
#define TRIANGLE_NUM 128

static GRE_FramePixel frame[TEST_W * TEST_H];
static float32 depth[TEST_W * TEST_H];
static gre_vertex4d vertex[POINT_NUM];
static GRE_Index triangleIndex[TRIANGLE_NUM][3];
static gre_polygon4d polygon[TRIANGLE_NUM];
static uint8 polygonHide[TRIANGLE_NUM];
static GRErgb24 polygonColor[TRIANGLE_NUM];

static uint32 frameHash(void)
{
	uint32 hash = 2166136261u;
	const uint8* data = (const uint8*)frame;
	for (uint32 i = 0; i < sizeof(frame); i++)
	{
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash;
}

static void initGrid(GRErgb24 color)
{
	uint16 triangleNum = 0;
	for (uint16 row = 0; row <= GRID_SIZE; row++)
	{
		for (uint16 column = 0; column <= GRID_SIZE; column++)
		{
			uint16 pointIndex = row * (GRID_SIZE + 1) + column;
			vertex[pointIndex].pos = (gre_fvector4d){ 2.0f + column * 8.0f,
				2.0f + row * 8.0f, 100.0f, 1.0f };
		}
	}
	for (uint16 row = 0; row < GRID_SIZE; row++)
	{
		for (uint16 column = 0; column < GRID_SIZE; column++)
		{
			uint16 topLeft = row * (GRID_SIZE + 1) + column;
			uint16 topRight = topLeft + 1;
			uint16 bottomLeft = topLeft + GRID_SIZE + 1;
			uint16 bottomRight = bottomLeft + 1;
			triangleIndex[triangleNum][0] = topLeft;
			triangleIndex[triangleNum][1] = topRight;
			triangleIndex[triangleNum][2] = bottomRight;
			polygon[triangleNum].num = 3;
			polygon[triangleNum].index = triangleIndex[triangleNum];
			polygonColor[triangleNum++] = color;
			triangleIndex[triangleNum][0] = topLeft;
			triangleIndex[triangleNum][1] = bottomRight;
			triangleIndex[triangleNum][2] = bottomLeft;
			polygon[triangleNum].num = 3;
			polygon[triangleNum].index = triangleIndex[triangleNum];
			polygonColor[triangleNum++] = color;
		}
	}
}

int main(void)
{
	gre_camera4d camera = { 0 };
	gre_object4d object = { 0 };
	GRErgb24 background = { 0, 0, 0 };
	GRErgb24 color = { 68, 166, 126 };
	GRE_FramePixel fillPixel = GRE_FramePixel_From_RGB24(color);
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	initGrid(color);
	object.pointNum = POINT_NUM;
	object.polygonNum = TRIANGLE_NUM;
	object.polygonList = polygon;

	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, &camera, 0);
	for (uint16 y = 2; y < 66; y++)
	{
		for (uint16 x = 2; x < 66; x++)
		{
		if (!GRE_FramePixel_Equals(frame[y * TEST_W + x], fillPixel))
			{
				printf("test_shared_edge_stress: gap at %u,%u FAILED\n", x, y);
				return 1;
			}
		}
	}
	uint32 forwardHash = frameHash();

	for (uint16 i = 0; i < TRIANGLE_NUM / 2; i++)
	{
		gre_polygon4d swap = polygon[i];
		polygon[i] = polygon[TRIANGLE_NUM - 1 - i];
		polygon[TRIANGLE_NUM - 1 - i] = swap;
	}
	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, &camera, 0);
	if (frameHash() != forwardHash)
	{
		printf("test_shared_edge_stress: draw order changed frame FAILED\n");
		return 1;
	}

	//合并线框路径不能在共享边附近留下背景裂缝
	GRErgb24 wireColor = { 230, 230, 230 };
	GRE_FramePixel wirePixel = GRE_FramePixel_From_RGB24(wireColor);
	YMGRE_Img_SetBrushColor(wireColor);
	YMGRE_CameraImage_Init(&camera, background);
	object.wireFrame = 1;
	YMGRE_TrangleObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, NULL, &camera);
	uint32 wireCount = 0;
	for (uint16 y = 2; y < 66; y++)
	{
		for (uint16 x = 2; x < 66; x++)
		{
			GRE_FramePixel pixel = frame[y * TEST_W + x];
			if (GRE_FramePixel_Equals(pixel, wirePixel))
				wireCount++;
		else if (!GRE_FramePixel_Equals(pixel, fillPixel))
			{
				printf("test_shared_edge_stress: wire gap at %u,%u FAILED\n", x, y);
				return 1;
			}
		}
	}
	if (wireCount == 0)
	{
		printf("test_shared_edge_stress: wire pixels missing FAILED\n");
		return 1;
	}
	//内部共享边必须在全部片元填充结束后保留，并保持单像素宽度
	if ((!GRE_FramePixel_Equals(frame[5 * TEST_W + 10], wirePixel)) ||
		GRE_FramePixel_Equals(frame[5 * TEST_W + 9], wirePixel) ||
		GRE_FramePixel_Equals(frame[5 * TEST_W + 11], wirePixel))
	{
		printf("test_shared_edge_stress: shared wire overwritten or widened FAILED\n");
		return 1;
	}
	printf("test_shared_edge_stress: triangles=%d wire=%u no gaps PASS\n",
		TRIANGLE_NUM, wireCount);
	return 0;
}
