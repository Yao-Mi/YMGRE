#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"

static GRE_Object4d createPanel(const char* name, const char* materialName, float32 centerX)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2, (char*)name, (char*)materialName);
	object->pointList[0] = (gre_vertex4d){ { centerX - 62, -92, 180, 1 }, 0, 4 };
	object->pointList[1] = (gre_vertex4d){ { centerX + 62, -92, 180, 1 }, 4, 4 };
	object->pointList[2] = (gre_vertex4d){ { centerX + 62, 92, 180, 1 }, 4, 0 };
	object->pointList[3] = (gre_vertex4d){ { centerX - 62, 92, 180, 1 }, 0, 0 };
	uint16 indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
		for (int j = 0; j < 3; j++) polygon->index[j] = indices[i * 3 + j];
		polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
		polygon->planeColor = (GRErgb24){ 255, 255, 255 };
	}
	object->BoundingSphereR = 130;
	object->boundType = GRE_Bounding_Sphere_R;
	object->mirrorKs = 0.15f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createPanelMaterial(const char* name, uint8 mapped)
{
	GRE_Material material = YMGRE_Creat_Material((char*)name);
	material->ambient = (GRErgb24){ 105, 105, 105 };
	material->diffuse = (GRErgb24){ 235, 235, 235 };
	material->width = material->height = 8;
	material->pixel = GRE_ImageBuff_Malloc(64 * sizeof(GRErgb24));
	for (int i = 0; i < 64; i++) material->pixel[i] = (GRErgb24){ 170, 185, 195 };
	if (!mapped) return material;
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->normalWidth = material->advanced->normalHeight = 64;
	material->advanced->normalPixel = GRE_ImageBuff_Malloc(4096 * sizeof(GRErgb24));
	for (uint16 y = 0; y < 64; y++) for (uint16 x = 0; x < 64; x++)
	{
		float32 fx = ((x & 15) - 7.5f) / 7.5f;
		float32 fy = ((y & 15) - 7.5f) / 7.5f;
		float32 r2 = fx * fx + fy * fy;
		gre_fvector4d normal = { 0, 0, 1, 0 };
		if (r2 < 1.0f)
			normal = (gre_fvector4d){ fx * 0.72f, fy * 0.72f,
				YMGRE_Sqrt(GREMax(0.0f, 1.0f - r2 * 0.52f)), 0 };
		YMGRE_Fvector4d_Normalize(&normal);
		material->advanced->normalPixel[y * 64 + x] = (GRErgb24){
			(uint8)((normal.x * 0.5f + 0.5f) * 255),
			(uint8)((normal.y * 0.5f + 0.5f) * 255),
			(uint8)((normal.z * 0.5f + 0.5f) * 255) };
	}
	return material;
}

int main(void)
{
	gre_list objects = { 0 }, lights = { 0 }, materials = { 0 };
	/* The camera's screen X axis is opposite world X; use +X for the visual left panel. */
	YMGRE_List_Append(&objects, sizeof(gre_object4d), createPanel("flat", "flat_panel", 78));
	YMGRE_List_Append(&objects, sizeof(gre_object4d), createPanel("mapped", "mapped_panel", -78));
	YMGRE_List_Append(&materials, sizeof(gre_material), createPanelMaterial("flat_panel", 0));
	YMGRE_List_Append(&materials, sizeof(gre_material), createPanelMaterial("mapped_panel", 1));
	YMGRE_List_Append(&lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 130, 145, 160 }, 0.32f));
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 225, 185 }, 1.25f);
	point->pos = (gre_fvector4d){ 0, 70, 55, 1 };
	point->proper.kc1 = 0.002f;
	YMGRE_List_Append(&lights, sizeof(gre_light4d), point);
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 760, 460, 42, 42, 36, 36);
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
