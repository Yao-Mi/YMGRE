#include "YMGRE_HeightMap.h"
#include "YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Creat.h"

static float32 heightMapLuminance(GRErgb24 pixel)
{
	return (77.0f * pixel.R + 150.0f * pixel.G + 29.0f * pixel.B) / (255.0f * 256.0f);
}

static uint16 heightMapCoordinate(float32 value, uint16 size)
{
	if (value <= 0.0f) return 0;
	if (value >= 1.0f) return size - 1;
	return (uint16)(value * (size - 1) + 0.5f);
}

int YMGRE_Object_ApplyHeightMap(GRE_Object4d object, const GRErgb24* heightMap,
	uint16 width, uint16 height, float32 heightScale, float32 heightBias)
{
	if (object == NULL || object->pointList == NULL || heightMap == NULL ||
		width == 0 || height == 0 || object->pointNum <= 0)
		return 0;
	if (!YMGRE_Object_GenerateVertexAttributes(object)) return 0;

	gre_fvector4d boundsMin = { 0 }, boundsMax = { 0 };
	float32 maxRadiusSquared = 0.0f;
	for (int i = 0; i < object->pointNum; i++)
	{
		GRE_Vertex4d vertex = &object->pointList[i];
		GRE_Vertex4d_wN attributes = &object->pointList_wN[i];
		uint16 x = heightMapCoordinate(vertex->u, width);
		uint16 y = heightMapCoordinate(vertex->v, height);
		float32 displacement = (heightMapLuminance(heightMap[y * width + x]) - heightBias) * heightScale;
		gre_fvector4d direction = attributes->normal;
		// Meshes such as the earth demo may use the opposite winding. For
		// radial meshes, make positive heightScale consistently point outward.
		float32 radialDot = direction.x * vertex->pos.x + direction.y * vertex->pos.y + direction.z * vertex->pos.z;
		if (radialDot < 0.0f)
		{
			direction.x = -direction.x; direction.y = -direction.y; direction.z = -direction.z;
		}
		vertex->pos.x += direction.x * displacement;
		vertex->pos.y += direction.y * displacement;
		vertex->pos.z += direction.z * displacement;
		object->pointList_[i] = *vertex;

		if (i == 0) boundsMin = boundsMax = vertex->pos;
		else
		{
			boundsMin.x = GREMin(boundsMin.x, vertex->pos.x);
			boundsMin.y = GREMin(boundsMin.y, vertex->pos.y);
			boundsMin.z = GREMin(boundsMin.z, vertex->pos.z);
			boundsMax.x = GREMax(boundsMax.x, vertex->pos.x);
			boundsMax.y = GREMax(boundsMax.y, vertex->pos.y);
			boundsMax.z = GREMax(boundsMax.z, vertex->pos.z);
		}
		float32 radiusSquared = vertex->pos.x * vertex->pos.x +
			vertex->pos.y * vertex->pos.y + vertex->pos.z * vertex->pos.z;
		maxRadiusSquared = GREMax(maxRadiusSquared, radiusSquared);
	}
	object->BoundingBoxMin = boundsMin;
	object->BoundingBoxMax = boundsMax;
	object->BoundingSphereR = YMGRE_Sqrt(maxRadiusSquared);
	return YMGRE_Object_GenerateVertexAttributes(object);
}
