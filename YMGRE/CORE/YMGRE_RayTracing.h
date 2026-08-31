#ifndef YMGRE_RAY_TRACING_H
#define YMGRE_RAY_TRACING_H

#include "../OPOBJ/YMGRE_OBJ.h"
#include "YMGRE_List.h"
#include "YMGRE_Camera.h"

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
typedef struct gre_ray_scene_hit_{GRE_Object4d object;uint32 polygonIndex;gre_ray_hit hit;} gre_ray_scene_hit;

// Moller-Trumbore intersection. Returns 1 for a hit in [tMin, tMax].
int YMGRE_Ray_IntersectTriangle(const GRE_Ray ray,
	const gre_fvector4d* p0, const gre_fvector4d* p1, const gre_fvector4d* p2,
	float32 tMin, float32 tMax, GRE_RayHit hit);
int YMGRE_Ray_IntersectObject(const GRE_Ray ray, GRE_Object4d object,
	float32 tMin, float32 tMax, gre_ray_scene_hit* result);
int YMGRE_Ray_IntersectScene(const GRE_Ray ray, const gre_list* objects,
	float32 tMin, float32 tMax, gre_ray_scene_hit* result);
int YMGRE_Ray_FromCameraPixel(GRE_Camera4d camera, uint16 pixelX, uint16 pixelY,
	GRE_Ray result);
void YMGRE_Ray_Reflect(const gre_fvector4d* incident, const gre_fvector4d* normal,
	gre_fvector4d* result);
int YMGRE_Ray_Refract(const gre_fvector4d* incident, const gre_fvector4d* normal,
	float32 etaIncident, float32 etaTransmitted, gre_fvector4d* result);
void YMGRE_Ray_SpawnFromSurface(const gre_fvector4d* position,
	const gre_fvector4d* normal, const gre_fvector4d* direction,
	float32 bias, GRE_Ray result);
GRErgb24 YMGRE_Ray_ShadeBlinnPhong(GRErgb24 baseColor,
	const gre_fvector4d* position, const gre_fvector4d* normal,
	const gre_fvector4d* viewDirection, const gre_fvector4d* lightPosition,
	GRErgb24 lightColor, float32 ambient, float32 lightIntensity,
	float32 attenuation, float32 specularStrength, float32 specularPower);

#endif
