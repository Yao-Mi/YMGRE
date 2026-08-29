#include "YMGRE_RayTracing.h"
#include "YMGRE_MathBase.h"
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
	printf("ray triangle intersection: %s\n", pass ? "PASS" : "FAIL");
	return pass ? 0 : 1;
}
