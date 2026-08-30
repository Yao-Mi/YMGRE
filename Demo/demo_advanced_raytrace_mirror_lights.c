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
  GRE_Object4d cube, mirror, lamp;
  gre_fvector4d light;
  gre_fvector4d eye;
  uint32 directLamp, reflectedLamp;
  uint32 directSpecular, reflectedSpecular;
  uint32 directSpecularX, directSpecularY, reflectedSpecularX,
      reflectedSpecularY;
  float32 sampleSpecular;
  int sampleSpecularDepth;
  int sampleCubeDepth;
  uint16 directMinY, directMaxY, reflectedMinY, reflectedMaxY;
} Stage;

static GRErgb24 shadeSurface(Stage *s, GRE_Ray ray, const gre_ray_scene_hit *h,
                             int depth) {
  GRErgb24 b = h->object->polygonList[h->polygonIndex].planeColor;
  /* The ray direction points toward the hit; shading expects hit-to-camera. */
  gre_fvector4d view = {-ray->direction.x, -ray->direction.y,
                        -ray->direction.z, 0};
  GRErgb24 color = YMGRE_Ray_ShadeBlinnPhong(
      b, &h->hit.position, &h->hit.normal, &view, &s->light,
      (GRErgb24){255, 255, 255}, .25f, 2.0f, .03f, 240.0f, 14.0f);
  /* Keep the existing pixel-location diagnostics independent of the formula. */
  s->sampleSpecular = (color.G > (uint8)(b.G + 45)) ? 240.0f : 0.0f;
  s->sampleSpecularDepth = depth;
  return color;
}

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
  if (h.object == s->cube)
    s->sampleCubeDepth = depth;
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
    GRErgb24 reflected = trace(s, &q, depth + 1);
    const float32 reflectivity = .88f;
    const GRErgb24 base = {42, 48, 58};
    return (GRErgb24){
        (uint8)(base.R * (1.0f - reflectivity) + reflected.R * reflectivity),
        (uint8)(base.G * (1.0f - reflectivity) + reflected.G * reflectivity),
        (uint8)(base.B * (1.0f - reflectivity) + reflected.B * reflectivity)};
  }
  return shadeSurface(s, ray, &h, depth);
}
static int render(Stage *s, gre_fvector4d light) {
  const uint16 w = 300, h = 230;
  s->light = light;
  s->camera = YMGRE_Creat_Camera(0, w, h, 38, 38, 31, 31);
  s->cube = YMGRE_MeshGener_Cube(2.8f, (GRErgb24){220, 45, 40}, "cube", "ray");
  s->mirror = YMGRE_MeshGener_RectPlane(12, 12, 8, 8, (GRErgb24){150, 155, 165},
                                        "mirror", "ray");
  s->lamp = YMGRE_MeshGener_Sphere(.38f, 6, 8, (GRErgb24){255, 245, 210},
                                   "lamp", "ray");
  if (!s->camera || !s->cube || !s->mirror || !s->lamp)
    return 0;
  s->cube->WorldCoordinate = (gre_fvector4d){0, 0, 9, 1};
  s->mirror->WorldCoordinate = (gre_fvector4d){0, -1.4f, 10, 1};
  s->lamp->WorldCoordinate = light;
  YMGRE_Object_LocalToWorld(s->cube);
  YMGRE_Object_LocalToWorld(s->mirror);
  YMGRE_Object_LocalToWorld(s->lamp);
  YMGRE_Camera_Frustum_Init(s->camera, 1, 100);
  s->eye = (gre_fvector4d){5, 4, 0, 1};
  gre_fvector4d target = {0, 0, 9, 1};
  YMGRE_UVNCamera_PositionInit(s->camera, &s->eye, &target, NULL, 0);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), s->cube);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), s->mirror);
  YMGRE_List_Append(&s->objects, sizeof(GRE_Object4d), s->lamp);
  GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(s->camera);
  s->directMinY = s->reflectedMinY = h;
  for (uint16 y = 0; y < h; y++)
    for (uint16 x = 0; x < w; x++) {
      gre_ray ray;
      GRErgb24 color = {8, 10, 16};
      if (YMGRE_Ray_FromCameraPixel(s->camera, x, y, &ray)) {
        gre_ray_scene_hit hit;
        if (YMGRE_Ray_IntersectScene(&ray, &s->objects, .002f, 100, &hit)) {
          if (hit.object == s->lamp)
            s->directLamp++;
          s->sampleSpecular = 0;
          s->sampleSpecularDepth = 0;
          s->sampleCubeDepth = -1;
          color = trace(s, &ray, 0);
          if (s->sampleCubeDepth == 0) {
            if (y < s->directMinY)
              s->directMinY = y;
            if (y > s->directMaxY)
              s->directMaxY = y;
          } else if (s->sampleCubeDepth > 0) {
            if (y < s->reflectedMinY)
              s->reflectedMinY = y;
            if (y > s->reflectedMaxY)
              s->reflectedMaxY = y;
          }
          if (s->sampleSpecular > 8.0f) {
            if (s->sampleSpecularDepth > 0) {
              s->reflectedSpecular++;
              s->reflectedSpecularX += x;
              s->reflectedSpecularY += y;
            } else {
              s->directSpecular++;
              s->directSpecularX += x;
              s->directSpecularY += y;
            }
          }
        }
      }
      out->data[y * w + x] = GRE_FramePixel_From_RGB24(color);
    }
  return 1;
}
int main(void) {
  const char *names[5] = {"front", "back", "left", "right", "top"};
  gre_fvector4d lights[5] = {{0, 0, 5.0f, 1},
                             {0, 0, 14.0f, 1},
                             {-5.0f, 0, 9, 1},
                             {5.0f, 0, 9, 1},
                             {0, 5.0f, 9, 1}};
  Stage stages[5] = {0};
  YMGRE_DemoView views[5];
  int pass = 1;
  for (int i = 0; i < 5; i++) {
    pass &= render(&stages[i], lights[i]);
    printf("mirror light %s: direct=%u reflected=%u specular=%u "
           "reflectedSpecular=%u\n",
           names[i], stages[i].directLamp, stages[i].reflectedLamp,
           stages[i].directSpecular, stages[i].reflectedSpecular);
    if (stages[i].directSpecular)
      printf("  direct specular center=(%u,%u)\n",
             stages[i].directSpecularX / stages[i].directSpecular,
             stages[i].directSpecularY / stages[i].directSpecular);
    if (stages[i].reflectedSpecular)
      printf("  reflected specular center=(%u,%u)\n",
             stages[i].reflectedSpecularX / stages[i].reflectedSpecular,
             stages[i].reflectedSpecularY / stages[i].reflectedSpecular);
    printf("  cube bounds direct=%u..%u reflected=%u..%u\n",
           stages[i].directMinY, stages[i].directMaxY, stages[i].reflectedMinY,
           stages[i].reflectedMaxY);
    pass &= stages[i].directLamp > 0 && stages[i].reflectedLamp > 0;
    if (i == 0 || i == 3) {
      uint32 directCount =
          stages[i].directSpecular ? stages[i].directSpecular : 1;
      uint32 reflectedCount =
          stages[i].reflectedSpecular ? stages[i].reflectedSpecular : 1;
      uint32 directX = stages[i].directSpecularX / directCount;
      uint32 directY = stages[i].directSpecularY / directCount;
      uint32 reflectedX = stages[i].reflectedSpecularX / reflectedCount;
      uint32 reflectedY = stages[i].reflectedSpecularY / reflectedCount;
      uint32 dx =
          directX > reflectedX ? directX - reflectedX : reflectedX - directX;
      pass &=
          stages[i].directSpecular > 100 && stages[i].reflectedSpecular > 100;
      pass &= dx < 8 && reflectedY > directY + 20;
    } else if (i == 4) {
      pass &=
          stages[i].directSpecular > 100 && stages[i].reflectedSpecular == 0;
    } else {
      pass &= stages[i].directSpecular == 0 && stages[i].reflectedSpecular == 0;
    }
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
