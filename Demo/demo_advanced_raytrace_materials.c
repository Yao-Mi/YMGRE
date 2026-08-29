#include "demo_host.h"
#include "YMGRE_RayTracing.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_Coordinates_Transform.h"

static GRE_Object4d makeQuad(float32 x, float32 z, GRErgb24 color, const char* name)
{
	GRE_Object4d o = YMGRE_Creat_Object(4, 2, (char*)name, "ray");
	if (o == NULL) return NULL;
	o->pointList[0] = (gre_vertex4d){{ x - 1.7f, -2.1f, z, 1 }, 0, 1};
	o->pointList[1] = (gre_vertex4d){{ x + 1.7f, -2.1f, z, 1 }, 1, 1};
	o->pointList[2] = (gre_vertex4d){{ x + 1.7f, 2.1f, z, 1 }, 1, 0};
	o->pointList[3] = (gre_vertex4d){{ x - 1.7f, 2.1f, z, 1 }, 0, 0};
	for (int i = 0; i < 2; i++) {
		o->polygonList[i].num = 3;
		o->polygonList[i].index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
		o->polygonList[i].planeColor = color;
	}
	o->polygonList[0].index[0] = 0; o->polygonList[0].index[1] = 1; o->polygonList[0].index[2] = 2;
	o->polygonList[1].index[0] = 0; o->polygonList[1].index[1] = 2; o->polygonList[1].index[2] = 3;
	o->WorldCoordinate.w = 1; o->scale = 1;
	YMGRE_Object_LocalToWorld(o);
	return o;
}

static GRErgb24 mixColor(GRErgb24 a, GRErgb24 b, float32 t)
{
	GRErgb24 out;
	if (t < 0) t = 0; if (t > 1) t = 1;
	out.R = (uint8)(a.R * (1 - t) + b.R * t);
	out.G = (uint8)(a.G * (1 - t) + b.G * t);
	out.B = (uint8)(a.B * (1 - t) + b.B * t);
	return out;
}

static GRErgb24 traceRay(const GRE_Ray ray, const gre_list* objects,
	GRE_Object4d mirror, GRE_Object4d glass, int depth)
{
	if (depth > 3) return (GRErgb24){ 8, 12, 20 };
	gre_ray_scene_hit hit;
	if (!YMGRE_Ray_IntersectScene(ray, objects, 0.002f, 100.0f, &hit))
		return (GRErgb24){ 8, 12, 20 };
	if (hit.object == mirror) {
		gre_ray reflected = { hit.hit.position, { 0, 0, 0, 0 } };
		YMGRE_Ray_Reflect(&ray->direction, &hit.hit.normal, &reflected.direction);
		reflected.origin.x += reflected.direction.x * 0.01f;
		reflected.origin.y += reflected.direction.y * 0.01f;
		reflected.origin.z += reflected.direction.z * 0.01f;
		return traceRay(&reflected, objects, mirror, glass, depth + 1);
	}
	if (hit.object == glass) {
		gre_ray inside = { hit.hit.position, { 0, 0, 0, 0 } };
		if (YMGRE_Ray_Refract(&ray->direction, &hit.hit.normal, 1.0f, 1.5f, &inside.direction)) {
			inside.origin.x += inside.direction.x * 0.01f;
			inside.origin.y += inside.direction.y * 0.01f;
			inside.origin.z += inside.direction.z * 0.01f;
			gre_ray_scene_hit exitHit;
			if (YMGRE_Ray_IntersectObject(&inside, glass, 0.002f, 100.0f, &exitHit)) {
				gre_ray outside = { exitHit.hit.position, { 0, 0, 0, 0 } };
				if (YMGRE_Ray_Refract(&inside.direction, &exitHit.hit.normal, 1.5f, 1.0f, &outside.direction)) {
					outside.origin.x += outside.direction.x * 0.01f;
					outside.origin.y += outside.direction.y * 0.01f;
					outside.origin.z += outside.direction.z * 0.01f;
					return mixColor((GRErgb24){ 30, 90, 130 }, traceRay(&outside, objects, mirror, glass, depth + 1), .72f);
				}
			}
		}
		return (GRErgb24){ 20, 70, 110 };
	}
	return hit.object->polygonList[hit.polygonIndex].planeColor;
}

int main(void)
{
	const uint16 width = 480, height = 330;
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, width, height, 42, 42, 30, 30);
	if (camera == NULL) return 1;
	YMGRE_Camera_Frustum_Init(camera, 1, 100);
	gre_fvector4d eye = { 0, 0, 0, 1 }, target = { 0, 0, 8, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &eye, &target, NULL, 0);
	gre_list objects = { 0 };
	GRE_Object4d red = makeQuad(-2.0f, 8.0f, (GRErgb24){ 210, 35, 35 }, "red_object");
	GRE_Object4d blue = makeQuad(2.0f, 10.5f, (GRErgb24){ 35, 80, 220 }, "blue_object");
	GRE_Object4d mirror = makeQuad(0, 13.0f, (GRErgb24){ 150, 155, 165 }, "mirror");
	GRE_Object4d glass = YMGRE_MeshGener_Sphere(8, 16, 16, (GRErgb24){ 100, 180, 220 }, "glass_ball", "ray");
	glass->WorldCoordinate = (gre_fvector4d){ 0, -0.1f, 7.5f, 1 }; glass->scale = 1.35f;
	YMGRE_Object_LocalToWorld(glass);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), red);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), blue);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), mirror);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), glass);
	GRE_RenderTarget out = YMGRE_Camera_GetRenderTarget(camera);
	for (uint16 y = 0; y < height; y++) for (uint16 x = 0; x < width; x++) {
		gre_ray ray; GRErgb24 color = { 8, 12, 20 };
		if (YMGRE_Ray_FromCameraPixel(camera, x, y, &ray)) color = traceRay(&ray, &objects, mirror, glass, 0);
		out->data[y * width + x] = GRE_FramePixel_From_RGB24(color);
	}
	YMGRE_DemoView view = { out, 0, 0, width, height };
	YMGRE_DemoHost_Show(width, height, &view, 1, 60);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
