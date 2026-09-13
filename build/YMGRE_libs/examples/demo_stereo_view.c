#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

//创建双目相机共享的静态场景
static void creatStereoScene(GRE_List objects, GRE_List lights)
{
	GRE_Object4d cube = YMGRE_MeshGener_Cube(40.0f, (GRErgb24){ 220, 96, 72 }, "cube", "");
	GRE_Object4d cylinder = YMGRE_MeshGener_Cylinder(17.0f, 46.0f, 24,
		(GRErgb24){ 70, 166, 126 }, "cylinder", "");
	cube->WorldCoordinate = (gre_fvector4d){ -24.0f, 0.0f, 90.0f, 1.0f };
	cylinder->WorldCoordinate = (gre_fvector4d){ 28.0f, 0.0f, 94.0f, 1.0f };

	//变换到世界坐标并加入场景
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Object_LocalToWorld(cylinder);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cube);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cylinder);

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f);
	YMGRE_List_Append(lights, sizeof(gre_light4d), light);
}

//创建一个双目观察相机
static GRE_Camera4d creatStereoCamera(int16 id, float32 x)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(id, 300, 300, 36.0f, 36.0f, 36.0f, 36.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	gre_fvector4d cameraPos = { x, 20, -90, 1 };
	gre_fvector4d targetPos = { 0, 0, 90, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &cameraPos, &targetPos, NULL, 0.0f);
	return camera;
}

//并排显示两个相机的独立画面
static void showStereoImage(GRE_Camera4d left, GRE_Camera4d right)
{
	YMGRE_DemoHost host;
	GRE_RenderTarget leftTarget = YMGRE_Camera_GetRenderTarget(left);
	GRE_RenderTarget rightTarget = YMGRE_Camera_GetRenderTarget(right);
	if (!YMGRE_DemoHost_Init(&host, 800, 600, 40))
		return;
	if (YMGRE_DemoHost_AddTarget(&host, leftTarget, 0, 0,
		leftTarget->width, leftTarget->height) != NULL &&
		YMGRE_DemoHost_AddTarget(&host, rightTarget, 500, 0,
		rightTarget->width, rightTarget->height) != NULL)
		YMGRE_DemoHost_Run(&host, 60);
	YMGRE_DemoHost_Destroy(&host);
}

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	creatStereoScene(&objects, &lights);

	//两个相机保留独立 Target，顺序共享 Workspace
	GRE_Camera4d left = creatStereoCamera(0, -10.0f);
	GRE_Camera4d right = creatStereoCamera(1, 10.0f);

	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(left, &lights, &objects, &materials, workspace);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(right, &lights, &objects, &materials, workspace);
	showStereoImage(left, right);

	//释放场景资源
	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(left);
	YMGRE_Free_Camera(right);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
