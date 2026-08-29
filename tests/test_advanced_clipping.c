#include "YMGRE_CullingAndClipping.h"
#include <math.h>
#include <stdio.h>

static int finiteVertex(const gre_vertex4d_wN* v)
{
	return isfinite(v->base.pos.x) && isfinite(v->base.pos.y) && isfinite(v->base.pos.z) &&
		isfinite(v->base.u) && isfinite(v->base.v) && isfinite(v->normal.x) &&
		isfinite(v->normal.y) && isfinite(v->normal.z) && isfinite(v->tangent.x) &&
		isfinite(v->tangent.y) && isfinite(v->tangent.z);
}

int main(void)
{
	gre_camera4d camera = { 0 };
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 10.0f;
	camera.perspectPlane.kl = -1.0f;
	camera.perspectPlane.kr = 1.0f;
	camera.perspectPlane.kd = -1.0f;
	camera.perspectPlane.ku = 1.0f;
	gre_vertex4d_wN input[3] = { 0 }, output[YMGRE_FRUSTUM_CLIP_VERTEX_MAX] = { 0 };
	input[0].base = (gre_vertex4d){ { -2.0f, -0.5f, 0.5f, 1 }, 0, 0 };
	input[1].base = (gre_vertex4d){ { 2.0f, -0.5f, 3.0f, 1 }, 1, 0 };
	input[2].base = (gre_vertex4d){ { 0.0f, 2.0f, 3.0f, 1 }, 0.5f, 1 };
	for (int i = 0; i < 3; i++)
	{
		input[i].normal = (gre_fvector4d){ (float32)i, 0.5f, 1.0f, 0 };
		input[i].tangent = (gre_fvector4d){ 1.0f, (float32)i, 0.25f, 0 };
		input[i].tangentW = i == 0 ? 1.0f : -1.0f;
		input[i].color = (GRErgb24){ (uint8)(30 + i * 80), (uint8)(40 + i * 70), (uint8)(50 + i * 60) };
		input[i].vertexLighting = (GRErgb24){ 20, 40, 60 };
		input[i].vertexSpecular = (GRErgb24){ 17, 33, 65 };
	}
	uint16 count = YMGRE_Polygon_FrustumClip_wN(input, 3, output,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, &camera);
	if (count < 3 || count > YMGRE_FRUSTUM_CLIP_VERTEX_MAX)
	{
		printf("advanced clipping: FAIL (vertex count=%u)\n", count);
		return 1;
	}
	for (uint16 i = 0; i < count; i++)
	{
		const gre_vertex4d_wN* v = &output[i];
		if (!finiteVertex(v) || v->vertexLighting.R != 20 ||
			v->vertexLighting.G != 40 || v->vertexLighting.B != 60 ||
			v->vertexSpecular.R != 17 || v->vertexSpecular.G != 33 ||
			v->vertexSpecular.B != 65 ||
			v->base.pos.z < camera.frustum.Znear - 1e-5f ||
			v->base.pos.z > camera.frustum.Zfar + 1e-5f ||
			v->base.pos.x < v->base.pos.z * camera.perspectPlane.kl - 1e-5f ||
			v->base.pos.x > v->base.pos.z * camera.perspectPlane.kr + 1e-5f ||
			v->base.pos.y < v->base.pos.z * camera.perspectPlane.kd - 1e-5f ||
			v->base.pos.y > v->base.pos.z * camera.perspectPlane.ku + 1e-5f)
		{
			printf("advanced clipping: FAIL (invalid output vertex %u)\n", i);
			return 1;
		}
	}
	printf("advanced clipping: PASS (input=3, output=%u, attributes finite and inside frustum)\n", count);
	return 0;
}
