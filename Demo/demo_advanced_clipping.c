#include "demo_host.h"
#include "YMGRE_Camera.h"
#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_TriangleRaster.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

#define W 640
#define H 420

int main(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, W, H, 45, 45, 35, 35);
	YMGRE_Camera_Frustum_Init(camera, 100, 500);
	gre_fvector4d eye = { 0, 0, 0, 1 }, targetPoint = { 0, 0, 180, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &targetPoint, NULL, 0);
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(camera);
	gre_camera4d render = *camera;
	render.img = *target;
	render.target = target;
	YMGRE_CameraImage_Init(&render, (GRErgb24){ 35, 38, 45 });

	/* One vertex is before Znear; clipping should produce a visible quad. */
	gre_vertex4d_wN input[3] = { 0 }, clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX] = { 0 };
	input[0].base = (gre_vertex4d){ { -60, -45, 55, 1 }, 0, 1 };
	input[1].base = (gre_vertex4d){ { 100, -45, 230, 1 }, 1, 1 };
	input[2].base = (gre_vertex4d){ { 0, 100, 230, 1 }, .5f, 0 };
	input[0].color = (GRErgb24){ 245, 55, 55 };
	input[1].color = (GRErgb24){ 55, 245, 90 };
	input[2].color = (GRErgb24){ 65, 100, 250 };
	for (int i = 0; i < 3; i++)
	{
		input[i].normal = (gre_fvector4d){ 0, 0, -1, 0 };
		input[i].tangent = (gre_fvector4d){ 1, 0, 0, 0 };
		input[i].tangentW = 1;
	}
	uint16 count = YMGRE_Polygon_FrustumClip_wN(input, 3, clipped,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, &render);
	if (count >= 3)
	{
		YMGRE_VertexList_CameraToViewPlane_wN(clipped, count, render.perspectPlane.Dis);
		YMGRE_VertexList_ViewPlaneToWindows_wN(clipped, count, &render);
		gre_polygon4d polygonStorage = { 0 };
		uint16 polygonIndices[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
		GRE_Polygon4d polygon = &polygonStorage;
		polygon->num = count;
		polygon->index = polygonIndices;
		for (uint16 i = 0; i < count; i++) polygon->index[i] = i;
		for (uint16 i = 1; i + 1 < count; i++)
		{
			uint16 tri[3] = { 0, i, (uint16)(i + 1) };
			GRE_Polygon4d triangle = polygon;
			triangle->index = tri;
			triangle->num = 3;
			YMGRE_TriangleRaster_FillVertexColor_wN(clipped, triangle, &render);
		}
	}
	GRE_RenderTarget output = YMGRE_Camera_GetRenderTarget(camera);
	YMGRE_DemoView view = { output, 0, 0, W, H };
	YMGRE_DemoHost_Show(W, H, &view, 1, 60);
	YMGRE_Free_Camera(camera);
	return 0;
}
