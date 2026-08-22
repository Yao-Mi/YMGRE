#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 490
#define FRAGMENT_POINT_MAX 11
#define FRAGMENT_POLYGON_MAX 5
#define POLYGON_POINT_MAX 5

//使用同一组顶点绘制多个相邻片元
static void drawFragments(GRE_Camera4d camera, const float32 point[][2], uint16 pointNum,
	const uint16 polygonIndex[][POLYGON_POINT_MAX], const uint16 polygonPointNum[], uint16 polygonNum,
	float32 offsetX, float32 offsetY, GRErgb24 color, uint8 showLines, uint8 triangleRaster)
{
	gre_vertex4d vertex[FRAGMENT_POINT_MAX] = { 0 };
	uint16 index[FRAGMENT_POLYGON_MAX][POLYGON_POINT_MAX] = { 0 };
	gre_polygon4d polygon[FRAGMENT_POLYGON_MAX] = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[FRAGMENT_POLYGON_MAX] = { 0 };
	GRErgb24 polygonColor[FRAGMENT_POLYGON_MAX] = { 0 };

	for (uint16 i = 0; i < pointNum; i++)
	{
		vertex[i].pos = (gre_fvector4d){ point[i][0] + offsetX,
			point[i][1] + offsetY, 100.0f, 1.0f };
	}
	for (uint16 i = 0; i < polygonNum; i++)
	{
		for (uint16 j = 0; j < polygonPointNum[i]; j++)
			index[i][j] = polygonIndex[i][j];
		polygon[i].num = polygonPointNum[i];
		polygon[i].index = index[i];
		polygonColor[i] = color;
	}
	object.pointNum = pointNum;
	object.polygonNum = polygonNum;
	object.polygonList = polygon;

	if (triangleRaster)
	{
		//三角片元使用填充与线框合并的快速光栅化路径
		object.wireFrame = showLines;
		YMGRE_TrangleObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
			polygonColor, NULL, camera);
	}
	else
	{
		YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
			polygonColor, camera, showLines);
	}
}

//五个三角片元共享中心点，拼成一个五边形
static void drawTriangleFragments(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	const float32 point[][2] = {
		{ 110, 5 }, { 205, 50 }, { 175, 145 }, { 45, 145 }, { 15, 50 }, { 110, 85 }
	};
	const uint16 polygonIndex[][POLYGON_POINT_MAX] = {
		{ 5, 0, 1 }, { 5, 1, 2 }, { 5, 2, 3 }, { 5, 3, 4 }, { 5, 4, 0 }
	};
	const uint16 polygonPointNum[] = { 3, 3, 3, 3, 3 };

	drawFragments(camera, point, 6, polygonIndex, polygonPointNum, 5,
		offsetX, offsetY, (GRErgb24){ 220, 86, 74 }, showLines, 1);
}

//四个矩形片元以二乘二排列，拼成一个大矩形
static void drawRectangleFragments(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	const float32 point[][2] = {
		{ 10, 15 }, { 110, 15 }, { 210, 15 },
		{ 10, 75 }, { 110, 75 }, { 210, 75 },
		{ 10, 135 }, { 110, 135 }, { 210, 135 }
	};
	const uint16 polygonIndex[][POLYGON_POINT_MAX] = {
		{ 0, 1, 4, 3 }, { 1, 2, 5, 4 }, { 3, 4, 7, 6 }, { 4, 5, 8, 7 }
	};
	const uint16 polygonPointNum[] = { 4, 4, 4, 4 };

	drawFragments(camera, point, 9, polygonIndex, polygonPointNum, 4,
		offsetX, offsetY, (GRErgb24){ 68, 166, 126 }, showLines, 0);
}

//五条弯折共享边将外轮廓划分为五个五边形片元
static void drawPentagonFragments(GRE_Camera4d camera, float32 offsetX, float32 offsetY, uint8 showLines)
{
	const float32 point[][2] = {
		{ 110, 5 }, { 205, 50 }, { 175, 145 }, { 45, 145 }, { 15, 50 },
		{ 110, 85 }, { 118, 46 }, { 162, 74 }, { 136, 119 }, { 70, 113 }, { 65, 60 }
	};
	const uint16 polygonIndex[][POLYGON_POINT_MAX] = {
		{ 0, 1, 7, 5, 6 }, { 1, 2, 8, 5, 7 }, { 2, 3, 9, 5, 8 },
		{ 3, 4, 10, 5, 9 }, { 4, 0, 6, 5, 10 }
	};
	const uint16 polygonPointNum[] = { 5, 5, 5, 5, 5 };

	drawFragments(camera, point, 11, polygonIndex, polygonPointNum, 5,
		offsetX, offsetY, (GRErgb24){ 72, 128, 210 }, showLines, 0);
}

//三行分别对照三角形、矩形和五边形片元的拼接结果
static void drawFragmentComparison(GRE_Camera4d camera)
{
	drawTriangleFragments(camera, 10, 5, 0);
	drawTriangleFragments(camera, 270, 5, 1);
	drawRectangleFragments(camera, 10, 170, 0);
	drawRectangleFragments(camera, 270, 170, 1);
	drawPentagonFragments(camera, 10, 335, 0);
	drawPentagonFragments(camera, 270, 335, 1);
}

//将片元拼接结果交给 YMGUI 显示
static void showFragmentImage(GRE_Camera4d camera)
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

	drawFragmentComparison(camera);
	showFragmentImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
