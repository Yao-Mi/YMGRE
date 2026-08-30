#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

typedef struct { GRE_Object4d glass; uint32 sphereHits, refracted, totalInternal; } Refraction;

static GRErgb24 trace(Refraction *s, GRE_Ray ray, const gre_list *objects, int depth)
{
  gre_ray_scene_hit hit;
  if (depth > 4 || !YMGRE_Ray_IntersectScene(ray, objects, 1e-4f, 100.0f, &hit))
    return (GRErgb24){10, 16, 28};
  if (hit.object != s->glass)
    return hit.object->polygonList[hit.polygonIndex].planeColor;
  s->sphereHits++;
  gre_fvector4d n = hit.hit.normal;
  float32 cosi = -(ray->direction.x*n.x + ray->direction.y*n.y + ray->direction.z*n.z);
  float32 etaIn = 1.0f, etaOut = 1.5f;
  if (cosi < 0.0f) { cosi = -cosi; n.x = -n.x; n.y = -n.y; n.z = -n.z; etaIn = 1.5f; etaOut = 1.0f; }
  gre_fvector4d dir;
  if (!YMGRE_Ray_Refract(&ray->direction, &n, etaIn, etaOut, &dir)) {
    s->totalInternal++;
    return (GRErgb24){18, 22, 38};
  }
  s->refracted++;
  gre_ray next = {hit.hit.position, dir};
  next.origin.x += n.x * .002f; next.origin.y += n.y * .002f; next.origin.z += n.z * .002f;
  return trace(s, &next, objects, depth + 1);
}

int main(void)
{
  const uint16 w = 560, h = 420;
  GRE_Camera4d camera = YMGRE_Creat_Camera(0, w, h, 38, 38, 31, 31);
  GRE_Object4d glass = YMGRE_MeshGener_Sphere(2.2f, 24, 32, (GRErgb24){180, 220, 255}, "glass", "ray");
  GRE_Object4d backdrop = YMGRE_MeshGener_Cube(5.0f, (GRErgb24){30, 120, 230}, "backdrop", "ray");
  if (!camera || !glass || !backdrop) return 1;
  glass->WorldCoordinate = (gre_fvector4d){0, 0, 8, 1};
  backdrop->WorldCoordinate = (gre_fvector4d){0, 0, 14, 1};
  YMGRE_Object_LocalToWorld(glass); YMGRE_Object_LocalToWorld(backdrop);
  YMGRE_Camera_Frustum_Init(camera, 1, 100);
  gre_fvector4d eye = {4, 2, 0, 1}, target = {0, 0, 8, 1};
  YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
  gre_list objects = {0};
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), glass);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), backdrop);
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
  Refraction stats = {glass, 0, 0, 0};
  uint32 pixels = 0;
  for (uint16 y = 0; y < h; y++) for (uint16 x = 0; x < w; x++) {
    gre_ray ray; GRErgb24 color = {10, 16, 28};
    if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray)) {
      gre_ray_scene_hit hit;
      if (YMGRE_Ray_IntersectScene(&ray, &objects, .001f, 100, &hit)) { color = trace(&stats, &ray, &objects, 0); pixels++; }
    }
    out->data[y*w+x] = GRE_FramePixel_From_RGB24(color);
  }
  int pass = pixels > 10000 && stats.sphereHits > 1000 && stats.refracted > 1000;
  printf("ray refraction: pixels=%u sphereHits=%u refracted=%u totalInternal=%u: %s\n",
    pixels, stats.sphereHits, stats.refracted, stats.totalInternal, pass ? "PASS" : "FAIL");
  YMGRE_DemoView view = {out, 0, 0, w, h}; YMGRE_DemoHost_Show(w, h, &view, 1, 60);
  YMGRE_Free_Object(glass); YMGRE_Free_Object(backdrop); YMGRE_Free_Camera(camera);
  return pass ? 0 : 1;
}
