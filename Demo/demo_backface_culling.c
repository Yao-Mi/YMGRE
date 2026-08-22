#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 500
#define DEMO_HEIGHT 490
#define PANEL_WIDTH 200
#define PANEL_HEIGHT 140
#define POLYGON_POINT_NUM 5

//绘制面板边框，剔除后的空面板仍保留观察位置
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

//根据顶点绕序计算法向，并按开关选择是否执行背面剔除
static void drawPolygonCase(GRE_Camera4d camera, const float32 point[][2], const uint16 sourceIndex[],
	float32 offsetX, float32 offsetY, GRErgb24 color, uint8 enableCulling)
{
	gre_vertex4d vertex[POLYGON_POINT_NUM] = { 0 };
	uint16 index[POLYGON_POINT_NUM] = { 0 };
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[1] = { 0 };
	GRErgb24 polygonColor[1] = { color };
	gre_fvector4d cameraPosition = { 0, 0, 0, 1 };

	for (uint16 i = 0; i < POLYGON_POINT_NUM; i++)
	{
		vertex[i].pos = (gre_fvector4d){ point[i][0] + offsetX,
			point[i][1] + offsetY, 100.0f, 1.0f };
		index[i] = sourceIndex[i];
	}
	gre_fvector4d u;
	gre_fvector4d v;
	YMGRE_Fvector4d_SubToResult(&vertex[index[1]].pos, &vertex[index[0]].pos, &u);
	YMGRE_Fvector4d_SubToResult(&vertex[index[2]].pos, &vertex[index[0]].pos, &v);
	YMGRE_Fvector4d_CrossToResult(&u, &v, &polygon.pN);
	polygon.num = POLYGON_POINT_NUM;
	polygon.index = index;
	object.pointNum = POLYGON_POINT_NUM;
	object.pointList = vertex;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	if (enableCulling)
		YMGRE_Backface_RemoveTo(&object, &cameraPosition, polygonHide);
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertex, polygonHide,
		polygonColor, camera, 1);
}

//正向、反向和镜像修正三种绕序，左侧关闭剔除，右侧开启剔除
static void drawCullingComparison(GRE_Camera4d camera)
{
	const float32 point[][2] = {
		{ 35, 110 }, { 25, 45 }, { 80, 15 }, { 160, 35 }, { 150, 115 }
	};
	const float32 mirroredPoint[][2] = {
		{ 165, 110 }, { 175, 45 }, { 120, 15 }, { 40, 35 }, { 50, 115 }
	};
	const uint16 frontIndex[] = { 4, 3, 2, 1, 0 };
	const uint16 backIndex[] = { 0, 1, 2, 3, 4 };
	const int16 panel[][2] = {
		{ 15, 10 }, { 285, 10 }, { 15, 175 }, { 285, 175 }, { 15, 340 }, { 285, 340 }
	};

	//正向绕序：开启剔除后仍可见
	drawPolygonCase(camera, point, frontIndex, panel[0][0], panel[0][1],
		(GRErgb24){ 68, 166, 126 }, 0);
	drawPolygonCase(camera, point, frontIndex, panel[1][0], panel[1][1],
		(GRErgb24){ 68, 166, 126 }, 1);
	//反向绕序：关闭剔除时可见，开启后隐藏
	drawPolygonCase(camera, point, backIndex, panel[2][0], panel[2][1],
		(GRErgb24){ 220, 86, 74 }, 0);
	drawPolygonCase(camera, point, backIndex, panel[3][0], panel[3][1],
		(GRErgb24){ 220, 86, 74 }, 1);
	//镜像会翻转法向，反转索引后恢复正向
	drawPolygonCase(camera, mirroredPoint, backIndex, panel[4][0], panel[4][1],
		(GRErgb24){ 72, 128, 210 }, 0);
	drawPolygonCase(camera, mirroredPoint, backIndex, panel[5][0], panel[5][1],
		(GRErgb24){ 72, 128, 210 }, 1);
	for (uint16 i = 0; i < 6; i++)
		drawPanelFrame(camera, panel[i][0], panel[i][1]);
}

//将背面剔除结果交给 YMGUI 显示
static void showCullingImage(GRE_Camera4d camera)
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

	drawCullingComparison(camera);
	showCullingImage(camera);

	YMGRE_Free_Camera(camera);
	return 0;
}
