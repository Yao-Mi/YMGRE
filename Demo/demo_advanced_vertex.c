#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

static GRE_Object4d createColorTriangle(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(3, 1, "vertex_color", "white");
	object->pointList[0] = (gre_vertex4d){ { -105, -78, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 105, -78, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 0, 105, 180, 1 }, 0.5f, 0 };
	object->polygonList[0].num = 3;
	object->polygonList[0].index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
	object->polygonList[0].index[0] = 0;
	object->polygonList[0].index[1] = 2;
	object->polygonList[0].index[2] = 1;
	object->polygonList[0].pN = (gre_fvector4d){ 0, 0, -1, 0 };
	object->polygonList[0].planeColor = (GRErgb24){ 255, 255, 255 };
	object->BoundingSphereR = 150;
	object->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Vertex;
	object->pointList_wN[0].color = (GRErgb24){ 255, 35, 35 };
	object->pointList_wN[1].color = (GRErgb24){ 35, 255, 70 };
	object->pointList_wN[2].color = (GRErgb24){ 40, 90, 255 };
	return object;
}

int main(void)
{
	gre_list objects = { 0 };
	YMGRE_List_Append(&objects, sizeof(gre_object4d), createColorTriangle());
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 560, 460, 40, 40, 40, 40);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_VertexColor_wN(camera,&objects,workspace);
	GRE_RenderTarget output = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { output, 0, 0, output->width, output->height };
	YMGRE_DemoHost_Show(output->width, output->height, &view, 1, 60);
	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
