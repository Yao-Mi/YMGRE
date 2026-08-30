#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include <stdio.h>

int main(void)
{
	const uint16 width = 560, height = 420;
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 38, 38, 31, 31);
	GRE_Object4d cube = YMGRE_MeshGener_Cube(4.0f, (GRErgb24){ 220, 220, 220 },
		"ray_single_cube", "ray");
	if (camera == NULL || cube == NULL) return 1;
	GRErgb24 faceColors[6] = {
		{ 220, 55, 45 }, { 45, 105, 230 }, { 45, 185, 90 },
		{ 235, 190, 40 }, { 200, 65, 205 }, { 45, 200, 205 }
	};
	for (uint32 i = 0; i < (uint32)cube->polygonNum; i++)
		cube->polygonList[i].planeColor = faceColors[i / 2];
	cube->WorldCoordinate = (gre_fvector4d){ 0, 0, 9, 1 };
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Camera_Frustum_Init(camera, 1, 100);
	gre_fvector4d eye = { 5, 4, 0, 1 }, target = { 0, 0, 9, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
	uint32 hitPixels = 0, facePixels[6] = { 0 };
	for (uint16 y = 0; y < height; y++) for (uint16 x = 0; x < width; x++) {
		gre_ray ray; gre_ray_scene_hit hit;
		GRErgb24 color = { 12, 16, 24 };
		if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray) &&
			YMGRE_Ray_IntersectObject(&ray, cube, 0.001f, 100.0f, &hit)) {
			color = cube->polygonList[hit.polygonIndex].planeColor;
			hitPixels++;
			facePixels[hit.polygonIndex / 2]++;
		}
		out->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
	}
	int visibleFaces = 0;
	for (int i = 0; i < 6; i++) if (facePixels[i] >= 100) visibleFaces++;
	int pass = hitPixels > 10000 && visibleFaces >= 3;
	printf("ray single object: hitPixels=%u, visibleFaces=%d: %s\n",
		hitPixels, visibleFaces, pass ? "PASS" : "FAIL");
	YMGRE_DemoView view = { out, 0, 0, width, height };
	YMGRE_DemoHost_Show(width, height, &view, 1, 60);
	YMGRE_Free_Object(cube);
	YMGRE_Free_Camera(camera);
	return pass ? 0 : 1;
}
