#include "YMGRE_CullingAndClipping.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <stdio.h>

static int vertexInside(GRE_Vertex4d vertex, GRE_Camera4d camera)
{
	return (vertex->pos.z >= camera->frustum.Znear - 0.001f) &&
		(vertex->pos.z <= camera->frustum.Zfar + 0.001f) &&
		(vertex->pos.x >= camera->perspectPlane.kl * vertex->pos.z - 0.001f) &&
		(vertex->pos.x <= camera->perspectPlane.kr * vertex->pos.z + 0.001f) &&
		(vertex->pos.y >= camera->perspectPlane.kd * vertex->pos.z - 0.001f) &&
		(vertex->pos.y <= camera->perspectPlane.ku * vertex->pos.z + 0.001f);
}

static int clippingCasePass(GRE_Camera4d camera, gre_vertex4d input[3])
{
	gre_vertex4d output[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	uint16 outputNum = YMGRE_Polygon_FrustumClip(input, 3, output,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera);
	if (outputNum < 3)
		return 0;
	for (uint16 i = 0; i < outputNum; i++)
	{
		if (!vertexInside(&output[i], camera))
			return 0;
	}
	return 1;
}

int main(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 64, 64, 45.0f, 45.0f, 45.0f, 45.0f);
	YMGRE_Camera_Frustum_Init(camera, 40.0f, 150.0f);
	gre_vertex4d cases[6][3] = {
		{ { { -30, -30, 20, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 0, 30, 100, 1 }, 0.5f, 1 } },
		{ { { -30, -30, 180, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 0, 30, 100, 1 }, 0.5f, 1 } },
		{ { { -140, 0, 100, 1 }, 0, 0 }, { { 30, -30, 100, 1 }, 1, 0 }, { { 30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 140, 0, 100, 1 }, 0, 0 }, { { -30, -30, 100, 1 }, 1, 0 }, { { -30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 0, -140, 100, 1 }, 0, 0 }, { { -30, 30, 100, 1 }, 1, 0 }, { { 30, 30, 100, 1 }, 0.5f, 1 } },
		{ { { 0, 140, 100, 1 }, 0, 0 }, { { -30, -30, 100, 1 }, 1, 0 }, { { 30, -30, 100, 1 }, 0.5f, 1 } }
	};
	for (uint16 i = 0; i < 6; i++)
	{
		if (!clippingCasePass(camera, cases[i]))
		{
			printf("test_frustum_clipping: plane=%u FAILED\n", i);
			return 1;
		}
	}

	gre_vertex4d output[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	gre_vertex4d behind[3] = {
		{ { -10.0f, -10.0f, -20.0f, 1.0f }, 0.0f, 0.0f },
		{ { 10.0f, -10.0f, -20.0f, 1.0f }, 1.0f, 0.0f },
		{ { 0.0f, 10.0f, -20.0f, 1.0f }, 0.5f, 1.0f }
	};
	if (YMGRE_Polygon_FrustumClip(behind, 3, output,
		YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera) != 0)
	{
		printf("test_frustum_clipping: behind-camera polygon survived FAILED\n");
		return 1;
	}

	printf("test_frustum_clipping: all six planes PASS\n");
	YMGRE_Free_Camera(camera);
	return 0;
}
