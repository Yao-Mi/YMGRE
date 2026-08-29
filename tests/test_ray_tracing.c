#include "YMGRE_RayTracing.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Free.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_List.h"
#include "YMGRE_Coordinates_Transform.h"
#include <stdio.h>

int main(void)
{
	gre_fvector4d p0 = { -1, -1, 5, 1 }, p1 = { 1, -1, 5, 1 }, p2 = { 0, 1, 5, 1 };
	gre_ray ray = { { 0, 0, 0, 1 }, { 0, 0, 1, 0 } };
	gre_ray_hit hit = { 0 };
	int pass = YMGRE_Ray_IntersectTriangle(&ray, &p0, &p1, &p2, 0.001f, 100.0f, &hit);
	pass = pass && YMGRE_Fabs(hit.distance - 5.0f) < 1e-5f;
	pass = pass && hit.u >= 0.0f && hit.v >= 0.0f && hit.u + hit.v <= 1.0f;
	gre_ray miss = { { 3, 3, 0, 1 }, { 0, 0, 1, 0 } };
	pass = pass && !YMGRE_Ray_IntersectTriangle(&miss, &p0, &p1, &p2, 0.001f, 100.0f, &hit);
	GRE_Object4d object = YMGRE_MeshGener_Cube(2.0f, (GRErgb24){255,255,255}, "ray_cube", "ray");
	gre_ray_scene_hit sceneHit = { 0 };
	pass = pass && YMGRE_Ray_IntersectObject(&ray, object, 0.001f, 100.0f, &sceneHit);
	pass = pass && sceneHit.object == object && sceneHit.hit.distance > 0.0f;
	YMGRE_Free_Object(object);
	GRE_Object4d front = YMGRE_MeshGener_Cube(2.0f, (GRErgb24){255,0,0}, "ray_front", "ray");
	GRE_Object4d back = YMGRE_MeshGener_Cube(2.0f, (GRErgb24){0,0,255}, "ray_back", "ray");
	front->WorldCoordinate.z = 6.0f;
	back->WorldCoordinate.z = 12.0f;
	YMGRE_Object_LocalToWorld(front);
	YMGRE_Object_LocalToWorld(back);
	gre_list objects = { 0 };
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), front);
	YMGRE_List_Append(&objects, sizeof(GRE_Object4d), back);
	gre_ray_scene_hit nearest = { 0 };
	int scenePass = YMGRE_Ray_IntersectScene(&ray, &objects, 0.001f, 100.0f, &nearest);
	scenePass = scenePass && nearest.object == front && nearest.hit.distance < 6.0f;
	pass = pass && scenePass;
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	printf("ray triangle intersection: %s\n", pass ? "PASS" : "FAIL");
	return pass ? 0 : 1;
}
