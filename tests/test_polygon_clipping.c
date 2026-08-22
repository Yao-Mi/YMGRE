#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include <stdio.h>

#define TEST_W 64
#define TEST_H 64
#define SOURCE_LINE_NUM 5

int main(void)
{
	gre_fline sourceLines[SOURCE_LINE_NUM] = {
		{ 5, 25, 25, 5 }, { 25, 5, 58, 22 }, { 58, 22, 52, 58 },
		{ 52, 58, 12, 54 }, { 12, 54, 5, 25 }
	};
	gre_flineslist input = { SOURCE_LINE_NUM, SOURCE_LINE_NUM, sourceLines };
	gre_line clippedLines[10];
	gre_lineslist output = { 10, clippedLines };
	gre_frect window = { 16, 16, 48, 48 };
	YMGRE_Polygon_clip2D(&input, &output, &window);

	if (output.lineNum == 0)
	{
		printf("test_polygon_clipping: empty result FAILED\n");
		return 1;
	}
	for (uint16 i = 0; i < output.lineNum; i++)
	{
		GRE_LINE line = &output.data[i];
		if ((line->x0 < window.x0) || (line->x0 > window.x1) ||
			(line->y0 < window.y0) || (line->y0 > window.y1) ||
			(line->x1 < window.x0) || (line->x1 > window.x1) ||
			(line->y1 < window.y0) || (line->y1 > window.y1))
		{
			printf("test_polygon_clipping: endpoint outside window FAILED\n");
			return 1;
		}
	}

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
	GRErgb24 color = { 180, 120, 60 };
	gre_fvector4d plane = { 0, 0, 1, -100 };
	YMGRE_CameraImage_Init(&camera, background);
	YMGRE_Img_Scanline_AreaFill(frame, TEST_W, TEST_H, &output, &plane, depth, color);

	GRE_FramePixel fillPixel = GRE_FramePixel_From_RGB24(color);
	int fillCount = 0;
	for (int y = 0; y < TEST_H; y++)
	{
		for (int x = 0; x < TEST_W; x++)
		{
			if (frame[y * TEST_W + x] == fillPixel)
			{
				fillCount++;
				if ((x < window.x0) || (x > window.x1) || (y < window.y0) || (y > window.y1))
				{
					printf("test_polygon_clipping: fill outside window FAILED\n");
					return 1;
				}
			}
		}
	}

	if (fillCount == 0)
	{
		printf("test_polygon_clipping: empty fill FAILED\n");
		return 1;
	}
	printf("test_polygon_clipping: lines=%u fill=%d PASS\n", output.lineNum, fillCount);
	return 0;
}
