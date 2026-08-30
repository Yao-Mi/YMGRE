#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include <stdio.h>

static GRErgb24 shade(GRErgb24 base, const gre_fvector4d* point,
	const gre_fvector4d* normal, const gre_fvector4d* light)
{
	gre_fvector4d toLight = { light->x - point->x, light->y - point->y,
		light->z - point->z, 0 };
	float32 distance = YMGRE_Fvector4d_Len1(&toLight);
	if (distance < 1e-5f) distance = 1e-5f;
	YMGRE_Fvector4d_Normalize(&toLight);
	float32 dot = normal->x * toLight.x + normal->y * toLight.y + normal->z * toLight.z;
	if (dot < 0) dot = 0;
	float32 attenuation = 1.0f / (1.0f + 0.035f * distance * distance);
	float32 intensity = 0.12f + 2.2f * dot * attenuation;
	if (intensity > 1.0f) intensity = 1.0f;
	GRErgb24 out = { (uint8)(base.R * intensity), (uint8)(base.G * intensity), (uint8)(base.B * intensity) };
	return out;
}

int main(void)
{
	const uint16 width = 560, height = 420;
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 38, 38, 31, 31);
	GRE_Object4d cube = YMGRE_MeshGener_Cube(4.0f, (GRErgb24){ 210, 150, 55 }, "ray_diffuse_cube", "ray");
	if (camera == NULL || cube == NULL) return 1;
	cube->WorldCoordinate = (gre_fvector4d){ 0, 0, 9, 1 };
	YMGRE_Object_LocalToWorld(cube);
	YMGRE_Camera_Frustum_Init(camera, 1, 100);
	gre_fvector4d eye = { 5, 3, 0, 1 }, target = { 0, 0, 9, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	gre_fvector4d light = { -4, 5, 2, 1 };
	GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
	uint32 hitPixels = 0, darkPixels = 0, brightPixels = 0;
	for (uint16 y = 0; y < height; y++) for (uint16 x = 0; x < width; x++) {
		gre_ray ray; gre_ray_scene_hit hit;
		GRErgb24 color = { 8, 10, 16 };
		if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray) &&
			YMGRE_Ray_IntersectObject(&ray, cube, 0.001f, 100.0f, &hit)) {
			color = shade(cube->polygonList[hit.polygonIndex].planeColor,
				&hit.hit.position, &hit.hit.normal, &light);
			uint16 lum = (uint16)color.R + color.G + color.B;
			if (lum < 100) darkPixels++; if (lum > 300) brightPixels++;
			hitPixels++;
		}
		out->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
	}
	int pass = hitPixels > 10000 && darkPixels > 100 && brightPixels > 100;
	printf("ray diffuse: hits=%u dark=%u bright=%u: %s\n",
		hitPixels, darkPixels, brightPixels, pass ? "PASS" : "FAIL");
	YMGRE_DemoView view = { out, 0, 0, width, height };
	YMGRE_DemoHost_Show(width, height, &view, 1, 60);
	YMGRE_Free_Object(cube); YMGRE_Free_Camera(camera);
	return pass ? 0 : 1;
}
