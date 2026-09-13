#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_W 100
#define TEST_H 180

static GRE_FramePixel frame[TEST_W * TEST_H];
static float32 depth[TEST_W * TEST_H];

int main(void)
{
	gre_camera4d camera = { 0 };
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	GRErgb24 background = { 0, 0, 0 };
	GRErgb24 color = { 72, 128, 210 };
	GRE_FramePixel backgroundPixel = GRE_FramePixel_From_RGB24(background);
	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_Img_SetBrushColor((GRErgb24){ 255, 255, 255 });

	gre_vertex4d points[5] = { 0 };
	points[0].pos = (gre_fvector4d){ 45, 5, 100.0f, 1.0f };
	points[1].pos = (gre_fvector4d){ 85, 45, 100.0f, 1.0f };
	points[2].pos = (gre_fvector4d){ 70, 160, 100.0f, 1.0f };
	points[3].pos = (gre_fvector4d){ 20, 160, 100.0f, 1.0f };
	points[4].pos = (gre_fvector4d){ 5, 45, 100.0f, 1.0f };
	GRE_Index index[5] = { 0, 1, 2, 3, 4 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 5;
	polygon.index = index;
	gre_object4d object = { 0 };
	object.pointNum = 5;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };

	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, points, polygonHide,
		polygonColor, &camera, 1);

	int gapCount = 0;
	for (int y = 5; y <= 160; y++)
	{
		int begX = 0;
		int endX = TEST_W - 1;
		while ((begX < TEST_W) && GRE_FramePixel_Equals(frame[y * TEST_W + begX], backgroundPixel))
			begX++;
		while ((endX >= 0) && GRE_FramePixel_Equals(frame[y * TEST_W + endX], backgroundPixel))
			endX--;
		for (int x = begX; x <= endX; x++)
		{
			if (GRE_FramePixel_Equals(frame[y * TEST_W + x], backgroundPixel))
				gapCount++;
		}
	}

	if (gapCount != 0)
	{
		printf("test_polygon_rasterization: edge gaps=%d FAILED\n", gapCount);
		return 1;
	}
	printf("test_polygon_rasterization: edge gaps=0 PASS\n");
	return 0;
}
