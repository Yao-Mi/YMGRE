#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"

int main(void)
{
  const uint16 width = 560, height = 420;
  GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 38, 38, 31, 31);
  if (!camera) return 1;
  YMGRE_Camera_Frustum_Init(camera, 1, 100);
  gre_fvector4d eye = {4, 2, 0, 1}, target = {0, 0, 7, 1};
  YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
  /* Clockwise winding presents the triangle's front face toward the camera. */
  gre_fvector4d p0 = {-3, -2, 7, 1}, p1 = {0, 2.8f, 7, 1}, p2 = {3, -2, 7, 1};
  gre_fvector4d light = {-2, 4, 3, 1};
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
  uint32 hits = 0, highlight = 0;
  for (uint16 y = 0; y < height; y++) for (uint16 x = 0; x < width; x++) {
    gre_ray ray; GRErgb24 color = {8, 10, 16};
    if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray)) {
      gre_ray_hit hit;
      if (YMGRE_Ray_IntersectTriangle(&ray, &p0, &p1, &p2, .001f, 100, &hit)) {
        gre_fvector4d viewDirection = {-ray.direction.x, -ray.direction.y,
          -ray.direction.z, 0};
        color = YMGRE_Ray_ShadeBlinnPhong((GRErgb24){205, 55, 40}, &hit.position,
          &hit.normal, &viewDirection, &light, (GRErgb24){255,255,255}, .12f,
          2.4f, .02f, 220.0f, 24.0f);
        hits++;
        if (color.R > 235 && color.G > 180 && color.B > 160) highlight++;
      }
    }
    out->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
  }
  int pass = hits > 10000 && highlight > 10;
  printf("ray shading: hits=%u highlight=%u: %s\n", hits, highlight, pass ? "PASS" : "FAIL");
  YMGRE_DemoView view = {out, 0, 0, width, height};
  YMGRE_DemoHost_Show(width, height, &view, 1, 60);
  YMGRE_Free_Camera(camera);
  return pass ? 0 : 1;
}
