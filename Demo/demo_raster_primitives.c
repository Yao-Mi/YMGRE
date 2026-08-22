#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define DEMO_WIDTH 900
#define DEMO_HEIGHT 300

static void linePanel(GRE_Camera4d camera)
{
	// 覆盖水平、竖直、斜向和交叉线段，观察端点是否重复或漏画。
	YMGRE_Img_SetBrushColor((GRErgb24){ 225, 229, 235 });
	const int16 lines[][4] = {
		{ 20, 30, 250, 30 }, { 20, 55, 250, 200 }, { 20, 200, 250, 55 },
		{ 135, 25, 135, 220 }, { 15, 215, 255, 215 }
	};
	for (uint16 i = 0; i < 5; i++)
		YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
			lines[i][0], lines[i][1], lines[i][2], lines[i][3]);
}

static void fillPanel(GRE_Camera4d camera)
{
	// 对照凸多边形和凹多边形的扫描线填充及轮廓线。
	gre_line edges[8];
	gre_lineslist ring = { 8, edges };
	gre_fvector4d plane = { 0, 0, 1, -100 };
	const int16 convex[][2] = { { 25, 30 }, { 115, 30 }, { 130, 105 }, { 70, 205 }, { 15, 105 } };
	const int16 concave[][2] = { { 155, 35 }, { 245, 35 }, { 210, 105 }, { 245, 195 }, { 155, 195 }, { 190, 105 } };
	const int16 (*shapes[])[2] = { convex, concave };
	const uint16 counts[] = { 5, 6 };
	const GRErgb24 colors[] = { { 70, 170, 125 }, { 210, 125, 65 } };
	for (uint16 shape = 0; shape < 2; shape++)
	{
		for (uint16 i = 0; i < counts[shape]; i++)
		{
			uint16 next = (i + 1) % counts[shape];
			edges[i] = (gre_line){
				shapes[shape][i][0], shapes[shape][i][1],
				shapes[shape][next][0], shapes[shape][next][1] };
		}
		ring.lineNum = counts[shape];
		YMGRE_Img_Scanline_AreaFill(camera->img.data, camera->img.width,
			camera->img.height, &ring, &plane, camera->img.zbuff, colors[shape]);
		YMGRE_Img_SetBrushColor((GRErgb24){ 235, 235, 235 });
		for (uint16 i = 0; i < counts[shape]; i++)
			YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
				edges[i].x0, edges[i].y0, edges[i].x1, edges[i].y1);
	}
}

static void clipPanel(GRE_Camera4d camera)
{
	// 先将超出窗口的五边形裁剪，再把裁剪边交给扫描线填充。
	gre_fline inputEdges[5];
	gre_flineslist input = { 5, 5, inputEdges };
	gre_line outputEdges[10];
	// Polygon_clip2D 以 lineNum 作为输出容量，并在返回时改写为实际边数。
	gre_lineslist output = { 10, outputEdges };
	gre_frect window = { 55, 45, 245, 215 };
	gre_fvector4d plane = { 0, 0, 1, -100 };
	const float32 points[][2] = { { 5, 115 }, { 85, 15 }, { 275, 55 }, { 220, 255 }, { 30, 230 } };
	for (uint16 i = 0; i < 5; i++)
	{
		uint16 next = (i + 1) % 5;
		inputEdges[i] = (gre_fline){ points[i][0], points[i][1], points[next][0], points[next][1] };
	}
	YMGRE_Polygon_clip2D(&input, &output, &window);
	YMGRE_Img_Scanline_AreaFill(camera->img.data, camera->img.width, camera->img.height,
		&output, &plane, camera->img.zbuff, (GRErgb24){ 80, 135, 210 });
	YMGRE_Img_SetBrushColor((GRErgb24){ 235, 235, 235 });
	for (uint16 i = 0; i < output.lineNum; i++)
		YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
			outputEdges[i].x0, outputEdges[i].y0, outputEdges[i].x1, outputEdges[i].y1);
	YMGRE_Img_SetBrushColor((GRErgb24){ 235, 190, 65 });
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window.x0, window.y0, window.x1, window.y0);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window.x1, window.y0, window.x1, window.y1);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window.x1, window.y1, window.x0, window.y1);
	YMGRE_Img_Line(camera->img.data, camera->img.width, camera->img.height,
		window.x0, window.y1, window.x0, window.y0);
}

int main(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 300, DEMO_HEIGHT, 45, 45, 45, 45);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	YMGRE_CameraImage_Init(camera, (GRErgb24){ 42, 43, 47 });
	linePanel(camera);
	GRE_Camera4d fillCamera = YMGRE_Creat_Camera(1, 300, DEMO_HEIGHT, 45, 45, 45, 45);
	YMGRE_Camera_Frustum_Init(fillCamera, 1, 500);
	YMGRE_CameraImage_Init(fillCamera, (GRErgb24){ 42, 43, 47 });
	fillPanel(fillCamera);
	GRE_Camera4d clipCamera = YMGRE_Creat_Camera(2, 300, DEMO_HEIGHT, 45, 45, 45, 45);
	YMGRE_Camera_Frustum_Init(clipCamera, 1, 500);
	YMGRE_CameraImage_Init(clipCamera, (GRErgb24){ 42, 43, 47 });
	clipPanel(clipCamera);
	LCD_Init(DEMO_WIDTH, DEMO_HEIGHT);
	LCD_Fill_RgbRect(0, 0, 300, DEMO_HEIGHT, camera->img.data);
	LCD_Fill_RgbRect(300, 0, 300, DEMO_HEIGHT, fillCamera->img.data);
	LCD_Fill_RgbRect(600, 0, 300, DEMO_HEIGHT, clipCamera->img.data);
	while (LCD_Update(60)) { }
	LCD_Destory();
	YMGRE_Free_Camera(camera);
	YMGRE_Free_Camera(fillCamera);
	YMGRE_Free_Camera(clipCamera);
	return 0;
}
