#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_Mem.h"

static GRE_Material createCheckerMaterial(void)
{
	GRE_Material material = YMGRE_Creat_Material("checker");
	material->width = 8;
	material->height = 8;
	material->pixel = GRE_malloc1(material->width * material->height * sizeof(GRErgb24));
	for (uint16 y = 0; y < material->height; y++)
		for (uint16 x = 0; x < material->width; x++)
			material->pixel[y * material->width + x] =
				((x + y) & 1) ? (GRErgb24){ 232, 232, 232 } : (GRErgb24){ 45, 70, 105 };
	return material;
}

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	GRE_Object4d plane = YMGRE_MeshGener_RectPlane(130, 110, 8, 8,
		(GRErgb24){ 255, 255, 255 }, "textured_plane", "checker");
	plane->WorldCoordinate = (gre_fvector4d){ 0, 0, 140, 1 };
	YMGRE_Object_LocalToWorld(plane);
	YMGRE_List_Append(&objects, sizeof(gre_object4d), plane);
	GRE_Material material = createCheckerMaterial();
	YMGRE_List_Append(&materials, sizeof(gre_material), material);
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 500, 500, 42, 42, 42, 42);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d position = { 0, 80, -80, 1 };
	gre_fvector4d target = { 0, 0, 140, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &position, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera, &lights, &objects,
		&materials, workspace);
	GRE_RenderTarget targetView = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { targetView, 0, 0,
		targetView->width, targetView->height };
	YMGRE_DemoHost_Show(500, 500, &view, 1, 60);
	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&materials, YMGRE_Free_Material);
	return 0;
}
