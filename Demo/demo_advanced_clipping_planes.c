#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

#define PLANE_COUNT 6
#define COLUMNS 3
#define PANEL_W 240
#define PANEL_H 220

typedef struct
{
	gre_list objects;
	gre_list lights;
	gre_list materials;
	GRE_Camera4d camera;
	GRE_RenderWorkspace workspace;
} ClipStage;

static GRE_Object4d createTriangle(int plane, const char* materialName)
{
	GRE_Object4d object = YMGRE_Creat_Object(3, 1, "clip_plane_triangle",
		(char*)materialName);
	gre_fvector4d positions[3] = {
		{ -95, -70, 180, 1 }, { 95, -70, 180, 1 }, { 0, 105, 180, 1 }
	};
	switch (plane)
	{
	case 0: positions[2].z = 60; break;
	case 1: positions[2].z = 340; break;
	case 2: positions[0].x = -245; break;
	case 3: positions[1].x = 245; break;
	case 4: positions[2].y = 235; break;
	default: positions[0].y = positions[1].y = -235; break;
	}
	object->pointList[0] = (gre_vertex4d){ positions[0], 0, 1 };
	object->pointList[1] = (gre_vertex4d){ positions[1], 1, 1 };
	object->pointList[2] = (gre_vertex4d){ positions[2], .5f, 0 };
	GRE_Polygon4d polygon = &object->polygonList[0];
	polygon->num = 3;
	polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
	polygon->index[0] = 0; polygon->index[1] = 2; polygon->index[2] = 1;
	polygon->pN = (gre_fvector4d){ 0, 0, -1, 0 };
	polygon->planeColor = (GRErgb24){ 255, 255, 255 };
	object->BoundingSphereR = 400;
	object->boundType = GRE_Bounding_Sphere_R;
	object->renderMode = GRE_RenderMode_Pixel;
	YMGRE_Object_GenerateVertexAttributes(object);
	object->pointList_wN[0].color = (GRErgb24){ 245, 55, 55 };
	object->pointList_wN[1].color = (GRErgb24){ 55, 245, 85 };
	object->pointList_wN[2].color = (GRErgb24){ 60, 100, 250 };
	return object;
}

static GRE_Material createWhite(const char* name)
{
	GRE_Material material = YMGRE_Creat_Material((char*)name);
	material->ambient = material->diffuse = (GRErgb24){ 255, 255, 255 };
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	return material;
}

static void initStage(ClipStage* stage, int plane)
{
	char name[24];
	snprintf(name, sizeof(name), "clip_plane_%d", plane);
	YMGRE_List_Append(&stage->objects, sizeof(gre_object4d),
		createTriangle(plane, name));
	YMGRE_List_Append(&stage->materials, sizeof(gre_material), createWhite(name));
	YMGRE_List_Append(&stage->lights, sizeof(gre_light4d), YMGRE_Creat_Light(0,
		GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f));
	stage->camera = YMGRE_Creat_Camera(plane, PANEL_W, PANEL_H, 45, 45, 45, 45);
	YMGRE_Camera_Frustum_Init(stage->camera, 100, 300);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(stage->camera, &eye, &target, NULL, 0);
	stage->workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_wN(stage->camera, &stage->lights, &stage->objects,
		&stage->materials, stage->workspace);
}

static void destroyStage(ClipStage* stage)
{
	YMGRE_Free_RenderWorkspace(stage->workspace);
	YMGRE_Free_Camera(stage->camera);
	YMGRE_List_Clear(&stage->lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&stage->objects, YMGRE_Free_Object);
	YMGRE_List_Clear(&stage->materials, YMGRE_Free_Material);
}

int main(void)
{
	ClipStage stages[PLANE_COUNT] = { 0 };
	YMGRE_DemoView views[PLANE_COUNT];
	for (int i = 0; i < PLANE_COUNT; i++)
	{
		initStage(&stages[i], i);
		views[i] = (YMGRE_DemoView){ YMGRE_Camera_GetRenderTarget(stages[i].camera),
			(i % COLUMNS) * PANEL_W, (i / COLUMNS) * PANEL_H, PANEL_W, PANEL_H };
	}
	YMGRE_DemoHost_Show(COLUMNS * PANEL_W, 2 * PANEL_H, views, PLANE_COUNT, 60);
	for (int i = 0; i < PLANE_COUNT; i++) destroyStage(&stages[i]);
	return 0;
}
