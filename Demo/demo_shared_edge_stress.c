#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 490
#define MESH_POINT_MAX 80
#define MESH_TRIANGLE_MAX 120

//批量绘制共享顶点的三角片元
static void drawTriangleMesh(GRE_Camera4d camera, const float32 point[][2], uint16 pointNum,
	const uint16 triangleIndex[][3], uint16 triangleNum, float32 offsetX, float32 offsetY,
	GRErgb24 color, uint8 showLines)
{
	gre_vertex4d vertex[MESH_POINT_MAX] = { 0 };
	uint16 index[MESH_TRIANGLE_MAX][3] = { 0 };
	gre_polygon4d polygon[MESH_TRIANGLE_MAX] = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[MESH_TRIANGLE_MAX] = { 0 };
	GRErgb24 polygonColor[MESH_TRIANGLE_MAX] = { 0 };

	for (uint16 i = 0; i < pointNum; i++)
		vertex[i].pos = (gre_fvector4d){ point[i][0] + offsetX,
			point[i][1] + offsetY, 100.0f, 1.0f };
	for (uint16 i = 0; i < triangleNum; i++)
	{
		index[i][0] = triangleIndex[i][0];
		index[i][1] = triangleIndex[i][1];
		index[i][2] = triangleIndex[i][2];
		polygon[i].num = 3;
		polygon[i].index = index[i];
		polygonColor[i] = color;
	}
	object.pointNum = pointNum;
	object.polygonNum = triangleNum;
	object.polygonList = polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, showLines);
}

//十乘六网格使用交替对角线拆分为一百二十个三角片元
static void drawGridMesh(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	static float32 point[77][2];
	static uint16 triangleIndex[120][3];
	uint16 triangleNum = 0;
	for (uint16 row = 0; row <= 6; row++)
	{
		for (uint16 column = 0; column <= 10; column++)
		{
			uint16 index = row * 11 + column;
			point[index][0] = 5.0f + column * 21.0f;
			point[index][1] = 5.0f + row * (130.0f / 6.0f);
		}
	}
	for (uint16 row = 0; row < 6; row++)
	{
		for (uint16 column = 0; column < 10; column++)
		{
			uint16 topLeft = row * 11 + column;
			uint16 topRight = topLeft + 1;
			uint16 bottomLeft = topLeft + 11;
			uint16 bottomRight = bottomLeft + 1;
			if ((row + column) & 1)
			{
				triangleIndex[triangleNum][0] = topLeft;
				triangleIndex[triangleNum][1] = topRight;
				triangleIndex[triangleNum++][2] = bottomLeft;
				triangleIndex[triangleNum][0] = topRight;
				triangleIndex[triangleNum][1] = bottomRight;
				triangleIndex[triangleNum++][2] = bottomLeft;
			}
			else
			{
				triangleIndex[triangleNum][0] = topLeft;
				triangleIndex[triangleNum][1] = topRight;
				triangleIndex[triangleNum++][2] = bottomRight;
				triangleIndex[triangleNum][0] = topLeft;
				triangleIndex[triangleNum][1] = bottomRight;
				triangleIndex[triangleNum++][2] = bottomLeft;
			}
		}
	}
	drawTriangleMesh(camera, point, 77, triangleIndex, triangleNum,
		offsetX, offsetY, (GRErgb24){ 68, 166, 126 }, showLines);
}

//十六个三角片元共享中心点，形成辐射状斜边组合
static void drawFanMesh(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	const float32 point[][2] = {
		{ 110, 70 }, { 110, 5 }, { 135, 10 }, { 156, 24 }, { 170, 45 },
		{ 175, 70 }, { 170, 95 }, { 156, 116 }, { 135, 130 }, { 110, 135 },
		{ 85, 130 }, { 64, 116 }, { 50, 95 }, { 45, 70 }, { 50, 45 },
		{ 64, 24 }, { 85, 10 }
	};
	uint16 triangleIndex[16][3];
	for (uint16 i = 0; i < 16; i++)
	{
		triangleIndex[i][0] = 0;
		triangleIndex[i][1] = i + 1;
		triangleIndex[i][2] = (i == 15) ? 1 : i + 2;
	}
	drawTriangleMesh(camera, point, 17, triangleIndex, 16,
		offsetX, offsetY, (GRErgb24){ 72, 128, 210 }, showLines);
}

//三行不规则顶点构成两条窄片元带，重点覆盖小数交点和正负斜率
static void drawThinMesh(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	static float32 point[45][2];
	static uint16 triangleIndex[56][3];
	uint16 triangleNum = 0;
	for (uint16 column = 0; column <= 14; column++)
	{
		float32 x = 5.0f + column * 15.0f;
		point[column][0] = x;
		point[column][1] = 5.0f;
		point[15 + column][0] = x;
		point[15 + column][1] = 65.0f + ((int)(column % 5) - 2) * 5.5f;
		point[30 + column][0] = x;
		point[30 + column][1] = 135.0f;
	}
	for (uint16 row = 0; row < 2; row++)
	{
		for (uint16 column = 0; column < 14; column++)
		{
			uint16 topLeft = row * 15 + column;
			uint16 topRight = topLeft + 1;
			uint16 bottomLeft = topLeft + 15;
			uint16 bottomRight = bottomLeft + 1;
			triangleIndex[triangleNum][0] = topLeft;
			triangleIndex[triangleNum][1] = topRight;
			triangleIndex[triangleNum++][2] = bottomRight;
			triangleIndex[triangleNum][0] = topLeft;
			triangleIndex[triangleNum][1] = bottomRight;
			triangleIndex[triangleNum++][2] = bottomLeft;
		}
	}
	drawTriangleMesh(camera, point, 45, triangleIndex, triangleNum,
		offsetX, offsetY, (GRErgb24){ 220, 86, 74 }, showLines);
}

//三组压力场景左侧纯填充，右侧叠加片元网格
static void drawStressComparison(GRE_Camera4d camera)
{
	drawGridMesh(camera, 5, 5, 0);
	drawGridMesh(camera, 265, 5, 1);
	drawFanMesh(camera, 5, 165, 0);
	drawFanMesh(camera, 265, 165, 1);
	drawThinMesh(camera, 5, 335, 0);
	drawThinMesh(camera, 265, 335, 1);
}

//将共享边压力测试结果交给 YMGUI 显示
static void showStressImage(GRE_Camera4d camera)
{
	LCD_Init(800, 600);
	LCD_Fill_RgbRect(0, 0, camera->img.width, camera->img.height, camera->img.data);
	while (LCD_Update(60))
	{
	}
	LCD_Destory();
}

int main(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, DEMO_WIDTH, DEMO_HEIGHT,
		45.0f, 45.0f, 45.0f, 45.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	YMGRE_CameraImage_Init(camera, (GRErgb24){ 42, 43, 47 });
	YMGRE_Img_SetBrushColor((GRErgb24){ 225, 229, 235 });

	drawStressComparison(camera);
	showStressImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
