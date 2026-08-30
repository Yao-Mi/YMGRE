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
	GRE_Object4d tieFirst = YMGRE_MeshGener_Cube(2.0f, (GRErgb24){255,0,0}, "tie_first", "ray");
	GRE_Object4d tieSecond = YMGRE_MeshGener_Cube(2.0f, (GRErgb24){0,0,255}, "tie_second", "ray");
	tieFirst->WorldCoordinate.z = tieSecond->WorldCoordinate.z = 6.0f;
	YMGRE_Object_LocalToWorld(tieFirst); YMGRE_Object_LocalToWorld(tieSecond);
	gre_list ties = { 0 };
	YMGRE_List_Append(&ties, sizeof(GRE_Object4d), tieFirst);
	YMGRE_List_Append(&ties, sizeof(GRE_Object4d), tieSecond);
	gre_ray_scene_hit tieHit = { 0 };
	pass = pass && YMGRE_Ray_IntersectScene(&ray, &ties, .001f, 100.0f, &tieHit);
	pass = pass && tieHit.object == tieFirst;
	YMGRE_List_Clear(&ties, YMGRE_Free_Object);
	gre_camera4d camera = { 0 };
	camera.img.width = 100; camera.img.height = 100;
	camera.perspectPlane.Dis = 1.0f;
	camera.perspectPlane.pL = -1.0f; camera.perspectPlane.pR = 1.0f;
	camera.perspectPlane.pD = -1.0f; camera.perspectPlane.pU = 1.0f;
	camera.move.cu = (gre_fvector4d){ 1, 0, 0, 0 };
	camera.move.cv = (gre_fvector4d){ 0, 1, 0, 0 };
	camera.move.cn = (gre_fvector4d){ 0, 0, 1, 0 };
	camera.pos = (gre_fvector4d){ 0, 0, 0, 1 };
	gre_ray cameraRay = { 0 };
	int cameraPass = YMGRE_Ray_FromCameraPixel(&camera, 49, 49, &cameraRay);
	cameraPass = cameraPass && cameraRay.origin.w == 1.0f && cameraRay.direction.z > 0.99f;
	pass = pass && cameraPass;
	gre_fvector4d incident = { 0, -0.6f, 0.8f, 0 }, normal = { 0, 1, 0, 0 }, reflected;
	YMGRE_Ray_Reflect(&incident, &normal, &reflected);
	pass = pass && reflected.y > 0.59f;
	pass = pass && YMGRE_Fabs(YMGRE_Fvector4d_Len1(&reflected) - 1.0f) < 1e-5f;
	gre_fvector4d refracted;
	pass = pass && YMGRE_Ray_Refract(&incident, &normal, 1.0f, 1.5f, &refracted);
	pass = pass && refracted.y < -0.8f && refracted.z > 0.5f;
	/* A planar mirror can be evaluated either with a virtual camera and the
	 * real scene, or with a mirrored scene and the real camera. */
	const float32 mirrorY = -1.4f;
	gre_fvector4d cameraReal = { 5, 4, 0, 1 };
	gre_fvector4d pointReal = { .3f, .2f, 7.6f, 1 };
	gre_fvector4d lightReal = { 0, 0, 5, 1 };
	gre_fvector4d normalReal = { .0f, .6f, -.8f, 0 };
	gre_fvector4d cameraVirtual = cameraReal;
	cameraVirtual.y = 2.0f * mirrorY - cameraReal.y;
	gre_fvector4d viewA = { cameraVirtual.x-pointReal.x, cameraVirtual.y-pointReal.y,
		cameraVirtual.z-pointReal.z, 0 };
	gre_fvector4d lightA = { lightReal.x-pointReal.x, lightReal.y-pointReal.y,
		lightReal.z-pointReal.z, 0 };
	YMGRE_Fvector4d_Normalize(&viewA); YMGRE_Fvector4d_Normalize(&lightA);
	gre_fvector4d halfA = { viewA.x+lightA.x, viewA.y+lightA.y, viewA.z+lightA.z, 0 };
	YMGRE_Fvector4d_Normalize(&halfA);
	float32 specA = YMGRE_Fvector4d_Dot(&normalReal, &halfA);
	gre_fvector4d pointVirtual = pointReal, lightVirtual = lightReal, normalVirtual = normalReal;
	pointVirtual.y = 2.0f * mirrorY - pointReal.y;
	lightVirtual.y = 2.0f * mirrorY - lightReal.y;
	normalVirtual.y = -normalReal.y;
	gre_fvector4d viewB = { cameraReal.x-pointVirtual.x, cameraReal.y-pointVirtual.y,
		cameraReal.z-pointVirtual.z, 0 };
	gre_fvector4d lightB = { lightVirtual.x-pointVirtual.x, lightVirtual.y-pointVirtual.y,
		lightVirtual.z-pointVirtual.z, 0 };
	YMGRE_Fvector4d_Normalize(&viewB); YMGRE_Fvector4d_Normalize(&lightB);
	gre_fvector4d halfB = { viewB.x+lightB.x, viewB.y+lightB.y, viewB.z+lightB.z, 0 };
	YMGRE_Fvector4d_Normalize(&halfB);
	float32 specB = YMGRE_Fvector4d_Dot(&normalVirtual, &halfB);
	pass = pass && YMGRE_Fabs(specA-specB) < 1e-5f;
	gre_fvector4d shadePoint = {0, 0, 5, 1}, shadeNormal = {0, 0, -1, 0};
	gre_fvector4d shadeView = {0, 0, -1, 0}, shadeLight = {0, 0, 0, 1};
	GRErgb24 shaded = YMGRE_Ray_ShadeBlinnPhong((GRErgb24){180, 40, 30}, &shadePoint,
		&shadeNormal, &shadeView, &shadeLight, (GRErgb24){255,255,255}, .1f, 4.0f, .01f, 180.0f, 24.0f);
	pass = pass && shaded.R > 200 && shaded.G > 40 && shaded.B > 30;
	printf("ray triangle intersection: %s\n", pass ? "PASS" : "FAIL");
	return pass ? 0 : 1;
}
