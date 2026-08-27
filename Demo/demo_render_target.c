#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	GRE_Object4d cube = YMGRE_MeshGener_Cube(55, (GRErgb24){ 220, 90, 75 }, "target_cube", "");
	cube->WorldCoordinate = (gre_fvector4d){ 0, 0, 150, 1 };
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_List_Append(&objects, sizeof(gre_object4d), cube);
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_PointLight,
		(GRErgb24){ 255, 230, 190 }, 1.0f);
	light->pos = (gre_fvector4d){ 0, 35, 65, 1 };
	light->proper.kc1 = 0.002f;
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);

	GRE_RenderTarget leftTarget = YMGRE_Creat_RenderTarget(300, 300);
	GRE_RenderTarget rightTarget = YMGRE_Creat_RenderTarget(300, 300);
	GRE_Camera4d left = YMGRE_Creat_CameraFromTarget(0, leftTarget, 38, 38, 38, 38);
	GRE_Camera4d right = YMGRE_Creat_CameraFromTarget(1, rightTarget, 38, 38, 38, 38);
	YMGRE_Camera_Frustum_Init(left, 1, 500);
	YMGRE_Camera_Frustum_Init(right, 1, 500);
	gre_fvector4d leftPos = { -55, 35, -70, 1 };
	gre_fvector4d rightPos = { 55, 35, -70, 1 };
	gre_fvector4d target = { 0, 0, 150, 1 };
	YMGRE_UVNCamera_PositionInit(left, &leftPos, &target, NULL, 0);
	YMGRE_UVNCamera_PositionInit(right, &rightPos, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(left, &lights, &objects,
		&materials, workspace);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(right, &lights, &objects,
		&materials, workspace);

	YMGRE_DemoHost host;
	if (!YMGRE_DemoHost_Init(&host, 800, 300, 40))
		return 1;
	if (YMGRE_DemoHost_AddTarget(&host, leftTarget, 0, 0, 300, 300) == NULL ||
		YMGRE_DemoHost_AddTarget(&host, rightTarget, 500, 0, 300, 300) == NULL)
	{
		YMGRE_DemoHost_Destroy(&host);
		return 1;
	}
	YMGRE_DemoHost_Run(&host, 60);
	YMGRE_DemoHost_Destroy(&host);

	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(left);
	YMGRE_Free_Camera(right);
	YMGRE_Free_RenderTarget(leftTarget);
	YMGRE_Free_RenderTarget(rightTarget);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
