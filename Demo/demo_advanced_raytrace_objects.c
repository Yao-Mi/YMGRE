#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

static GRE_Object4d makeCube(float32 x, float32 z, GRErgb24 color, const char* name)
{
	GRE_Object4d object = YMGRE_MeshGener_Cube(3.2f, color, (char*)name, "ray");
	if (object == NULL) return NULL;
	object->WorldCoordinate = (gre_fvector4d){ x, 0, z, 1 };
	YMGRE_Object_LocalToWorld(object);
	return object;
}

int main(void)
{
	const uint16 width = 560, height = 420;
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 38, 38, 31, 31);
	GRE_Object4d red = makeCube(-2.2f, 9.0f, (GRErgb24){ 220, 45, 40 }, "ray_red");
	GRE_Object4d blue = makeCube(2.2f, 11.0f, (GRErgb24){ 40, 80, 225 }, "ray_blue");
	if (camera == NULL || red == NULL || blue == NULL) return 1;
	YMGRE_Camera_Frustum_Init(camera, 1, 100);
	gre_fvector4d eye = { 0, 1.2f, 0, 1 }, target = { 0, 0, 10, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	gre_list objects = { 0 };
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), red);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), blue);
	GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
	uint32 hitPixels = 0, redPixels = 0, bluePixels = 0;
	for (uint16 y = 0; y < height; y++) for (uint16 x = 0; x < width; x++) {
		gre_ray ray; gre_ray_scene_hit hit;
		GRErgb24 color = { 10, 14, 22 };
		if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray) &&
			YMGRE_Ray_IntersectScene(&ray, &objects, 0.001f, 100.0f, &hit)) {
			color = hit.object->polygonList[hit.polygonIndex].planeColor;
			hitPixels++;
			if (hit.object == red) redPixels++; else if (hit.object == blue) bluePixels++;
		}
		out->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
	}
	int pass = hitPixels > 12000 && redPixels > 3000 && bluePixels > 3000;
	printf("ray two objects: hits=%u red=%u blue=%u: %s\n",
		hitPixels, redPixels, bluePixels, pass ? "PASS" : "FAIL");
	YMGRE_DemoView view = { out, 0, 0, width, height };
	YMGRE_DemoHost_Show(width, height, &view, 1, 60);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	YMGRE_Free_Camera(camera);
	return pass ? 0 : 1;
}
