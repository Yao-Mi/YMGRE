#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

//创建不同视口相机共享的静态场景
static void creatViewportScene(GRE_List objects, GRE_List lights)
{
	GRE_Object4d cube = YMGRE_MeshGener_Cube(34.0f, (GRErgb24){ 220, 86, 74 }, "cube", "");
	GRE_Object4d cylinder = YMGRE_MeshGener_Cylinder(17.0f, 44.0f, 20,
		(GRErgb24){ 68, 166, 126 }, "cylinder", "");
	GRE_Object4d sphere = YMGRE_MeshGener_Sphere(20.0f, 8, 16,
		(GRErgb24){ 72, 128, 210 }, "sphere", "");
	cube->WorldCoordinate = (gre_fvector4d){ -45.0f, 0.0f, 100.0f, 1.0f };
	cylinder->WorldCoordinate = (gre_fvector4d){ 0.0f, 0.0f, 100.0f, 1.0f };
	sphere->WorldCoordinate = (gre_fvector4d){ 45.0f, 0.0f, 100.0f, 1.0f };
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Object_LocalToWorld(cylinder);
	YMGRE_Object_LocalToWorld(sphere);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cube);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cylinder);
	YMGRE_List_Append(objects, sizeof(gre_object4d), sphere);

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	YMGRE_List_Append(lights, sizeof(gre_light4d), light);
}

//创建五百乘三百的宽视口主相机
static GRE_Camera4d creatWideCamera(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 500, 300,
		52.0f, 52.0f, 32.0f, 32.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	gre_fvector4d position = { -65, 35, -100, 1 };
	gre_fvector4d target = { 0, 0, 100, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &position, &target, NULL, 0.0f);
	return camera;
}

//创建二百四十乘三百的窄视口辅助相机
static GRE_Camera4d creatPortraitCamera(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(1, 240, 300,
		28.0f, 28.0f, 48.0f, 48.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	gre_fvector4d position = { 70, 55, -75, 1 };
	gre_fvector4d target = { 0, 0, 100, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &position, &target, NULL, 0.0f);
	return camera;
}

//将不同尺寸的两个相机目标交给 YMGUI 显示
static void showViewportImages(GRE_Camera4d wide, GRE_Camera4d portrait)
{
	LCD_Init(800, 600);
	LCD_Fill_RgbRect(0, 0, wide->img.width, wide->img.height, wide->img.data);
	LCD_Fill_RgbRect(500, 0, portrait->img.width, portrait->img.height, portrait->img.data);
	while (LCD_Update(60))
	{
	}
	LCD_Destory();
}

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	creatViewportScene(&objects, &lights);
	GRE_Camera4d wide = creatWideCamera();
	GRE_Camera4d portrait = creatPortraitCamera();
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();

	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(wide,
		&lights, &objects, &materials, workspace);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(portrait,
		&lights, &objects, &materials, workspace);
	showViewportImages(wide, portrait);

	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(wide);
	YMGRE_Free_Camera(portrait);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
