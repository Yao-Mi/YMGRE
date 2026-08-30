#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_RayTracing.h"
#include "demo_host.h"
#include <stdio.h>
static const gre_fvector4d light = {-2.8f, 3.2f, 9.5f, 1};
static GRE_Object4d lamp;
static uint32 reflectedLampHits;
static uint32 contactRedPixels;
static float32 redMinDistance = 100.0f, redMaxDistance;
static uint32 nearContactRedPixels;
static uint32 objectEdgeRedPixels, mirrorEdgeRedPixels;
static uint32 reflectedRedPolygons[12];
static uint8 directCubeMaxR, reflectedCubeMaxR;
static GRErgb24 trace(GRE_Ray r, gre_list *os, GRE_Object4d mirror, int depth) {
  gre_ray_scene_hit h;
  if (depth > 2 || !YMGRE_Ray_IntersectScene(r, os, .002f, 100, &h))
    return (GRErgb24){8, 10, 16};
  if (h.object == lamp) {
    if (depth > 0)
      reflectedLampHits++;
    return (GRErgb24){255, 245, 210};
  }
  if (h.object == mirror) {
    gre_ray q = {h.hit.position, {0}};
    YMGRE_Ray_Reflect(&r->direction, &h.hit.normal, &q.direction);
    float32 side = q.direction.x * h.hit.normal.x +
                   q.direction.y * h.hit.normal.y +
                   q.direction.z * h.hit.normal.z;
    float32 bias = side >= 0.0f ? .002f : -.002f;
    q.origin.x += h.hit.normal.x * bias;
    q.origin.y += h.hit.normal.y * bias;
    q.origin.z += h.hit.normal.z * bias;
    GRErgb24 reflected = trace(&q, os, mirror, depth + 1);
    const GRErgb24 mirrorBase = {45, 50, 58};
    const float32 reflectivity = .82f;
    return (GRErgb24){
        (uint8)(mirrorBase.R * (1.0f - reflectivity) + reflected.R * reflectivity),
        (uint8)(mirrorBase.G * (1.0f - reflectivity) + reflected.G * reflectivity),
        (uint8)(mirrorBase.B * (1.0f - reflectivity) + reflected.B * reflectivity)};
  }
  GRErgb24 b = h.object->polygonList[h.polygonIndex].planeColor;
  gre_fvector4d l = {light.x - h.hit.position.x, light.y - h.hit.position.y,
                     light.z - h.hit.position.z, 0};
  float32 d = YMGRE_Fvector4d_Len1(&l);
  YMGRE_Fvector4d_Normalize(&l);
  float32 n =
      h.hit.normal.x * l.x + h.hit.normal.y * l.y + h.hit.normal.z * l.z;
  if (n < 0)
    n = 0;
  float32 k = .25f + 2 * n / (1 + .03f * d * d);
  if (k > 1)
    k = 1;
  return (GRErgb24){b.R * k, b.G * k, b.B * k};
}
int main(void) {
  const uint16 w = 520, hh = 380;
  GRE_Camera4d c = YMGRE_Creat_Camera(0, w, hh, 38, 38, 31, 31);
  GRE_Object4d cube = YMGRE_MeshGener_Cube(2.8f, (GRErgb24){220, 45, 40},
                                           "cube", "ray"),
               mirror = YMGRE_MeshGener_RectPlane(
                   12, 12, 12, 12, (GRErgb24){150, 155, 165}, "mirror", "ray");
  lamp = YMGRE_MeshGener_Sphere(.38f, 8, 12, (GRErgb24){255, 245, 210}, "lamp",
                                "ray");
  if (!c || !cube || !mirror || !lamp)
    return 1;
  cube->WorldCoordinate = (gre_fvector4d){0, 0, 9, 1};
  mirror->WorldCoordinate = (gre_fvector4d){0, -1.4f, 10, 1};
  lamp->WorldCoordinate = light;
  YMGRE_Object_LocalToWorld(cube);
  YMGRE_Object_LocalToWorld(mirror);
  YMGRE_Object_LocalToWorld(lamp);
  YMGRE_Camera_Frustum_Init(c, 1, 100);
  gre_fvector4d e = {5, 4, 0, 1}, t = {0, 0, 9, 1};
  YMGRE_UVNCamera_PositionInit(c, &e, &t, NULL, 0);
  gre_list os = {0};
  YMGRE_List_Append(&os, sizeof(GRE_Object4d), cube);
  YMGRE_List_Append(&os, sizeof(GRE_Object4d), mirror);
  YMGRE_List_Append(&os, sizeof(GRE_Object4d), lamp);
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(c);
  uint32 refl = 0, lp = 0;
  for (uint16 y = 0; y < hh; y++)
    for (uint16 x = 0; x < w; x++) {
      gre_ray r;
      GRErgb24 col = {8, 10, 16};
      if (YMGRE_Ray_FromCameraPixel(c, x, y, &r)) {
        gre_ray_scene_hit h;
        if (YMGRE_Ray_IntersectScene(&r, &os, .002f, 100, &h)) {
          col = trace(&r, &os, mirror, 0);
          if (h.object == mirror && (col.R > 30 || col.G > 30 || col.B > 30))
            refl++;
          if (h.object == lamp)
            lp++;
          if (h.object == cube && col.R > directCubeMaxR)
            directCubeMaxR = col.R;
          if (col.R > 80 && col.R > col.G * 2) {
            if (h.object == cube && h.hit.position.y < -1.25f)
              objectEdgeRedPixels++;
            if (h.object == mirror && h.hit.position.x > -1.55f &&
                h.hit.position.x < 1.55f && h.hit.position.z > 7.4f &&
                h.hit.position.z < 10.6f)
              mirrorEdgeRedPixels++;
          }
          if (h.object == mirror && col.R > 80 && col.R > col.G * 2 &&
              h.hit.position.x > -1.55f && h.hit.position.x < 1.55f &&
              h.hit.position.z > 7.4f && h.hit.position.z < 10.6f) {
            contactRedPixels++;
            gre_ray reflected = {h.hit.position, {0}};
            YMGRE_Ray_Reflect(&r.direction, &h.hit.normal, &reflected.direction);
            float32 bias = reflected.direction.y * h.hit.normal.y >= 0.0f ? .002f : -.002f;
            reflected.origin.x += h.hit.normal.x * bias;
            reflected.origin.y += h.hit.normal.y * bias;
            reflected.origin.z += h.hit.normal.z * bias;
            gre_ray_scene_hit rh;
            if (YMGRE_Ray_IntersectObject(&reflected, cube, .002f, 100, &rh)) {
              if (rh.polygonIndex < 12) reflectedRedPolygons[rh.polygonIndex]++;
              if (col.R > reflectedCubeMaxR) reflectedCubeMaxR = col.R;
              if (rh.hit.distance < redMinDistance) redMinDistance = rh.hit.distance;
              if (rh.hit.distance > redMaxDistance) redMaxDistance = rh.hit.distance;
              if (rh.hit.distance < .10f) nearContactRedPixels++;
            }
          }
        }
      }
      out->data[y * w + x] = GRE_FramePixel_From_RGB24(col);
    }
  int pass = refl > 100 && lp > 10 && reflectedLampHits > 2;
  printf("ray mirror: reflection=%u light=%u reflectedLight=%u objectEdgeRed=%u mirrorEdgeRed=%u contactRed=%u nearRed=%u redDistance=%.4f..%.4f: %s\n", refl, lp,
         reflectedLampHits, objectEdgeRedPixels, mirrorEdgeRedPixels,
         contactRedPixels, nearContactRedPixels, redMinDistance, redMaxDistance,
         pass ? "PASS" : "FAIL");
  printf("directCubeMaxR=%u reflectedCubeMaxR=%u reflected polygons:",
         directCubeMaxR, reflectedCubeMaxR);
  for (int i = 0; i < 12; i++) if (reflectedRedPolygons[i])
    printf(" %d=%u", i, reflectedRedPolygons[i]);
  printf("\n");
  YMGRE_DemoView v = {out, 0, 0, w, hh};
  YMGRE_DemoHost_Show(w, hh, &v, 1, 60);
  YMGRE_List_Clear(&os, YMGRE_Free_Object);
  YMGRE_Free_Camera(c);
  return pass ? 0 : 1;
}
