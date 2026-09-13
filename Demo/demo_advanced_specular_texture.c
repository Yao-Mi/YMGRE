#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

#define STAGE_COUNT 3
#define PANEL_W 300
#define PANEL_H 340

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} TextureSpecularStage;

static GRE_Object4d createPanel(int stage)
{
	GRE_Object4d object = YMGRE_Creat_Object(4, 2,
		"specular_texture_panel", "specular_texture_material");
	object->pointList[0] = (gre_vertex4d){ { -125, -105, 180, 1 }, 0, 1 };
	object->pointList[1] = (gre_vertex4d){ { 125, -105, 180, 1 }, 1, 1 };
	object->pointList[2] = (gre_vertex4d){ { 125, 105, 180, 1 }, 1, 0 };
	object->pointList[3] = (gre_vertex4d){ { -125, 105, 180, 1 }, 0, 0 };
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		polygon->num = 3;
		polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
		for (int j = 0; j < 3; j++) polygon->index[j] = indices[i * 3 + j];
		polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
		polygon->planeColor = (GRErgb24){ 255, 255, 255 };
	}
	object->BoundingSphereR = 170;
	object->boundType = GRE_Bounding_Sphere_R;
	object->mirrorKs = stage == 0 ? 0.0f : 0.9f;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->renderMode = GRE_RenderMode_Pixel;
	return object;
}

static GRE_Material createMaterial(int stage)
{
	GRE_Material material = YMGRE_Creat_Material("specular_texture_material");
	material->ambient = (GRErgb24){ 100, 100, 100 };
	material->diffuse = (GRErgb24){ 180, 180, 180 };
	material->specular = stage == 2 ?
		(GRErgb24){ 45, 90, 255 } : (GRErgb24){ 255, 255, 255 };
	material->width = material->height = 8;
	material->pixel = GRE_ImageBuff_Malloc(64 * sizeof(GRErgb24));
	for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
		material->pixel[y * 8 + x] = (((x / 2) + (y / 2)) & 1) ?
			(GRErgb24){ 255, 255, 255 } : (GRErgb24){ 0, 0, 0 };
	return material;
}

static void initStage(TextureSpecularStage* stage, int index)
{
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d), createPanel(index));
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createMaterial(index));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 0.2f));
	GRE_Light4d point = YMGRE_Creat_Light(1, GRE_PointLight,
		(GRErgb24){ 255, 245, 225 }, 1.0f);
	point->pos = (gre_fvector4d){ 0, 0, 55, 1 };
	point->proper.kc0 = 1.0f;
	point->proper.kc1 = 0.001f;
	point->proper.kc2 = 0.0f;
	point->proper.shadowK = 0.0f;
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), point);
	stage->camera = YMGRE_Creat_Camera(index, PANEL_W, PANEL_H, 42, 42, 40, 40);
	YMGRE_Camera_Frustum_Init(stage->camera, 1, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static void destroyStage(TextureSpecularStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	TextureSpecularStage stages[STAGE_COUNT] = { 0 };
	YMGRE_DemoView views[STAGE_COUNT];
	for (int i = 0; i < STAGE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			i * PANEL_W, 0, PANEL_W, PANEL_H };
	}
	YMGRE_DemoHost_Show(STAGE_COUNT * PANEL_W, PANEL_H, views, STAGE_COUNT, 60);
	for (int i = 0; i < STAGE_COUNT; i++) destroyStage(&stages[i]);
	return 0;
}
