#include "YMGRE_CullingAndClipping.h"
#include <stdio.h>

int main(void)
{
	gre_vertex4d vertex[3] = {
		{ { -10, -10, 100, 1 }, 0, 0 },
		{ { 10, -10, 100, 1 }, 1, 0 },
		{ { 0, 10, 100, 1 }, 0.5f, 1 }
	};
	GRE_Index frontIndex[3] = { 0, 1, 2 };
	GRE_Index backIndex[3] = { 0, 2, 1 };
	gre_polygon4d polygon[2] = { 0 };
	gre_object4d object = { 0 };
	uint8 polygonHide[2] = { 0 };
	gre_fvector4d cameraFront = { 0, 0, 0, 1 };
	gre_fvector4d cameraBack = { 0, 0, 200, 1 };
	polygon[0].num = 3;
	polygon[0].index = frontIndex;
	polygon[0].pN = (gre_fvector4d){ 0, 0, -1, 0 };
	polygon[1].num = 3;
	polygon[1].index = backIndex;
	polygon[1].pN = (gre_fvector4d){ 0, 0, 1, 0 };
	object.pointNum = 3;
	object.pointList = vertex;
	object.polygonNum = 2;
	object.polygonList = polygon;

	YMGRE_Backface_RemoveTo(&object, &cameraFront, polygonHide);
	if ((polygonHide[0] != 0) || (polygonHide[1] != 1))
	{
		printf("test_backface_culling: front camera visibility FAILED\n");
		return 1;
	}
	YMGRE_Backface_RemoveTo(&object, &cameraBack, polygonHide);
	if ((polygonHide[0] != 1) || (polygonHide[1] != 0))
	{
		printf("test_backface_culling: rear camera visibility FAILED\n");
		return 1;
	}
	if ((polygon[0].ishide != 0) || (polygon[1].ishide != 0))
	{
		printf("test_backface_culling: source state changed FAILED\n");
		return 1;
	}
	printf("test_backface_culling: winding and camera side PASS\n");
	return 0;
}
