#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

static GRE_Object4d createTiltedQuad(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2, "perspective_quad", "checker");
	gre_vertex4d vertices[4] = {
		{ { -115, -90, 105, 1 }, 0, 1 }, { { 105, -90, 285, 1 }, 1, 1 },
		{ { 105, 90, 285, 1 }, 1, 0 }, { { -115, 90, 105, 1 }, 0, 0 }
	};
	for (int i = 0; i < 4; i++) object->pointList[i] = vertices[i];
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
		for (int j = 0; j < 3; j++) polygon->index[j] = indices[i * 3 + j];
		polygon->pN = (gre_fvector4d){ -180, 0, -220, 0 };
		polygon->planeColor = (GRErgb24){ 255, 255, 255 };
	}
	object->BoundingSphereR = 220;
	object->boundType = GRE_Bounding_Sphere_R;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createChecker(void)
{
	GRE_Material material = YMGRE_Creat_Material("checker");
	material->width = material->height = 16;
	material->pixel = GRE_ImageBuff_Malloc(256 * sizeof(GRErgb24));
	for (uint16 y = 0; y < 16; y++) for (uint16 x = 0; x < 16; x++)
		material->pixel[y * 16 + x] = ((x + y) & 1) ?
			(GRErgb24){ 238, 238, 238 } : (GRErgb24){ 28, 42, 58 };
	return material;
}

int main(void)
{
	gre_list objects = { 0 }, lights = { 0 }, materials = { 0 };
	YMGRE_List_Append(&objects, sizeof(gre_object4d), createTiltedQuad());
	YMGRE_List_Append(&materials, sizeof(gre_material), createChecker());
	YMGRE_List_Append(&lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f));
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 700, 440, 42, 42, 38, 38);
	YMGRE_Camera_Frustum_Init(camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(camera, &lights, &objects, &materials, workspace);
	GRE_RenderTarget output = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { output, 0, 0, output->width, output->height };
	YMGRE_DemoHost_Show(output->width, output->height, &view, 1, 60);
	YMGRE_Free_RenderWorkspace(workspace); YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&materials, YMGRE_Free_Material);
	return 0;
}
