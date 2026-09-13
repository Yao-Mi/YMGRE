#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

//创建基础形状场景
static void creatBasicScene(GRE_List objects, GRE_List lights)
{
	GRE_Object4d plane = YMGRE_MeshGener_RectPlane(36.0f, 36.0f, 4, 4,
		(GRErgb24){ 207, 174, 76 }, "plane", "");
	GRE_Object4d cube = YMGRE_MeshGener_Cube(28.0f,
		(GRErgb24){ 225, 92, 76 }, "cube", "");
	GRE_Object4d box = YMGRE_MeshGener_Box(38.0f, 25.0f, 22.0f,
		(GRErgb24){ 146, 105, 190 }, "box", "");
	GRE_Object4d cylinder = YMGRE_MeshGener_Cylinder(14.0f, 32.0f, 20,
		(GRErgb24){ 70, 166, 126 }, "cylinder", "");
	GRE_Object4d cone = YMGRE_MeshGener_Cone(17.0f, 36.0f, 20,
		(GRErgb24){ 218, 130, 63 }, "cone", "");
	GRE_Object4d sphere = YMGRE_MeshGener_Sphere(17.0f, 8, 16,
		(GRErgb24){ 74, 128, 210 }, "sphere", "");
	plane->WorldCoordinate = (gre_fvector4d){ -55.0f, 27.0f, 95.0f, 1.0f };
	cube->WorldCoordinate = (gre_fvector4d){ 0.0f, 27.0f, 95.0f, 1.0f };
	box->WorldCoordinate = (gre_fvector4d){ 55.0f, 27.0f, 95.0f, 1.0f };
	cylinder->WorldCoordinate = (gre_fvector4d){ -55.0f, -27.0f, 95.0f, 1.0f };
	cone->WorldCoordinate = (gre_fvector4d){ 0.0f, -27.0f, 95.0f, 1.0f };
	sphere->WorldCoordinate = (gre_fvector4d){ 55.0f, -27.0f, 95.0f, 1.0f };

	//变换到世界坐标并加入场景
	YMGRE_Object_LocalToWorld(plane);
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Object_LocalToWorld(box);
	YMGRE_Object_LocalToWorld(cylinder);
	YMGRE_Object_LocalToWorld(cone);
	YMGRE_Object_LocalToWorld(sphere);
	plane->wireFrame = 1;
	cube->wireFrame = 1;
	box->wireFrame = 1;
	cylinder->wireFrame = 1;
	cone->wireFrame = 1;
	sphere->wireFrame = 1;
	YMGRE_List_Append(objects, sizeof(gre_object4d), plane);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cube);
	YMGRE_List_Append(objects, sizeof(gre_object4d), box);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cylinder);
	YMGRE_List_Append(objects, sizeof(gre_object4d), cone);
	YMGRE_List_Append(objects, sizeof(gre_object4d), sphere);

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f);
	YMGRE_List_Append(lights, sizeof(gre_light4d), light);
}

//创建观察基础形状的相机
static GRE_Camera4d creatBasicCamera(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 500, 500, 38.0f, 38.0f, 38.0f, 38.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	gre_fvector4d cameraPos = { 0, 75, -65, 1 };
	gre_fvector4d targetPos = { 0, 0, 95, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &cameraPos, &targetPos, NULL, 0.0f);
	camera->wireFrame = GRE_Render_Solid;//实体表面，线框由模型标志控制
	return camera;
}

//将渲染结果交给 YMGUI 显示
static void showCameraImage(GRE_Camera4d camera)
{
	YMGRE_DemoHost host;
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(camera);
	if (!YMGRE_DemoHost_Init(&host, 800, 600, 40))
		return;
	if (YMGRE_DemoHost_AddTarget(&host, target, 0, 0,
		target->width, target->height) != NULL)
		YMGRE_DemoHost_Run(&host, 60);
	YMGRE_DemoHost_Destroy(&host);
}

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	creatBasicScene(&objects, &lights);
	GRE_Camera4d camera = creatBasicCamera();

	//使用独立工作区完成一次静态渲染
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera, &lights, &objects, &materials, workspace);
	showCameraImage(camera);

	//释放场景资源
	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
