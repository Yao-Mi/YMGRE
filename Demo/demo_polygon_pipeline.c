#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	GRE_Object4d object = YMGRE_MeshGener_Cube(70, (GRErgb24){ 210, 145, 70 },
		"polygon_pipeline_cube", "");
	object->WorldCoordinate = (gre_fvector4d){ 0, 0, 150, 1 };
	YMGRE_Object_LocalToWorld(object);
	YMGRE_List_Append(&objects, sizeof(gre_object4d), object);
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_PointLight,
		(GRErgb24){ 255, 230, 190 }, 1.0f);
	light->pos = (gre_fvector4d){ 0, 45, 65, 1 };
	light->proper.kc1 = 0.002f;
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 500, 500, 40, 40, 40, 40);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d position = { 0, 70, -85, 1 };
	gre_fvector4d target = { 0, 0, 150, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &position, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(camera, &lights, &objects,
		&materials, workspace);
	GRE_RenderTarget targetView = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { targetView, 0, 0,
		targetView->width, targetView->height };
	YMGRE_DemoHost_Show(500, 500, &view, 1, 60);
	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
