#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

int main(void)
{
	gre_list objects = { 0 }, lights = { 0 }, materials = { 0 };
	GRE_Object4d cube = YMGRE_MeshGener_Cube(105, (GRErgb24){ 210, 210, 210 }, "advanced_cube", "cube_mat");
	cube->WorldCoordinate = (gre_fvector4d){ 0, 0, 180, 1 };
	cube->renderMode = GRE_RenderMode_Face;
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Object_GenerateVertexAttributes(cube);
	for (int i = 0; i < cube->pointNum; i++) cube->pointList_wN[i].color = (GRErgb24){ 235, 235, 235 };
	YMGRE_List_Append(&objects, sizeof(gre_object4d), cube);
	GRE_Material material = YMGRE_Creat_Material("cube_mat");
	material->ambient = (GRErgb24){ 55, 55, 55 }; material->diffuse = (GRErgb24){ 215, 215, 215 };
	material->width = material->height = 1; material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24)); material->pixel[0] = (GRErgb24){ 215, 215, 215 };
	YMGRE_List_Append(&materials, sizeof(gre_material), material);
	GRE_Light4d point = YMGRE_Creat_Light(0, GRE_PointLight, (GRErgb24){ 255, 245, 225 }, 1.6f);
	point->pos = (gre_fvector4d){ -95, 125, 75, 1 }; point->proper.kc1 = 0.001f; point->proper.shadowK = 0.04f;
	YMGRE_List_Append(&lights, sizeof(gre_light4d), point);
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 640, 520, 42, 42, 38, 38);
	YMGRE_Camera_Frustum_Init(camera, 1, 600);
	gre_fvector4d eye = { 0, 70, -95, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(camera, &lights, &objects, &materials, workspace);
	GRE_RenderTarget output = YMGRE_Camera_GetRenderTarget(camera); YMGRE_DemoView view = { output, 0, 0, output->width, output->height };
	YMGRE_DemoHost_Show(output->width, output->height, &view, 1, 60);
	YMGRE_Free_RenderWorkspace(workspace); YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light); YMGRE_List_Clear(&objects, YMGRE_Free_Object); YMGRE_List_Clear(&materials, YMGRE_Free_Material); return 0;
}
