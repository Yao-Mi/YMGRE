#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 490
#define PANEL_WIDTH 140
#define PANEL_HEIGHT 200

//绘制一个小视口的边框，边框对应完整视景体投影范围
static void drawPanelFrame(GRE_Camera4d camera, int16 x, int16 y)
{
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x, y, x + PANEL_WIDTH, y);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x + PANEL_WIDTH, y, x + PANEL_WIDTH, y + PANEL_HEIGHT);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x + PANEL_WIDTH, y + PANEL_HEIGHT, x, y + PANEL_HEIGHT);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		x, y + PANEL_HEIGHT, x, y);
}

//将裁剪后的相机空间多边形投影到指定小视口
static void drawClippedPolygon(GRE_Camera4d camera, const gre_vertex4d source[], uint16 sourceNum,
	int16 panelX, int16 panelY, GRErgb24 color)
{
	gre_vertex4d clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	uint16 clippedNum = YMGRE_Polygon_FrustumClip((GRE_Vertex4d)source, sourceNum, clipped,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera);
	if (clippedNum == 0)
		return;

	float32 viewWidth = camera->perspectPlane.pR - camera->perspectPlane.pL;
	float32 viewHeight = camera->perspectPlane.pU - camera->perspectPlane.pD;
	for (uint16 i = 0; i < clippedNum; i++)
	{
		float32 projectScale = camera->perspectPlane.Dis / clipped[i].pos.z;
		float32 viewX = clipped[i].pos.x * projectScale;
		float32 viewY = clipped[i].pos.y * projectScale;
		clipped[i].pos.x = panelX + (viewX - camera->perspectPlane.pL) * PANEL_WIDTH / viewWidth;
		clipped[i].pos.y = panelY + (camera->perspectPlane.pU - viewY) * PANEL_HEIGHT / viewHeight;
	}

	uint16 index[YMGRE_FRUSTUM_CLIP_VERTEX_MAX] = { 0 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };
	for (uint16 i = 0; i < clippedNum; i++)
		index[i] = i;
	polygon.num = clippedNum;
	polygon.index = index;
	object.pointNum = clippedNum;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, clipped, polygonHide,
		polygonColor, camera, 1);
}

//依次展示完全可见、近远平面、左右上侧面和相机后方裁剪
static void drawFrustumCases(GRE_Camera4d camera)
{
	const gre_vertex4d visible[] = {
		{ { -55, -45, 100, 1 }, 0, 0 }, { { 55, -45, 100, 1 }, 1, 0 },
		{ { 0, 60, 100, 1 }, 0.5f, 1 }
	};
	const gre_vertex4d nearCrossing[] = {
		{ { -60, -50, 100, 1 }, 0, 0 }, { { 60, -50, 100, 1 }, 1, 0 },
		{ { 0, 10, 20, 1 }, 0.5f, 1 }
	};
	const gre_vertex4d farCrossing[] = {
		{ { -60, -50, 100, 1 }, 0, 0 }, { { 60, -50, 100, 1 }, 1, 0 },
		{ { 0, 60, 190, 1 }, 0.5f, 1 }
	};
	const gre_vertex4d leftCrossing[] = {
		{ { -150, 0, 100, 1 }, 0, 0 }, { { 40, -60, 100, 1 }, 1, 0 },
		{ { 40, 60, 100, 1 }, 0.5f, 1 }
	};
	const gre_vertex4d topCrossing[] = {
		{ { -60, -30, 100, 1 }, 0, 0 }, { { 60, -30, 100, 1 }, 1, 0 },
		{ { 0, 150, 100, 1 }, 0.5f, 1 }
	};
	const gre_vertex4d behindCamera[] = {
		{ { -30, -30, -20, 1 }, 0, 0 }, { { 30, -30, -20, 1 }, 1, 0 },
		{ { 0, 30, -20, 1 }, 0.5f, 1 }
	};
	const int16 panel[][2] = { { 10, 15 }, { 180, 15 }, { 350, 15 },
		{ 10, 260 }, { 180, 260 }, { 350, 260 } };

	drawClippedPolygon(camera, visible, 3, panel[0][0], panel[0][1], (GRErgb24){ 68, 166, 126 });
	drawClippedPolygon(camera, nearCrossing, 3, panel[1][0], panel[1][1], (GRErgb24){ 220, 86, 74 });
	drawClippedPolygon(camera, farCrossing, 3, panel[2][0], panel[2][1], (GRErgb24){ 72, 128, 210 });
	drawClippedPolygon(camera, leftCrossing, 3, panel[3][0], panel[3][1], (GRErgb24){ 206, 154, 54 });
	drawClippedPolygon(camera, topCrossing, 3, panel[4][0], panel[4][1], (GRErgb24){ 151, 103, 190 });
	drawClippedPolygon(camera, behindCamera, 3, panel[5][0], panel[5][1], (GRErgb24){ 200, 200, 200 });
	for (uint16 i = 0; i < 6; i++)
		drawPanelFrame(camera, panel[i][0], panel[i][1]);
}

//将视景体裁剪结果交给 YMGUI 显示
static void showFrustumImage(GRE_Camera4d camera)
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
	YMGRE_Camera_Frustum_Init(camera, 40.0f, 150.0f);
	YMGRE_CameraImage_Init(camera, (GRErgb24){ 42, 43, 47 });
	YMGRE_Img_SetBrushColor((GRErgb24){ 225, 229, 235 });

	drawFrustumCases(camera);
	showFrustumImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
