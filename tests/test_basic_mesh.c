#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Free.h"
#include <stdio.h>

static int fails;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void checkMesh(GRE_Object4d object, int pointNum, int polygonNum,
	const char* name, uint8 checkOutward)
{
	CHECK(object != NULL, name);
	CHECK(object->pointNum == pointNum, "mesh point count");
	CHECK(object->polygonNum == polygonNum, "mesh polygon count");
	CHECK(object->boundType == GRE_Bounding_Sphere_R && object->BoundingSphereR > 0.0f,
		"mesh bounding sphere");
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		CHECK(polygon->num == 3 && polygon->index != NULL, "mesh uses triangle polygons");
		for (int j = 0; j < polygon->num; j++)
			CHECK(polygon->index[j] < object->pointNum, "mesh index is in range");
		float32 normalLen2 = polygon->pN.x * polygon->pN.x + polygon->pN.y * polygon->pN.y +
			polygon->pN.z * polygon->pN.z;
		CHECK(normalLen2 > 0.0f, "mesh triangle is not degenerate");
		gre_fvector4d center = { 0 };
		for (int j = 0; j < 3; j++)
		{
			GRE_Fvector4d point = &object->pointList[polygon->index[j]].pos;
			center.x += point->x;
			center.y += point->y;
			center.z += point->z;
		}
		if (checkOutward)
			CHECK(center.x * polygon->pN.x + center.y * polygon->pN.y +
				center.z * polygon->pN.z > 0.0f, "mesh triangle normal faces outward");
	}
}

static void checkTorus(GRE_Object4d object, int pointNum, int polygonNum, float32 majorRadius)
{
	checkMesh(object, pointNum, polygonNum, "torus is created", 0);
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		gre_fvector4d center = { 0 };
		for (int j = 0; j < 3; j++)
		{
			GRE_Fvector4d point = &object->pointList[polygon->index[j]].pos;
			center.x += point->x / 3.0f;
			center.y += point->y / 3.0f;
			center.z += point->z / 3.0f;
		}
		float32 radialLength = YMGRE_Sqrt(center.x * center.x + center.z * center.z);
		float32 coreX = majorRadius * center.x / radialLength;
		float32 coreZ = majorRadius * center.z / radialLength;
		CHECK((center.x - coreX) * polygon->pN.x + center.y * polygon->pN.y +
			(center.z - coreZ) * polygon->pN.z > 0.0f, "torus triangle normal faces outward");
	}
}

static void checkPlane(GRE_Object4d object, int pointNum, int polygonNum)
{
	CHECK(object != NULL, "plane is created");
	CHECK(object->pointNum == pointNum, "plane point count");
	CHECK(object->polygonNum == polygonNum, "plane polygon count");
	CHECK(object->boundType == GRE_Bounding_Sphere_R && object->BoundingSphereR > 0.0f,
		"plane bounding sphere");
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		CHECK(polygon->num == 3 && polygon->index != NULL, "plane uses triangle polygons");
		for (int j = 0; j < polygon->num; j++)
			CHECK(polygon->index[j] < object->pointNum, "plane index is in range");
		CHECK(polygon->pN.y > 0.0f, "plane normal faces upward");
	}
}

int main(void)
{
	GRErgb24 color = { 120, 160, 200 };
	GRE_Object4d plane = YMGRE_MeshGener_RectPlane(20.0f, 30.0f, 3, 4,
		color, "plane", "");
	GRE_Object4d cube = YMGRE_MeshGener_Cube(20.0f, color, "cube", "");
	GRE_Object4d box = YMGRE_MeshGener_Box(20.0f, 30.0f, 40.0f, color, "box", "");
	GRE_Object4d cylinder = YMGRE_MeshGener_Cylinder(10.0f, 30.0f, 12, color, "cylinder", "");
	GRE_Object4d cone = YMGRE_MeshGener_Cone(10.0f, 30.0f, 12, color, "cone", "");
	GRE_Object4d sphere = YMGRE_MeshGener_Sphere(10.0f, 6, 12, color, "sphere", "");
	GRE_Object4d torus = YMGRE_MeshGener_Torus(14.0f, 4.0f, 16, 8, color, "torus", "");
	GRE_Object4d capsule = YMGRE_MeshGener_Capsule(8.0f, 18.0f, 4, 12,
		color, "capsule", "");
	GRE_Object4d tetrahedron = YMGRE_MeshGener_Tetrahedron(10.0f, color, "tetrahedron", "");
	GRE_Object4d octahedron = YMGRE_MeshGener_Octahedron(10.0f, color, "octahedron", "");
	GRE_Object4d dodecahedron = YMGRE_MeshGener_Dodecahedron(10.0f, color, "dodecahedron", "");
	GRE_Object4d icosahedron = YMGRE_MeshGener_Icosahedron(10.0f, color, "icosahedron", "");
	checkPlane(plane, 20, 24);
	checkMesh(cube, 8, 12, "cube is created", 1);
	checkMesh(box, 8, 12, "box is created", 1);
	checkMesh(cylinder, 26, 48, "cylinder is created", 1);
	checkMesh(cone, 14, 24, "cone is created", 1);
	checkMesh(sphere, 62, 120, "sphere is created", 1);
	CHECK(sphere->pointList[1].v > 0.0f && sphere->pointList[1].u == 0.0f, "sphere UVs are initialized");
	checkTorus(torus, 128, 256, 14.0f);
	checkMesh(capsule, 98, 192, "capsule is created", 1);
	checkMesh(tetrahedron, 4, 4, "tetrahedron is created", 1);
	checkMesh(octahedron, 6, 8, "octahedron is created", 1);
	checkMesh(dodecahedron, 20, 36, "dodecahedron is created", 1);
	checkMesh(icosahedron, 12, 20, "icosahedron is created", 1);
	YMGRE_Free_Object(plane);
	YMGRE_Free_Object(cube);
	YMGRE_Free_Object(box);
	YMGRE_Free_Object(cylinder);
	YMGRE_Free_Object(cone);
	YMGRE_Free_Object(sphere);
	YMGRE_Free_Object(torus);
	YMGRE_Free_Object(capsule);
	YMGRE_Free_Object(tetrahedron);
	YMGRE_Free_Object(octahedron);
	YMGRE_Free_Object(dodecahedron);
	YMGRE_Free_Object(icosahedron);

	if (fails == 0)
		printf("test_basic_mesh: ALL PASS\n");
	else
		printf("test_basic_mesh: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
