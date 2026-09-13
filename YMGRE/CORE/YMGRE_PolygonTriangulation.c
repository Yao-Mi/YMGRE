#include "YMGRE_PolygonTriangulation.h"
#include "YMGRE_MathBase.h"

// 计算二维叉积，符号同时表示三个点的绕序方向。
static float32 cross2(const gre_fvector4d* a, const gre_fvector4d* b, const gre_fvector4d* c)
{
	return (b->x - a->x) * (c->y - a->y) - (b->y - a->y) * (c->x - a->x);
}

static uint8 pointInTriangle(const gre_fvector4d* p, const gre_fvector4d* a,
	const gre_fvector4d* b, const gre_fvector4d* c)
{
	float32 c0 = cross2(a, b, p);
	float32 c1 = cross2(b, c, p);
	float32 c2 = cross2(c, a, p);
	uint8 hasNeg = (c0 < -1e-6f) || (c1 < -1e-6f) || (c2 < -1e-6f);
	uint8 hasPos = (c0 > 1e-6f) || (c1 > 1e-6f) || (c2 > 1e-6f);
	return !(hasNeg && hasPos);
}

// 对简单多边形执行耳切：每次移除一个不包含其他顶点的凸耳。
uint16 YMGRE_Polygon_Triangulate(const gre_fvector4d* vertices, uint16 vertexNum,
	GRE_Index* triangleIndices, uint16 triangleCapacity)
{
	if (vertices == NULL || triangleIndices == NULL || vertexNum < 3 ||
		triangleCapacity < vertexNum - 2)
		return 0;
	// 用有向面积统一输入绕序，使后续耳朵始终按逆时针判断。
	float32 area = 0.0f;
	for (uint16 i = 0; i < vertexNum; i++)
	{
		uint16 next = (i + 1) % vertexNum;
		area += vertices[i].x * vertices[next].y - vertices[next].x * vertices[i].y;
	}
	if (area > -1e-6f && area < 1e-6f)
		return 0;
	uint16 order[vertexNum];
	if (area > 0.0f)
		for (uint16 i = 0; i < vertexNum; i++) order[i] = i;
	else
		for (uint16 i = 0; i < vertexNum; i++) order[i] = vertexNum - 1 - i;

	uint16 remaining = vertexNum;
	uint16 triangles = 0;
	uint16 guard = 0;
	while (remaining > 3 && guard++ < vertexNum * vertexNum)
	{
		uint8 clipped = 0;
		for (uint16 i = 0; i < remaining; i++)
		{
			uint16 prev = (i + remaining - 1) % remaining;
			uint16 next = (i + 1) % remaining;
			const gre_fvector4d* a = &vertices[order[prev]];
			const gre_fvector4d* b = &vertices[order[i]];
			const gre_fvector4d* c = &vertices[order[next]];
			// 顺时针或近共线的角不是有效耳朵。
			if (cross2(a, b, c) <= 1e-6f)
				continue;
			uint8 contains = 0;
			// 耳朵内部不能包含多边形的其他顶点，否则会越过边界。
			for (uint16 j = 0; j < remaining; j++)
			{
				if (j == prev || j == i || j == next) continue;
				if (pointInTriangle(&vertices[order[j]], a, b, c)) { contains = 1; break; }
			}
			if (contains) continue;
			triangleIndices[triangles * 3 + 0] = order[prev];
			triangleIndices[triangles * 3 + 1] = order[i];
			triangleIndices[triangles * 3 + 2] = order[next];
			triangles++;
			for (uint16 j = i; j + 1 < remaining; j++) order[j] = order[j + 1];
			remaining--;
			clipped = 1;
			break;
		}
		// 没有找到耳朵通常意味着输入自交或存在未处理的退化点。
		if (!clipped) return 0;
	}
	if (remaining == 3)
	{
		triangleIndices[triangles * 3 + 0] = order[0];
		triangleIndices[triangles * 3 + 1] = order[1];
		triangleIndices[triangles * 3 + 2] = order[2];
		triangles++;
	}
	return triangles;
}
