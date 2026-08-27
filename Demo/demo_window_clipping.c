#include "demo_host.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 260
#define POLYGON_POINT_NUM 5

//直接绘制未经过自定义窗口裁剪的多边形
static void drawSourcePolygon(GRE_Camera4d camera, const float32 point[][2], GRErgb24 color)
{
	gre_vertex4d vertex[POLYGON_POINT_NUM] = { 0 };
	uint16 index[POLYGON_POINT_NUM] = { 0 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };

	for (uint16 i = 0; i < POLYGON_POINT_NUM; i++)
	{
		vertex[i].pos = (gre_fvector4d){ point[i][0], point[i][1], 100.0f, 1.0f };
		index[i] = i;
	}
	polygon.num = POLYGON_POINT_NUM;
	polygon.index = index;
	object.pointNum = POLYGON_POINT_NUM;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, 1);
}

//绘制矩形裁剪窗口
static void drawWindow(GRE_Camera4d camera, GRE_FRECT window)
{
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window->x0, window->y0, window->x1, window->y0);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window->x1, window->y0, window->x1, window->y1);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window->x1, window->y1, window->x0, window->y1);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window->x0, window->y1, window->x0, window->y0);
}

//使用相同形状对照完整绘制与矩形窗口裁剪
static void drawClippingComparison(GRE_Camera4d camera)
{
	const float32 source[][2] = { { 15, 100 }, { 105, 15 }, { 235, 90 }, { 210, 235 }, { 40, 225 } };
	const float32 clipSource[][2] = { { 260, 100 }, { 350, 15 }, { 495, 90 }, { 475, 240 }, { 280, 225 } };
	gre_fline sourceLines[POLYGON_POINT_NUM];
	gre_flineslist input = { POLYGON_POINT_NUM, POLYGON_POINT_NUM, sourceLines };
	gre_line clippedLines[10];
	gre_lineslist output = { 10, clippedLines };
	gre_frect window = { 305, 55, 475, 225 };
	gre_fvector4d plane = { 0, 0, 1, -100 };
	GRErgb24 color = { 206, 154, 54 };

	//左侧按完整图像范围绘制
	drawSourcePolygon(camera, source, color);

	//右侧先裁剪到窗口，再填充裁剪后的闭合边集合
	for (uint16 i = 0; i < POLYGON_POINT_NUM; i++)
	{
		uint16 next = (i + 1) % POLYGON_POINT_NUM;
		sourceLines[i] = (gre_fline){ clipSource[i][0], clipSource[i][1],
			clipSource[next][0], clipSource[next][1] };
	}
	YMGRE_Polygon_clip2D(&input, &output, &window);
	YMGRE_Img_Scanline_AreaFill(camera->img.data, camera->img.width, camera->img.height,
		&output, &plane, camera->img.zbuff, color);
	for (uint16 i = 0; i < output.lineNum; i++)
	{
		YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
			output.data[i].x0, output.data[i].y0, output.data[i].x1, output.data[i].y1);
	}
	drawWindow(camera, &window);
}

//将裁剪结果交给 YMGUI 显示
static void showClippingImage(GRE_Camera4d camera)
{
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { target, 0, 0, target->width, target->height };
	YMGRE_DemoHost_Show(800, 600, &view, 1, 60);
}

int main(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, DEMO_WIDTH, DEMO_HEIGHT,
		45.0f, 45.0f, 45.0f, 45.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	YMGRE_CameraImage_Init(camera, (GRErgb24){ 42, 43, 47 });
	YMGRE_Img_SetBrushColor((GRErgb24){ 225, 229, 235 });

	drawClippingComparison(camera);
	showClippingImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
