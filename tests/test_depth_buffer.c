#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_W 64
#define TEST_H 64

static void drawTriangle(GRE_Camera4d camera, GRErgb24 color, float32 depth, float32 offsetX)
{
	gre_vertex4d vertex[3] = { 0 };
	uint16 index[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };
	polygon.num = 3;
	polygon.index = index;
	object.pointNum = 3;
	object.polygonNum = 1;
	object.polygonList = &polygon;

	vertex[0].pos = (gre_fvector4d){ 6.0f + offsetX, 52.0f, depth, 1.0f };
	vertex[1].pos = (gre_fvector4d){ 28.0f + offsetX, 10.0f, depth, 1.0f };
	vertex[2].pos = (gre_fvector4d){ 50.0f + offsetX, 52.0f, depth, 1.0f };
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, 1);
}

static uint32 frameHash(GRE_FrameBuffer frame, uint32 count)
{
	uint32 hash = 2166136261u;
	const uint8* data = (const uint8*)frame;
	for (uint32 i = 0; i < count * sizeof(GRE_FramePixel); i++)
	{
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash;
}

int main(void)
{
	gre_camera4d camera = { 0 };
	GRErgb24 background = { 0, 0, 0 };
	GRErgb24 farColor = { 220, 86, 74 };
	GRErgb24 nearColor = { 72, 128, 210 };
	GRE_FramePixel nearPixel = GRE_FramePixel_From_RGB24(nearColor);
	YMGRE_Img_SetBrushColor((GRErgb24){ 255, 255, 255 });
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	static GRE_FramePixel frame[TEST_W * TEST_H];
	static float32 depth[TEST_W * TEST_H];
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;

	YMGRE_CameraImage_Init(&camera, background);
	drawTriangle(&camera, farColor, 140.0f, 0.0f);
	drawTriangle(&camera, nearColor, 80.0f, 8.0f);
	uint32 firstHash = frameHash(frame, TEST_W * TEST_H);
	if (!GRE_FramePixel_Equals(frame[32 * TEST_W + 32], nearPixel))
	{
		printf("test_depth_buffer: near triangle did not win FAILED\n");
		return 1;
	}

	YMGRE_CameraImage_Init(&camera, background);
	drawTriangle(&camera, nearColor, 80.0f, 8.0f);
	drawTriangle(&camera, farColor, 140.0f, 0.0f);
	if (frameHash(frame, TEST_W * TEST_H) != firstHash)
	{
		printf("test_depth_buffer: draw order changed result FAILED\n");
		return 1;
	}
	printf("test_depth_buffer: order-independent occlusion PASS\n");
	return 0;
}
