#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_RayTracing.h"
#include "demo_host.h"
#include <stdio.h>

typedef struct {
  GRE_Camera4d camera;
  gre_list objects;
  GRE_Object4d mirror, lamp;
  gre_fvector4d light;
  uint32 directLamp, reflectedLamp;
} Stage;

static GRErgb24 trace(Stage *s, GRE_Ray ray, int depth) {
  gre_ray_scene_hit h;
  float32 tmin = depth ? 1e-5f : .002f;
  if (depth > 2 || !YMGRE_Ray_IntersectScene(ray, &s->objects, tmin, 100, &h))
    return (GRErgb24){8, 10, 16};
  if (h.object == s->lamp) {
    if (depth)
      s->reflectedLamp++;
    return (GRErgb24){255, 245, 210};
  }
  if (h.object == s->mirror) {
    gre_ray q = {h.hit.position, {0}};
    YMGRE_Ray_Reflect(&ray->direction, &h.hit.normal, &q.direction);
    float32 side = q.direction.x * h.hit.normal.x +
                   q.direction.y * h.hit.normal.y +
                   q.direction.z * h.hit.normal.z,
            bias = side >= 0 ? .002f : -.002f;
    q.origin.x += h.hit.normal.x * bias;
    q.origin.y += h.hit.normal.y * bias;
    q.origin.z += h.hit.normal.z * bias;
    return trace(s, &q, depth + 1);
  }
  GRErgb24 b = h.object->polygonList[h.polygonIndex].planeColor;
  gre_fvector4d l = {s->light.x - h.hit.position.x,
                     s->light.y - h.hit.position.y,
                     s->light.z - h.hit.position.z, 0};
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
static int render(Stage *s, gre_fvector4d light) {
  const uint16 w = 300, h = 230;
  s->light = light;
  s->camera = YMGRE_Creat_Camera(0, w, h, 38, 38, 31, 31);
  GRE_Object4d cube =
      YMGRE_MeshGener_Cube(2.8f, (GRErgb24){220, 45, 40}, "cube", "ray");
  s->mirror = YMGRE_MeshGener_RectPlane(12, 12, 8, 8, (GRErgb24){150, 155, 165},
                                        "mirror", "ray");
  s->lamp = YMGRE_MeshGener_Sphere(.38f, 6, 8, (GRErgb24){255, 245, 210},
                                   "lamp", "ray");
  if (!s->camera || !cube || !s->mirror || !s->lamp)
    return 0;
  cube->WorldCoordinate = (gre_fvector4d){0, 0, 9, 1};
  s->mirror->WorldCoordinate = (gre_fvector4d){0, -1.4f, 10, 1};
  s->lamp->WorldCoordinate = light;
  YMGRE_Object_LocalToWorld(cube);
  YMGRE_Object_LocalToWorld(s->mirror);
  YMGRE_Object_LocalToWorld(s->lamp);
  YMGRE_Camera_Frustum_Init(s->camera, 1, 100);
  gre_fvector4d eye = {5, 4, 0, 1}, target = {0, 0, 9, 1};
  YMGRE_UVNCamera_PositionInit(s->camera, &eye, &target, NULL, 0);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), cube);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), s->mirror);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), s->lamp);
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(s->camera);
  for (uint16 y = 0; y < h; y++)
    for (uint16 x = 0; x < w; x++) {
      gre_ray ray;
      GRErgb24 color = {8, 10, 16};
      if (YMGRE_Ray_FromCameraPixel(s->camera, x, y, &ray)) {
        gre_ray_scene_hit hit;
        if (YMGRE_Ray_IntersectScene(&ray, &s->objects, .002f, 100, &hit)) {
          if (hit.object == s->lamp)
            s->directLamp++;
          color = trace(s, &ray, 0);
        }
      }
      out->data[y * w + x] = GRE_FramePixel_From_RGB24(color);
    }
  return 1;
}
int main(void) {
  const char *names[5] = {"front", "back", "left", "right", "top"};
  gre_fvector4d lights[5] = {{0, 0, 5.5f, 1},
                             {0, 0, 12.5f, 1},
                             {-3.5f, 0, 9, 1},
                             {3.5f, 0, 9, 1},
                             {0, 3.5f, 9, 1}};
  Stage stages[5] = {0};
  YMGRE_DemoView views[5];
  int pass = 1;
  for (int i = 0; i < 5; i++) {
    pass &= render(&stages[i], lights[i]);
    printf("mirror light %s: direct=%u reflected=%u\n", names[i],
           stages[i].directLamp, stages[i].reflectedLamp);
    views[i] = (YMGRE_DemoView){
        YMGRE_Camera_GetRenderTarget(stages[i].camera),
        (int16)((i < 3 ? i : i - 3) * 310 + (i >= 3 ? 155 : 0)),
        (int16)(i < 3 ? 0 : 240), 300, 230};
  }
  YMGRE_DemoHost_Show(930, 470, views, 5, 60);
  for (int i = 0; i < 5; i++) {
    YMGRE_List_Clear(&stages[i].objects, YMGRE_Free_Object);
    YMGRE_Free_Camera(stages[i].camera);
  }
  return pass ? 0 : 1;
}
