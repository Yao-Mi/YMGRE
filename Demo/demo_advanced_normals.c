#include "demo_host.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

static GRE_Camera4d createCamera(int16 id)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(id, 360, 420, 38, 38, 40, 40);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d eye = { 0, 45, -70, 1 }, target = { 0, 0, 160, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	return camera;
}

int main(void)
{
	gre_list objects = { 0 }, lights = { 0 }, materials = { 0 };
	GRE_Object4d cylinder = YMGRE_MeshGener_Cylinder(62, 125, 12,
		(GRErgb24){ 215, 225, 235 }, "normal_cylinder", "solid");
	cylinder->WorldCoordinate = (gre_fvector4d){ 0, 0, 160, 1 };
	cylinder->mirrorKs = 0.25f;
	YMGRE_Object_LocalToWorld(cylinder);
	YMGRE_Object_GenerateVertexAttributes(cylinder);
	cylinder->renderMode = GRE_RenderMode_Vertex;
	YMGRE_List_Append(&objects, sizeof(gre_object4d), cylinder);
	GRE_Material solid = YMGRE_Creat_Material("solid");
	solid->width = solid->height = 1;
	solid->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	solid->pixel[0] = (GRErgb24){ 215, 225, 235 };
	YMGRE_List_Append(&materials, sizeof(gre_material), solid);
	GRE_Light4d ambient = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 145, 155, 170 }, 0.38f);
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 220, 175 }, 1.1f);
	point->pos = (gre_fvector4d){ -70, 75, 45, 1 };
	point->proper.kc1 = 0.0025f;
	YMGRE_List_Append(&lights, sizeof(gre_light4d), ambient);
	YMGRE_List_Append(&lights, sizeof(gre_light4d), point);
	GRE_Camera4d flatCamera = createCamera(0);
	GRE_Camera4d smoothCamera = createCamera(1);
	GRE_RenderWorkspace flatWorkspace = YMGRE_Creat_RenderWorkspace();
	GRE_RenderWorkspace smoothWorkspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(flatCamera, &lights, &objects,
		&materials, flatWorkspace);
	YMGRE_Camera_TanglePipline_wN(smoothCamera, &lights, &objects, &materials, smoothWorkspace);
	YMGRE_DemoView views[2] = {
		{ YMGRE_Camera_GetRenderTarget(flatCamera), 0, 0, 360, 420 },
		{ YMGRE_Camera_GetRenderTarget(smoothCamera), 360, 0, 360, 420 }
	};
	YMGRE_DemoHost_Show(720, 420, views, 2, 60);
	YMGRE_Free_RenderWorkspace(flatWorkspace);
	YMGRE_Free_RenderWorkspace(smoothWorkspace);
	YMGRE_Free_Camera(flatCamera); YMGRE_Free_Camera(smoothCamera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&materials, YMGRE_Free_Material);
	return 0;
}
