#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

int main(void)
{
	const uint16 width = 640, height = 440;
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 42, 42, 30, 30);
	if (camera == NULL) return 1;
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 100.0f);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 6, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	gre_fvector4d p0 = { -2.8f, -2.0f, 6.0f, 1 };
	gre_fvector4d p1 = { 2.8f, -2.0f, 6.0f, 1 };
	gre_fvector4d p2 = { 0, 2.6f, 6.0f, 1 };
	GRE_RenderTarget targetBuffer = YMGRE_Camera_GetRenderTarget(camera);
	for (uint16 y = 0; y < height; y++)
	{
		for (uint16 x = 0; x < width; x++)
		{
			gre_ray ray;
			GRErgb24 color = { 10, 14, 24 };
			if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray))
			{
				gre_ray_hit hit;
				if (YMGRE_Ray_IntersectTriangle(&ray, &p0, &p1, &p2, 0.001f, 100.0f, &hit))
				{
					/* Barycentrics make the triangle boundary and interpolation obvious. */
					color.R = (uint8)(40.0f + 215.0f * (1.0f - hit.u - hit.v));
					color.G = (uint8)(40.0f + 215.0f * hit.u);
					color.B = (uint8)(40.0f + 215.0f * hit.v);
				}
			}
			targetBuffer->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
		}
	}
	YMGRE_DemoView view = { targetBuffer, 0, 0, width, height };
	YMGRE_DemoHost_Show(width, height, &view, 1, 60);
	YMGRE_Free_Camera(camera);
	return 0;
}
