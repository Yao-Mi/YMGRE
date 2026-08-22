#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>
#include <string.h>

#define TEST_W 64
#define TEST_H 64

static int fails;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static void noFree(void* data)
{
	(void)data;
}

static uint32 frameHash(GRE_FrameBuffer frame, uint32 count)
{
	uint32 hash = 2166136261u;
	const uint8* data = (const uint8*)frame;
	for (uint32 i = 0; i < count * sizeof(GRE_FramePixel); i++)
	{
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash;
}

static int depthWasWritten(const float32* depth, uint32 count, float32 clearValue)
{
	for (uint32 i = 0; i < count; i++)
	{
		if (depth[i] != clearValue)
			return 1;
	}
	return 0;
}

static void matrixIdentity(GRE_FMat4x4 matrix)
{
	GRE_memset(matrix, 0, sizeof(gre_fmat4x4));
	for (int i = 0; i < 4; i++)
		matrix->val[i][i] = 1.0f;
}

static GRE_Object4d makeTriangle(void)
{
	GRE_Object4d object = YMGRE_Creat_Object(3, 1, "triangle", "");
	object->pointList[0].pos = (gre_fvector4d){ -20.0f, -20.0f, 100.0f, 1.0f };
	object->pointList[1].pos = (gre_fvector4d){ 0.0f, 20.0f, 100.0f, 1.0f };
	object->pointList[2].pos = (gre_fvector4d){ 20.0f, -20.0f, 100.0f, 1.0f };
	object->polygonList[0].num = 3;
	object->polygonList[0].index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
	object->polygonList[0].index[0] = 0;
	object->polygonList[0].index[1] = 1;
	object->polygonList[0].index[2] = 2;
	object->polygonList[0].pN = (gre_fvector4d){ 0.0f, 0.0f, -1600.0f, 0.0f };
	object->polygonList[0].planeColor = (GRErgb24){ 180, 120, 80 };
	object->BoundingSphereR = 30.0f;
	object->boundType = GRE_Bounding_Sphere_R;
	object->WorldCoordinate.z = 100.0f;
	object->WorldCoordinate.w = 1.0f;
	return object;
}

static GRE_Camera4d makeCamera(int16 id)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(id, TEST_W, TEST_H, 45.0f, 45.0f, 45.0f, 45.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 500.0f);
	matrixIdentity(&camera->move.TMat);
	return camera;
}

int main(void)
{
	GRE_Object4d object = makeTriangle();
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f);
	GRE_Camera4d camera0 = makeCamera(0);
	GRE_Camera4d camera1 = makeCamera(1);
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	YMGRE_List_Append(&objects, sizeof(gre_object4d), object);
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);

	object->isDelete = 77;
	object->polygonList[0].ishide = 55;
	object->polygonList[0].planeColor_ = (GRErgb24){ 1, 2, 3 };
	object->pointList_[0].pos = (gre_fvector4d){ 7, 8, 9, 10 };
	light->proper.pos_ = (gre_fvector4d){ 11, 12, 13, 14 };

	GRE_RenderWorkspace shared = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera0, &lights, &objects, &materials, shared);
	uint32 hash0 = frameHash(camera0->img.data, TEST_W * TEST_H);
	CHECK(hash0 != 0, "first camera renders a frame");
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, shared);
	CHECK(frameHash(camera0->img.data, TEST_W * TEST_H) == hash0, "second camera does not overwrite first target");
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == hash0, "shared workspace produces the same view");

	GRE_RenderWorkspace independent = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, independent);
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == hash0, "independent workspace matches shared workspace");

	YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(camera0, &lights, &objects, &materials, shared);
	uint32 polygonHash = frameHash(camera0->img.data, TEST_W * TEST_H);
	CHECK(depthWasWritten(camera0->img.zbuff, TEST_W * TEST_H, camera0->frustum.Zfar),
		"polygon pipeline writes the depth buffer");
	YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, independent);
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == polygonHash,
		"polygon pipeline matches between shared and independent workspaces");
	CHECK(object->isDelete == 77, "object culling state remains unchanged");
	CHECK(object->polygonList[0].ishide == 55, "polygon visibility remains unchanged");
	CHECK(object->polygonList[0].planeColor_.R == 1 && object->polygonList[0].planeColor_.G == 2 &&
		object->polygonList[0].planeColor_.B == 3, "polygon lighting state remains unchanged");
	CHECK(object->pointList_[0].pos.x == 7 && object->pointList_[0].pos.y == 8, "object transformed vertices remain unchanged");
	CHECK(light->proper.pos_.x == 11 && light->proper.pos_.y == 12, "light camera position remains unchanged");

	gre_vertex4d points[3];
	uint8 hidden[1];
	GRErgb24 colors[1];
	gre_fvector4d lightPos[1];
	gre_render_workspace external;
	YMGRE_RenderWorkspace_Init(&external, points, 3, hidden, colors, 1, lightPos, 1);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, &external);
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == hash0, "external static workspace matches dynamic workspace");
	static GRE_FramePixel externalColor[TEST_W * TEST_H];
	static float32 externalDepth[TEST_W * TEST_H];
	gre_render_target externalTarget;
	YMGRE_RenderTarget_Init(&externalTarget, TEST_W, TEST_H, externalColor, externalDepth);
	GRE_Camera4d camera2 = YMGRE_Creat_CameraFromTarget(2, &externalTarget, 45.0f, 45.0f, 45.0f, 45.0f);
	CHECK(camera2->img.data == NULL && camera2->img.zbuff == NULL,
		"external target camera does not allocate a private framebuffer");
	CHECK(YMGRE_Camera_GetRenderTarget(camera2) == &externalTarget,
		"external target camera reports its bound target");
	YMGRE_Camera_Frustum_Init(camera2, 1.0f, 500.0f);
	matrixIdentity(&camera2->move.TMat);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera2, &lights, &objects, &materials, shared);
	CHECK(frameHash(externalColor, TEST_W * TEST_H) == hash0, "camera renders directly into an external target");

	YMGRE_Free_RenderWorkspace(independent);
	YMGRE_Free_RenderWorkspace(shared);
	YMGRE_List_Clear(&objects, noFree);
	YMGRE_List_Clear(&lights, noFree);
	YMGRE_Free_Camera(camera0);
	YMGRE_Free_Camera(camera1);
	YMGRE_Free_Camera(camera2);
	YMGRE_Free_Light(light);
	YMGRE_Free_Object(object);

	if (fails == 0)
		printf("test_render_context: ALL PASS\n");
	else
		printf("test_render_context: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
