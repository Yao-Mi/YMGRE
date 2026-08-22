#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 260

//绘制一个带深度的屏幕空间三角形
static void drawTriangle(GRE_Camera4d camera, float32 offsetX, float32 shapeOffsetX,
	GRErgb24 color, float32 depth, uint8 showLines)
{
	gre_vertex4d vertex[3] = { 0 };
	uint16 index[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };

	vertex[0].pos = (gre_fvector4d){ 20.0f + offsetX + shapeOffsetX, 205.0f, depth, 1.0f };
	vertex[1].pos = (gre_fvector4d){ 120.0f + offsetX + shapeOffsetX, 35.0f, depth, 1.0f };
	vertex[2].pos = (gre_fvector4d){ 220.0f + offsetX + shapeOffsetX, 205.0f, depth, 1.0f };
	polygon.num = 3;
	polygon.index = index;
	object.pointNum = 3;
	object.polygonNum = 1;
	object.polygonList = &polygon;

	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, showLines);
}

//两组相同图元交换绘制顺序，观察遮挡结果是否一致
static void drawDepthComparison(GRE_Camera4d camera)
{
	GRErgb24 farColor = { 220, 86, 74 };
	GRErgb24 nearColor = { 72, 128, 210 };

	//左侧：远处红色先画，近处蓝色后画
	drawTriangle(camera, 0.0f, 0.0f, farColor, 140.0f, 1);
	drawTriangle(camera, 0.0f, 35.0f, nearColor, 80.0f, 1);
	//右侧：近处蓝色先画，远处红色后画
	drawTriangle(camera, 260.0f, 35.0f, nearColor, 80.0f, 1);
	drawTriangle(camera, 260.0f, 0.0f, farColor, 140.0f, 1);
}

//将深度遮挡结果交给 YMGUI 显示
static void showDepthImage(GRE_Camera4d camera)
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

	drawDepthComparison(camera);
	showDepthImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
