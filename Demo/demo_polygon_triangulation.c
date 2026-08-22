#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_PolygonTriangulation.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define PANEL_W 450
#define PANEL_H 300

static void drawPolygon(GRE_Camera4d camera, const gre_fvector4d* points, uint16 pointNum,
	const uint16* indices, uint16 triangleNum, GRErgb24 color, uint8 wire)
{
	// 将剖分输出的索引逐个包装成三角形，复用现有多边形光栅化入口。
	gre_vertex4d vertices[16] = { 0 };
	uint16 polyIndices[3];
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 hidden = 0;
	GRErgb24 polygonColor = color;
	for (uint16 i = 0; i < pointNum; i++) vertices[i].pos = points[i];
	object.pointNum = pointNum;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	for (uint16 i = 0; i < triangleNum; i++)
	{
		polyIndices[0] = indices[i * 3];
		polyIndices[1] = indices[i * 3 + 1];
		polyIndices[2] = indices[i * 3 + 2];
		polygon.num = 3;
		polygon.index = polyIndices;
		YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertices, &hidden,
			&polygonColor, camera, wire);
	}
}

static void drawOriginal(GRE_Camera4d camera, const gre_fvector4d* points, uint16 pointNum,
	GRErgb24 color)
{
	// 左侧保留原始多边形，作为三角剖分结果的轮廓对照。
	gre_vertex4d vertices[16] = { 0 };
	uint16 indices[16];
	gre_polygon4d polygon = { 0 };
	gre_object4d object = { 0 };
	uint8 hidden = 0;
	GRErgb24 polygonColor = color;
	for (uint16 i = 0; i < pointNum; i++) { vertices[i].pos = points[i]; indices[i] = i; }
	polygon.num = pointNum;
	polygon.index = indices;
	object.pointNum = pointNum;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	YMGRE_PolygonObject_Primitive_RasterizationTo(&object, vertices, &hidden,
		&polygonColor, camera, 1);
}

int main(void)
{
	GRE_Camera4d left = YMGRE_Creat_Camera(0, PANEL_W, PANEL_H, 45, 45, 45, 45);
	GRE_Camera4d right = YMGRE_Creat_Camera(1, PANEL_W, PANEL_H, 45, 45, 45, 45);
	YMGRE_Camera_Frustum_Init(left, 1, 500);
	YMGRE_Camera_Frustum_Init(right, 1, 500);
	YMGRE_CameraImage_Init(left, (GRErgb24){ 42, 43, 47 });
	YMGRE_CameraImage_Init(right, (GRErgb24){ 42, 43, 47 });
	const gre_fvector4d points[] = {
		{ 40, 220, 100, 1 }, { 85, 55, 100, 1 }, { 190, 95, 100, 1 },
		{ 145, 130, 100, 1 }, { 205, 235, 100, 1 }, { 80, 250, 100, 1 }
	};
	uint16 triangles[12];
	// 六边形应生成 vertexNum - 2 个三角形。
	uint16 triangleNum = YMGRE_Polygon_Triangulate(points, 6, triangles, 4);
	drawOriginal(left, points, 6, (GRErgb24){ 75, 155, 215 });
	drawPolygon(right, points, 6, triangles, triangleNum, (GRErgb24){ 75, 155, 215 }, 1);
	LCD_Init(PANEL_W * 2, PANEL_H);
	LCD_Fill_RgbRect(0, 0, PANEL_W, PANEL_H, left->img.data);
	LCD_Fill_RgbRect(PANEL_W, 0, PANEL_W, PANEL_H, right->img.data);
	while (LCD_Update(60)) { }
	LCD_Destory();
	YMGRE_Free_Camera(left);
	YMGRE_Free_Camera(right);
	return triangleNum == 4 ? 0 : 1;
}
