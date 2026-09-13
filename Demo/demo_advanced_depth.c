#include "demo_host.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

#define PANEL_W 360
#define PANEL_H 360

static GRE_Object4d createQuad(float32 z, GRErgb24 color, const char* name)
{
	GRE_Object4d o = YMGRE_Creat_Object(4, 2, (char*)name, NULL);
	o->pointList[0] = (gre_vertex4d){ { -125, -100, z, 1 }, 0, 1 };
	o->pointList[1] = (gre_vertex4d){ { 125, -100, z, 1 }, 1, 1 };
	o->pointList[2] = (gre_vertex4d){ { 125, 100, z, 1 }, 1, 0 };
	o->pointList[3] = (gre_vertex4d){ { -125, 100, z, 1 }, 0, 0 };
	GRE_Index indices[6] = { 0, 3, 2, 0, 2, 1 };
	for (int i = 0; i < 2; i++) { GRE_Polygon4d p = &o->polygonList[i]; p->num = 3; p->index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index)); for (int j = 0; j < 3; j++) p->index[j] = indices[i * 3 + j]; p->pN = (gre_fvector4d){ 0, 0, -1, 0 }; p->planeColor = color; }
	o->BoundingSphereR = 170; o->boundType = GRE_Bounding_Sphere_R; YMGRE_Object_GenerateVertexAttributes(o); o->renderMode = GRE_RenderMode_Face; for (int i = 0; i < 4; i++) o->pointList_wN[i].color = color; return o;
}

static GRE_Camera4d renderOrder(uint8 frontFirst)
{
	gre_list objects = { 0 };
	GRE_Object4d front = createQuad(145, (GRErgb24){ 245, 55, 55 }, "front");
	GRE_Object4d back = createQuad(210, (GRErgb24){ 50, 120, 245 }, "back");
	YMGRE_List_Append(&objects, sizeof(gre_object4d), frontFirst ? front : back);
	YMGRE_List_Append(&objects, sizeof(gre_object4d), frontFirst ? back : front);
	GRE_Camera4d c = YMGRE_Creat_Camera(frontFirst, PANEL_W, PANEL_H, 42, 42, 42, 42);
	YMGRE_Camera_Frustum_Init(c, 1, 500); gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 180, 1 }; YMGRE_UVNCamera_PositionInit(c, &eye, &target, NULL, 0);
	GRE_RenderWorkspace w = YMGRE_Creat_RenderWorkspace(); YMGRE_Camera_TanglePipline_VertexColor_wN(c, &objects, w);
	YMGRE_Free_RenderWorkspace(w); YMGRE_List_Clear(&objects, YMGRE_Free_Object); return c;
}

int main(void)
{
	GRE_Camera4d first = renderOrder(1), second = renderOrder(0);
	YMGRE_DemoView views[2] = { { YMGRE_Camera_GetRenderTarget(first), 0, 0, PANEL_W, PANEL_H }, { YMGRE_Camera_GetRenderTarget(second), PANEL_W, 0, PANEL_W, PANEL_H } };
	YMGRE_DemoHost_Show(PANEL_W * 2, PANEL_H, views, 2, 60);
	YMGRE_Free_Camera(first); YMGRE_Free_Camera(second); return 0;
}
