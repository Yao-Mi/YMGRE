#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>

int main(void)
{
  const uint16 w = 560, h = 420;
  GRE_Camera4d camera = YMGRE_Creat_Camera(0, w, h, 38, 38, 31, 31);
  GRE_Object4d mesh = YMGRE_MeshGener_Cube(3.0f, (GRErgb24){45, 100, 230}, "mesh", "ray");
  if (!camera || !mesh) return 1;
  mesh->WorldCoordinate = (gre_fvector4d){2.2f, 0, 9, 1};
  YMGRE_Object_LocalToWorld(mesh);
  YMGRE_Camera_Frustum_Init(camera, 1, 100);
  gre_fvector4d eye = {5, 3, 0, 1}, target = {0, 0, 9, 1};
  YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
  const gre_fvector4d light = {-4.0f, 5.0f, 3.0f, 1};
  gre_list objects = {0}; YMGRE_List_Append(&objects, sizeof(GRE_Object4d), mesh);
  const gre_fvector4d boxMin = {-3.3f, -1.5f, 7.5f, 1};
  const gre_fvector4d boxMax = {-0.3f, 1.5f, 10.5f, 1};
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
  uint32 analyticHits = 0, meshHits = 0, analyticNormals = 0;
  for (uint16 y = 0; y < h; y++) for (uint16 x = 0; x < w; x++) {
    gre_ray ray; GRErgb24 color = {12, 16, 26};
    if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray)) {
      gre_ray_hit a; gre_ray_scene_hit m;
      int ha = YMGRE_Ray_IntersectAABB(&ray, &boxMin, &boxMax, .001f, 100, &a);
      int hm = YMGRE_Ray_IntersectScene(&ray, &objects, .001f, 100, &m);
      if (ha && (!hm || a.distance < m.hit.distance)) {
        gre_fvector4d view = {-ray.direction.x, -ray.direction.y, -ray.direction.z, 0};
        color = YMGRE_Ray_ShadeBlinnPhong((GRErgb24){230, 55, 45}, &a.position, &a.normal,
          &view, &light, (GRErgb24){255,255,255}, .12f, 1.4f, .02f, 40.0f, 24.0f);
        analyticHits++; if (YMGRE_Fabs(a.normal.x)+YMGRE_Fabs(a.normal.y)+YMGRE_Fabs(a.normal.z) > .9f) analyticNormals++;
      } else if (hm) {
        gre_fvector4d view = {-ray.direction.x, -ray.direction.y, -ray.direction.z, 0};
        color = YMGRE_Ray_ShadeBlinnPhong(m.object->polygonList[m.polygonIndex].planeColor,
          &m.hit.position, &m.hit.normal, &view, &light, (GRErgb24){255,255,255}, .12f, 1.4f, .02f, 40.0f, 24.0f);
        meshHits++;
      }
    }
    out->data[y*w+x] = GRE_FramePixel_From_RGB24(color);
  }
  int pass = analyticHits > 5000 && meshHits > 5000 && analyticNormals == analyticHits;
  printf("ray AABB: analytic=%u mesh=%u normals=%u: %s\n", analyticHits, meshHits, analyticNormals, pass ? "PASS" : "FAIL");
  YMGRE_DemoView view = {out, 0, 0, w, h}; YMGRE_DemoHost_Show(w, h, &view, 1, 60);
  YMGRE_Free_Object(mesh); YMGRE_Free_Camera(camera); return pass ? 0 : 1;
}
