#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

typedef struct {
  GRE_Object4d glass, lamp;
  gre_fvector4d light;
  uint32 sphereHits, refracted, totalInternal, lampHits, highlightPixels;
  uint32 transmittedBackground;
} Refraction;

static GRErgb24 trace(Refraction *s, GRE_Ray ray, const gre_list *objects, int depth)
{
  gre_ray_scene_hit hit;
  if (depth > 4 || !YMGRE_Ray_IntersectScene(ray, objects, 1e-4f, 100.0f, &hit))
    return (GRErgb24){24, 34, 52};
  if (hit.object == s->lamp) {
    s->lampHits++;
    return (GRErgb24){255, 245, 205};
  }
  if (hit.object != s->glass) {
    if (depth >= 2) s->transmittedBackground++;
    return hit.object->polygonList[hit.polygonIndex].planeColor;
  }
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
  gre_ray next;
  YMGRE_Ray_SpawnFromSurface(&hit.hit.position, &n, &dir, .002f, &next);
  GRErgb24 transmitted = trace(s, &next, objects, depth + 1);
  if (depth > 0) return transmitted;
  gre_fvector4d view = {-ray->direction.x, -ray->direction.y, -ray->direction.z, 0};
  GRErgb24 surface = YMGRE_Ray_ShadeBlinnPhong(
    (GRErgb24){35, 65, 90}, &hit.hit.position, &hit.hit.normal, &view, &s->light,
    (GRErgb24){255, 255, 255}, .08f, .7f, .018f, 255.0f, 48.0f);
  if (surface.R > 150 && surface.G > 150 && surface.B > 150) s->highlightPixels++;
  return (GRErgb24){
    (uint8)(transmitted.R * .82f + surface.R * .18f),
    (uint8)(transmitted.G * .82f + surface.G * .18f),
    (uint8)(transmitted.B * .82f + surface.B * .18f)};
}

int main(void)
{
  const uint16 w = 560, h = 420;
  GRE_Camera4d camera = YMGRE_Creat_Camera(0, w, h, 38, 38, 31, 31);
  GRE_Object4d glass = YMGRE_MeshGener_Sphere(2.2f, 24, 32, (GRErgb24){180, 220, 255}, "glass", "ray");
  GRE_Object4d backdrop = YMGRE_MeshGener_Cube(7.0f, (GRErgb24){35, 75, 150}, "backdrop", "ray");
  GRE_Object4d red = YMGRE_MeshGener_Cube(1.5f, (GRErgb24){235, 45, 40}, "red", "ray");
  GRE_Object4d green = YMGRE_MeshGener_Cube(1.5f, (GRErgb24){35, 220, 80}, "green", "ray");
  GRE_Object4d yellow = YMGRE_MeshGener_Cube(1.5f, (GRErgb24){245, 205, 35}, "yellow", "ray");
  GRE_Object4d lamp = YMGRE_MeshGener_Sphere(.32f, 8, 12, (GRErgb24){255, 245, 205}, "lamp", "ray");
  if (!camera || !glass || !backdrop || !red || !green || !yellow || !lamp) return 1;
  glass->WorldCoordinate = (gre_fvector4d){0, 0, 8, 1};
  backdrop->WorldCoordinate = (gre_fvector4d){0, 0, 14, 1};
  red->WorldCoordinate = (gre_fvector4d){-2.0f, 0, 10.8f, 1};
  green->WorldCoordinate = (gre_fvector4d){0, 0, 10.8f, 1};
  yellow->WorldCoordinate = (gre_fvector4d){2.0f, 0, 10.8f, 1};
  lamp->WorldCoordinate = (gre_fvector4d){-3.0f, 3.6f, 5.0f, 1};
  YMGRE_Object_LocalToWorld(glass); YMGRE_Object_LocalToWorld(backdrop);
  YMGRE_Object_LocalToWorld(red); YMGRE_Object_LocalToWorld(green); YMGRE_Object_LocalToWorld(yellow);
  YMGRE_Object_LocalToWorld(lamp);
  YMGRE_Camera_Frustum_Init(camera, 1, 100);
  gre_fvector4d eye = {4, 2, 0, 1}, target = {0, 0, 8, 1};
  YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
  gre_list objects = {0};
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), glass);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), backdrop);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), red);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), green);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), yellow);
  YMGRE_List_Append(&objects, sizeof(GRE_Object4d), lamp);
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
  Refraction stats = {glass, lamp, lamp->WorldCoordinate, 0, 0, 0, 0, 0, 0};
  uint32 pixels = 0;
  for (uint16 y = 0; y < h; y++) for (uint16 x = 0; x < w; x++) {
    gre_ray ray; GRErgb24 color = {10, 16, 28};
    if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray)) {
      gre_ray_scene_hit hit;
      if (YMGRE_Ray_IntersectScene(&ray, &objects, .001f, 100, &hit)) { color = trace(&stats, &ray, &objects, 0); pixels++; }
    }
    out->data[y*w+x] = GRE_FramePixel_From_RGB24(color);
  }
  int pass = pixels > 10000 && stats.sphereHits > 1000 && stats.refracted > 1000 &&
    stats.lampHits > 10 && stats.highlightPixels > 10;
  pass = pass && stats.transmittedBackground > 1000;
  printf("ray refraction: pixels=%u sphereHits=%u refracted=%u totalInternal=%u lamp=%u highlight=%u transmittedBackground=%u: %s\n",
    pixels, stats.sphereHits, stats.refracted, stats.totalInternal, stats.lampHits,
    stats.highlightPixels, stats.transmittedBackground, pass ? "PASS" : "FAIL");
  YMGRE_DemoView view = {out, 0, 0, w, h}; YMGRE_DemoHost_Show(w, h, &view, 1, 60);
  YMGRE_Free_Object(glass); YMGRE_Free_Object(backdrop); YMGRE_Free_Object(red);
  YMGRE_Free_Object(green); YMGRE_Free_Object(yellow); YMGRE_Free_Object(lamp); YMGRE_Free_Camera(camera);
  return pass ? 0 : 1;
}
