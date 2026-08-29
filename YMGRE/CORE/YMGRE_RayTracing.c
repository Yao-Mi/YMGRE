#include "YMGRE_RayTracing.h"
#include "YMGRE_MathBase.h"

int YMGRE_Ray_IntersectTriangle(const GRE_Ray ray,
	const gre_fvector4d* p0, const gre_fvector4d* p1, const gre_fvector4d* p2,
	float32 tMin, float32 tMax, GRE_RayHit hit)
{
	if (ray == NULL || p0 == NULL || p1 == NULL || p2 == NULL || hit == NULL) return 0;
	gre_fvector4d e1 = { p1->x - p0->x, p1->y - p0->y, p1->z - p0->z, 0 };
	gre_fvector4d e2 = { p2->x - p0->x, p2->y - p0->y, p2->z - p0->z, 0 };
	gre_fvector4d pvec;
	YMGRE_Fvector4d_CrossToResult(&ray->direction, &e2, &pvec);
	float32 det = YMGRE_Fvector4d_Dot(&e1, &pvec);
	if (YMGRE_Fabs(det) < 1e-7f) return 0;
	float32 invDet = 1.0f / det;
	gre_fvector4d tvec = { ray->origin.x - p0->x, ray->origin.y - p0->y, ray->origin.z - p0->z, 0 };
	float32 baryU = YMGRE_Fvector4d_Dot(&tvec, &pvec) * invDet;
	if (baryU < 0.0f || baryU > 1.0f) return 0;
	gre_fvector4d qvec;
	YMGRE_Fvector4d_CrossToResult(&tvec, &e1, &qvec);
	float32 baryV = YMGRE_Fvector4d_Dot(&ray->direction, &qvec) * invDet;
	if (baryV < 0.0f || baryU + baryV > 1.0f) return 0;
	float32 distance = YMGRE_Fvector4d_Dot(&e2, &qvec) * invDet;
	if (distance < tMin || distance > tMax) return 0;
	hit->distance = distance; hit->u = baryU; hit->v = baryV;
	hit->position.x = ray->origin.x + ray->direction.x * distance;
	hit->position.y = ray->origin.y + ray->direction.y * distance;
	hit->position.z = ray->origin.z + ray->direction.z * distance;
	YMGRE_Fvector4d_CrossToResult(&e1, &e2, &hit->normal);
	if (YMGRE_Fvector4d_Len1(&hit->normal) > 1e-8f) YMGRE_Fvector4d_Normalize(&hit->normal);
	hit->normal.w = 0;
	return 1;
}

int YMGRE_Ray_IntersectObject(const GRE_Ray ray, GRE_Object4d object,
	float32 tMin, float32 tMax, gre_ray_scene_hit* result)
{
	if (ray == NULL || object == NULL || result == NULL || object->pointList == NULL) return 0;
	int found = 0; float32 closest = tMax; gre_ray_hit candidate;
	for (uint32 pi = 0; pi < (uint32)object->polygonNum; pi++)
	{
		GRE_Polygon4d polygon = &object->polygonList[pi];
		if (polygon->index == NULL || polygon->num < 3) continue;
		uint16 first = polygon->index[0];
		for (int corner = 1; corner + 1 < polygon->num; corner++)
		{
			uint16 i1 = polygon->index[corner], i2 = polygon->index[corner + 1];
			if (first >= object->pointNum || i1 >= object->pointNum || i2 >= object->pointNum) continue;
			if (!YMGRE_Ray_IntersectTriangle(ray, &object->pointList[first].pos,
				&object->pointList[i1].pos, &object->pointList[i2].pos,
				tMin, closest, &candidate)) continue;
			found = 1; closest = candidate.distance;
			result->object = object; result->polygonIndex = pi; result->hit = candidate;
		}
	}
	return found;
}

int YMGRE_Ray_IntersectScene(const GRE_Ray ray, const gre_list* objects,
	float32 tMin, float32 tMax, gre_ray_scene_hit* result)
{
	if (ray == NULL || objects == NULL || result == NULL) return 0;
	int found = 0;
	float32 closest = tMax;
	for (GRE_ListNode node = objects->listhead; node != NULL; node = node->next)
	{
		GRE_Object4d object = (GRE_Object4d)node->data;
		gre_ray_scene_hit candidate;
		if (YMGRE_Ray_IntersectObject(ray, object, tMin, closest, &candidate))
		{
			found = 1;
			closest = candidate.hit.distance;
			*result = candidate;
		}
	}
	return found;
}
