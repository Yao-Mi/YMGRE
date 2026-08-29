#ifndef YMGRE_RAY_TRACING_H
#define YMGRE_RAY_TRACING_H

#include "../OPOBJ/YMGRE_OBJ.h"

typedef struct gre_ray_
{
	gre_fvector4d origin;
	gre_fvector4d direction;
} gre_ray;
typedef gre_ray* GRE_Ray;

typedef struct gre_ray_hit_
{
	float32 distance;
	float32 u;
	float32 v;
	gre_fvector4d position;
	gre_fvector4d normal;
} gre_ray_hit;
typedef gre_ray_hit* GRE_RayHit;

// Moller-Trumbore intersection. Returns 1 for a hit in [tMin, tMax].
int YMGRE_Ray_IntersectTriangle(const GRE_Ray ray,
	const gre_fvector4d* p0, const gre_fvector4d* p1, const gre_fvector4d* p2,
	float32 tMin, float32 tMax, GRE_RayHit hit);

#endif
