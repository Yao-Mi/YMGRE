#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_HeightMap.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_MathBase.h"
#include <stdio.h>

int main(void)
{
	GRE_Object4d plane = YMGRE_MeshGener_RectPlane(2.0f, 2.0f, 1, 1,
		(GRErgb24){ 255, 255, 255 }, "height_test", "height_test");
	GRErgb24 height[4] = {
		{ 0, 0, 0 }, { 255, 255, 255 },
		{ 0, 0, 0 }, { 255, 255, 255 }
	};
	int applied = YMGRE_Object_ApplyHeightMap(plane, height, 2, 2, 4.0f, 0.0f);
	int passed = applied && YMGRE_Fabs(plane->pointList[0].pos.y) < 0.01f &&
		YMGRE_Fabs(plane->pointList[1].pos.y - 4.0f) < 0.02f &&
		plane->pointList_wN != NULL && plane->BoundingBoxMax.y > 3.9f;
	plane->WorldCoordinate = (gre_fvector4d){ 0, 0, 12, 1 };
	YMGRE_Object_LocalToWorld_wN(plane);
	passed = passed && YMGRE_Fabs(plane->pointList_wN[1].base.pos.z - 11.0f) < 0.01f;
	printf("height map displacement: %s\n", passed ? "PASS" : "FAIL");
	YMGRE_Free_Object(plane);
	return passed ? 0 : 1;
}
