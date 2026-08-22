#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 490

//使用屏幕坐标直接绘制单个多边形，便于构造精确的退化条件
static void drawPolygon(GRE_Camera4d camera, const float32 point[][2], uint16 pointNum,
	GRErgb24 color, uint8 showLines)
{
	gre_vertex4d vertex[8] = { 0 };
	uint16 index[8] = { 0 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };

	for (uint16 i = 0; i < pointNum; i++)
	{
		vertex[i].pos = (gre_fvector4d){ point[i][0], point[i][1], 100.0f, 1.0f };
		index[i] = i;
	}
	polygon.num = pointNum;
	polygon.index = index;
	object.pointNum = pointNum;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, showLines);
}

//绘制六个测试区域的边界，空面板代表退化图元被正确忽略
static void drawPanelBorders(GRE_Camera4d camera)
{
	YMGRE_Img_SetBrushColor((GRErgb24){ 104, 108, 116 });
	for (uint16 row = 0; row < 2; row++)
	{
		for (uint16 column = 0; column < 3; column++)
		{
			int16 left = 8 + column * 164;
			int16 top = 8 + row * 241;
			int16 right = left + 155;
			int16 bottom = top + 224;
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				left, top, right, top);
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				right, top, right, bottom);
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				right, bottom, left, bottom);
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				left, bottom, left, top);
		}
	}
}

//依次覆盖重复顶点、零长度边、共线、极窄、近零面积和退化后恢复绘制
static void drawDegenerateCases(GRE_Camera4d camera)
{
	const float32 repeatedPoint[][2] = {
		{ 38, 45 }, { 38, 45 }, { 133, 52 }, { 142, 184 }, { 30, 195 }
	};
	const float32 samePoint[][2] = {
		{ 246, 120 }, { 246, 120 }, { 246, 120 }
	};
	const float32 collinear[][2] = {
		{ 354, 55 }, { 405, 120 }, { 456, 185 }
	};
	const float32 thinTriangle[][2] = {
		{ 65, 275 }, { 86, 445 }, { 88, 444 }
	};
	const float32 nearZero[][2] = {
		{ 245.0f, 355.0f }, { 245.1f, 355.0f },
		{ 245.1f, 355.00003f }, { 245.0f, 355.00003f }
	};
	const float32 outside[][2] = {
		{ 540, 520 }, { 610, 535 }, { 575, 610 }
	};
	const float32 normal[][2] = {
		{ 370, 285 }, { 466, 315 }, { 445, 435 }, { 355, 420 }
	};

	drawPolygon(camera, repeatedPoint, 5, (GRErgb24){ 68, 166, 126 }, 1);
	drawPolygon(camera, samePoint, 3, (GRErgb24){ 220, 86, 74 }, 1);
	drawPolygon(camera, collinear, 3, (GRErgb24){ 220, 86, 74 }, 1);
	drawPolygon(camera, thinTriangle, 3, (GRErgb24){ 232, 186, 62 }, 1);
	drawPolygon(camera, nearZero, 4, (GRErgb24){ 220, 86, 74 }, 1);
	drawPolygon(camera, outside, 3, (GRErgb24){ 220, 86, 74 }, 1);
	drawPolygon(camera, normal, 4, (GRErgb24){ 72, 128, 210 }, 1);
	drawPanelBorders(camera);
}

//将退化几何测试结果交给 YMGUI 显示
static void showDegenerateImage(GRE_Camera4d camera)
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

	drawDegenerateCases(camera);
	showDegenerateImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
