#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 230
#define POLYGON_POINT_MAX 5

//直接使用屏幕坐标绘制一个基础多边形
static void drawPolygon(GRE_Camera4d camera, const float32 point[][2], uint16 pointNum,
	GRErgb24 color, uint8 showLines)
{
	gre_vertex4d vertex[POLYGON_POINT_MAX] = { 0 };
	uint16 index[POLYGON_POINT_MAX] = { 0 };
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

//依次绘制三角形、四边形和五边形的填充与叠线对照
static void drawPolygonComparison(GRE_Camera4d camera)
{
	const float32 triangleSolid[][2] = { { 10, 190 }, { 45, 35 }, { 80, 190 } };
	const float32 triangleWire[][2] = { { 90, 190 }, { 125, 35 }, { 160, 190 } };
	const float32 quadSolid[][2] = { { 170, 50 }, { 225, 50 }, { 230, 190 }, { 175, 190 } };
	const float32 quadWire[][2] = { { 245, 50 }, { 300, 50 }, { 305, 190 }, { 250, 190 } };
	const float32 pentagonSolid[][2] = { { 355, 35 }, { 395, 75 }, { 380, 190 }, { 330, 190 }, { 315, 75 } };
	const float32 pentagonWire[][2] = { { 455, 35 }, { 495, 75 }, { 480, 190 }, { 430, 190 }, { 415, 75 } };

	drawPolygon(camera, triangleSolid, 3, (GRErgb24){ 220, 86, 74 }, 0);
	drawPolygon(camera, triangleWire, 3, (GRErgb24){ 220, 86, 74 }, 1);
	drawPolygon(camera, quadSolid, 4, (GRErgb24){ 68, 166, 126 }, 0);
	drawPolygon(camera, quadWire, 4, (GRErgb24){ 68, 166, 126 }, 1);
	drawPolygon(camera, pentagonSolid, 5, (GRErgb24){ 72, 128, 210 }, 0);
	drawPolygon(camera, pentagonWire, 5, (GRErgb24){ 72, 128, 210 }, 1);
}

//将光栅化结果交给 YMGUI 显示
static void showPolygonImage(GRE_Camera4d camera)
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

	drawPolygonComparison(camera);
	showPolygonImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
