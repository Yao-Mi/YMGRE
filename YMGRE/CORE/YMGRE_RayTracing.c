#include "YMGRE_RayTracing.h"
#include "YMGRE_MathBase.h"

static int rayIntersectsBoundSphere(const GRE_Ray ray, GRE_Object4d object,
	float32 tMin, float32 tMax)
{
	if (object->boundType != GRE_Bounding_Sphere_R || object->BoundingSphereR <= 0.0f)
		return 1;
	gre_fvector4d center = object->WorldCoordinate;
	float32 radius = object->BoundingSphereR * ((object->scale > 0.0f) ? object->scale : 1.0f);
	float32 ox = ray->origin.x - center.x, oy = ray->origin.y - center.y, oz = ray->origin.z - center.z;
	float32 b = ox * ray->direction.x + oy * ray->direction.y + oz * ray->direction.z;
	float32 c = ox * ox + oy * oy + oz * oz - radius * radius;
	float32 disc = b * b - c;
	if (disc < 0.0f) return 0;
	float32 root = YMGRE_Sqrt(disc);
	float32 nearT = -b - root, farT = -b + root;
	return farT >= tMin && nearT <= tMax;
}

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

int YMGRE_Ray_IntersectSphere(const GRE_Ray ray, const gre_fvector4d* center,
	float32 radius, float32 tMin, float32 tMax, GRE_RayHit hit)
{
	if (ray == NULL || center == NULL || hit == NULL || radius <= 0.0f) return 0;
	float32 ox = ray->origin.x - center->x, oy = ray->origin.y - center->y,
		 oz = ray->origin.z - center->z;
	float32 a = ray->direction.x * ray->direction.x + ray->direction.y * ray->direction.y + ray->direction.z * ray->direction.z;
	if (a < 1e-12f) return 0;
	float32 halfB = ox * ray->direction.x + oy * ray->direction.y + oz * ray->direction.z;
	float32 c = ox * ox + oy * oy + oz * oz - radius * radius;
	float32 discriminant = halfB * halfB - a * c;
	if (discriminant < 0.0f) return 0;
	float32 root = YMGRE_Sqrt(discriminant), t = (-halfB - root) / a;
	if (t < tMin || t > tMax) { t = (-halfB + root) / a; if (t < tMin || t > tMax) return 0; }
	hit->distance = t; hit->u = hit->v = 0.0f;
	hit->position.x = ray->origin.x + ray->direction.x * t;
	hit->position.y = ray->origin.y + ray->direction.y * t;
	hit->position.z = ray->origin.z + ray->direction.z * t;
	hit->position.w = 1.0f;
	hit->normal.x = (hit->position.x - center->x) / radius;
	hit->normal.y = (hit->position.y - center->y) / radius;
	hit->normal.z = (hit->position.z - center->z) / radius;
	hit->normal.w = 0.0f;
	return 1;
}

int YMGRE_Ray_IntersectObject(const GRE_Ray ray, GRE_Object4d object,
	float32 tMin, float32 tMax, gre_ray_scene_hit* result)
{
	if (ray == NULL || object == NULL || result == NULL || object->pointList == NULL) return 0;
	if (!rayIntersectsBoundSphere(ray, object, tMin, tMax)) return 0;
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
			float32 tieEpsilon = 1e-5f * ((closest > 1.0f) ? closest : 1.0f);
			if (found && candidate.hit.distance >= closest - tieEpsilon) continue;
			found = 1;
			closest = candidate.hit.distance;
			*result = candidate;
		}
	}
	return found;
}

int YMGRE_Ray_FromCameraPixel(GRE_Camera4d camera, uint16 pixelX, uint16 pixelY,
	GRE_Ray result)
{
	if (camera == NULL || result == NULL || camera->img.width == 0 || camera->img.height == 0)
		return 0;
	if (pixelX >= camera->img.width || pixelY >= camera->img.height) return 0;
	float32 fx = ((float32)pixelX + 0.5f) / (float32)camera->img.width;
	float32 fy = ((float32)pixelY + 0.5f) / (float32)camera->img.height;
	float32 viewX = camera->perspectPlane.pL +
		(camera->perspectPlane.pR - camera->perspectPlane.pL) * fx;
	float32 viewY = camera->perspectPlane.pU -
		(camera->perspectPlane.pU - camera->perspectPlane.pD) * fy;
	result->origin = camera->pos;
	result->origin.w = 1.0f;
	result->direction.x = camera->move.cu.x * viewX + camera->move.cv.x * viewY +
		camera->move.cn.x * camera->perspectPlane.Dis;
	result->direction.y = camera->move.cu.y * viewX + camera->move.cv.y * viewY +
		camera->move.cn.y * camera->perspectPlane.Dis;
	result->direction.z = camera->move.cu.z * viewX + camera->move.cv.z * viewY +
		camera->move.cn.z * camera->perspectPlane.Dis;
	result->direction.w = 0.0f;
	if (YMGRE_Fvector4d_Len2(&result->direction) < 1e-12f) return 0;
	YMGRE_Fvector4d_Normalize(&result->direction);
	return 1;
}

void YMGRE_Ray_Reflect(const gre_fvector4d* incident, const gre_fvector4d* normal,
	gre_fvector4d* result)
{
	if (incident == NULL || normal == NULL || result == NULL) return;
	float32 d = incident->x * normal->x + incident->y * normal->y + incident->z * normal->z;
	d *= 2.0f;
	result->x = incident->x - d * normal->x;
	result->y = incident->y - d * normal->y;
	result->z = incident->z - d * normal->z;
	result->w = 0.0f;
	if (YMGRE_Fvector4d_Len2(result) > 1e-12f) YMGRE_Fvector4d_Normalize(result);
}

int YMGRE_Ray_Refract(const gre_fvector4d* incident, const gre_fvector4d* normal,
	float32 etaIncident, float32 etaTransmitted, gre_fvector4d* result)
{
	if (incident == NULL || normal == NULL || result == NULL || etaIncident <= 0.0f || etaTransmitted <= 0.0f) return 0;
	gre_fvector4d n = *normal;
	float32 cosi = -(incident->x * n.x + incident->y * n.y + incident->z * n.z);
	if (cosi < 0.0f) { cosi = -cosi; n.x = -n.x; n.y = -n.y; n.z = -n.z; }
	float32 eta = etaIncident / etaTransmitted;
	float32 k = 1.0f - eta * eta * (1.0f - cosi * cosi);
	if (k < 0.0f) return 0;
	float32 a = eta * cosi - YMGRE_Sqrt(k);
	result->x = eta * incident->x + a * n.x;
	result->y = eta * incident->y + a * n.y;
	result->z = eta * incident->z + a * n.z;
	result->w = 0.0f;
	YMGRE_Fvector4d_Normalize(result);
	return 1;
}

float32 YMGRE_Ray_FresnelSchlick(float32 cosTheta,
	float32 etaIncident, float32 etaTransmitted)
{
	if (etaIncident <= 0.0f || etaTransmitted <= 0.0f) return 1.0f;
	if (cosTheta < 0.0f) cosTheta = -cosTheta;
	if (cosTheta > 1.0f) cosTheta = 1.0f;
	float32 r0 = (etaIncident - etaTransmitted) / (etaIncident + etaTransmitted);
	r0 *= r0;
	float32 m = 1.0f - cosTheta;
	return r0 + (1.0f - r0) * m * m * m * m * m;
}

void YMGRE_Ray_SpawnFromSurface(const gre_fvector4d* position,
	const gre_fvector4d* normal, const gre_fvector4d* direction,
	float32 bias, GRE_Ray result)
{
	if (position == NULL || normal == NULL || direction == NULL || result == NULL) return;
	if (bias < 0.0f) bias = -bias;
	float32 side = direction->x * normal->x + direction->y * normal->y + direction->z * normal->z;
	float32 offset = side >= 0.0f ? bias : -bias;
	result->origin.x = position->x + normal->x * offset;
	result->origin.y = position->y + normal->y * offset;
	result->origin.z = position->z + normal->z * offset;
	result->origin.w = 1.0f;
	result->direction = *direction;
	result->direction.w = 0.0f;
}

GRErgb24 YMGRE_Ray_ShadeBlinnPhong(GRErgb24 baseColor,
	const gre_fvector4d* position, const gre_fvector4d* normal,
	const gre_fvector4d* viewDirection, const gre_fvector4d* lightPosition,
	GRErgb24 lightColor, float32 ambient, float32 lightIntensity,
	float32 attenuation, float32 specularStrength, float32 specularPower)
{
	if (position == NULL || normal == NULL || viewDirection == NULL || lightPosition == NULL)
		return (GRErgb24){0, 0, 0};
	if (ambient < 0) ambient = 0; if (ambient > 1) ambient = 1;
	if (specularPower < 1) specularPower = 1;
	gre_fvector4d l = { lightPosition->x - position->x, lightPosition->y - position->y,
		lightPosition->z - position->z, 0 };
	float32 distance = YMGRE_Fvector4d_Len1(&l);
	if (distance > 1e-8f) YMGRE_Fvector4d_Normalize(&l);
	gre_fvector4d v = *viewDirection;
	if (YMGRE_Fvector4d_Len2(&v) > 1e-8f) YMGRE_Fvector4d_Normalize(&v);
	float32 ndotl = normal->x*l.x + normal->y*l.y + normal->z*l.z;
	if (ndotl < 0) ndotl = 0;
	float32 factor = 1.0f / (1.0f + attenuation * distance * distance);
	float32 diffuse = lightIntensity * factor * ndotl;
	gre_fvector4d h = { l.x + v.x, l.y + v.y, l.z + v.z, 0 };
	float32 specular = 0;
	if (ndotl > 0 && YMGRE_Fvector4d_Len2(&h) > 1e-8f) {
		YMGRE_Fvector4d_Normalize(&h);
		float32 ndoth = normal->x*h.x + normal->y*h.y + normal->z*h.z;
		if (ndoth > 0) specular = specularStrength * factor * YMGRE_Pow(ndoth, specularPower);
	}
	float32 r = baseColor.R * (ambient + diffuse * lightColor.R / 255.0f) + specular * lightColor.R / 255.0f;
	float32 g = baseColor.G * (ambient + diffuse * lightColor.G / 255.0f) + specular * lightColor.G / 255.0f;
	float32 b = baseColor.B * (ambient + diffuse * lightColor.B / 255.0f) + specular * lightColor.B / 255.0f;
	if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	return (GRErgb24){ (uint8)r, (uint8)g, (uint8)b };
}
