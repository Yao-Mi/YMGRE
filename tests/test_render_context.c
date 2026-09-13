#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_RenderContext.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_Light.h"
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

static int frameIsColor(GRE_FrameBuffer frame, uint32 count, GRErgb24 color)
{
	GRE_FramePixel expected = GRE_FramePixel_From_RGB24(color);
	for (uint32 i = 0; i < count; i++)
		if (!GRE_FramePixel_Equals(frame[i], expected))
			return 0;
	return 1;
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
	object->polygonList[0].index = GRE_PolyIndex_Malloc(3 * sizeof(GRE_Index));
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

static void testLightColors(void)
{
	gre_polygon4d polygon = { 0 };
	polygon.planeColor = (GRErgb24){ 200, 200, 200 };
	gre_fvector4d point = { 0, 0, 0, 1 };
	gre_fvector4d normal = { 0, 0, 1, 0 };
	GRErgb24 color = { 0 };
	GRE_Light4d ambient = YMGRE_Creat_Light(10, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 0.2f);
	GRE_Light4d pointLight = YMGRE_Creat_Light(11, GRE_PointLight,
		(GRErgb24){ 255, 0, 0 }, 1.0f);
	pointLight->proper.pos_ = (gre_fvector4d){ 0, 0, 10, 1 };
	pointLight->proper.shadowK = 0;

	YMGRE_PolygonLighting_Color(&polygon, &point, &normal, ambient, &color, 0, 1);
	CHECK(color.R >= 39 && color.R <= 40 && color.G == color.R && color.B == color.R,
		"global light strength scales ambient contribution");
	YMGRE_PolygonLighting_Color(&polygon, &point, &normal, pointLight, &color, 0, 1);
	CHECK(color.R > color.G && color.G == color.B,
		"red point light changes the lit surface color");

	color = (GRErgb24){ 0 };
	pointLight->proper.lightcolor = (GRErgb24){ 0, 0, 255 };
	YMGRE_PolygonLighting_Color(&polygon, &point, &normal, pointLight, &color, 0, 1);
	CHECK(color.B > color.R && color.R == color.G,
		"blue point light changes the lit surface color");

	color = (GRErgb24){ 0 };
	pointLight->type = GRE_SpotLight;
	pointLight->proper.lightcolor = (GRErgb24){ 0, 255, 0 };
	pointLight->proper.spot.direct = (gre_fvector4d){ 0, 0, -1, 0 };
	YMGRE_PolygonLighting_Color(&polygon, &point, &normal, pointLight, &color, 0, 1);
	CHECK(color.G > color.R && color.R == color.B,
		"spot light color changes the lit surface inside its cone");

	color = (GRErgb24){ 0 };
	pointLight->proper.spot.direct = (gre_fvector4d){ 1, 0, 0, 0 };
	YMGRE_PolygonLighting_Color(&polygon, &point, &normal, pointLight, &color, 0, 1);
	CHECK(color.R == 0 && color.G == 0 && color.B == 0,
		"spot light contributes nothing outside its outer cone");

	color = (GRErgb24){ 0 };
	pointLight->type = GRE_PointLight;
	pointLight->proper.lightcolor = (GRErgb24){ 255, 255, 255 };
	pointLight->proper.pos_ = (gre_fvector4d){ 0, 0, 0, 1 };
	polygon.planeColor = (GRErgb24){ 0, 0, 0 };
	gre_fvector4d specularPoint = { 0, 0, 10, 1 };
	gre_fvector4d specularNormal = { 0, 0, -1, 0 };
	YMGRE_PolygonLighting_ColorAdvanced(&polygon, &specularPoint, &specularNormal, pointLight,
		&color, 1.0f, 1, (GRErgb24){ 0, 0, 255 });
	CHECK(color.B > 200 && color.R == 0 && color.G == 0,
		"advanced point-light specular uses the material specular color");

	GRErgb24 broad = { 0, 0, 0 }, tight = { 0, 0, 0 };
	pointLight->proper.pos_ = (gre_fvector4d){ 6, 0, 0, 1 };
	YMGRE_PolygonLighting_ColorAdvanced(&polygon, &specularPoint, &specularNormal,
		pointLight, &broad, 1.0f, 8, (GRErgb24){ 255, 255, 255 });
	YMGRE_PolygonLighting_ColorAdvanced(&polygon, &specularPoint, &specularNormal,
		pointLight, &tight, 1.0f, 96, (GRErgb24){ 255, 255, 255 });
	CHECK(broad.R > tight.R + 100 && broad.G > tight.G + 100 && broad.B > tight.B + 100,
		"higher specular power narrows the off-axis highlight");

	polygon.planeColor = (GRErgb24){ 180, 110, 45 };
	pointLight->proper.pos_ = (gre_fvector4d){ 0, 0, -10, 1 };
	pointLight->proper.kc0 = 1.0f;
	pointLight->proper.kc1 = pointLight->proper.kc2 = 0.0f;
	pointLight->proper.shadowK = 0.5f;
	GRErgb24 backBase = { 0, 0, 0 }, backSpecular = { 0, 0, 0 };
	YMGRE_PolygonLighting_ComponentsAdvanced(&polygon, &point, &normal, pointLight,
		&backBase, &backSpecular, 1.0f, 30, (GRErgb24){ 0, 0, 255 });
	CHECK(backBase.R == 90 && backBase.G == 55 && backBase.B == 22,
		"point-light shadowK preserves the material diffuse color");
	CHECK(backSpecular.R == 0 && backSpecular.G == 0 && backSpecular.B == 0,
		"back-facing point light produces no specular contribution");

	GRErgb24 spotBase = { 0, 0, 0 }, spotSpecular = { 0, 0, 0 };
	pointLight->type = GRE_SpotLight;
	pointLight->proper.pos_ = (gre_fvector4d){ 0, 0, 0, 1 };
	pointLight->proper.spot.direct = (gre_fvector4d){ 0, 0, 1, 0 };
	YMGRE_PolygonLighting_ComponentsAdvanced(&polygon, &specularPoint,
		&specularNormal, pointLight, &spotBase, &spotSpecular, 1.0f, 30,
		(GRErgb24){ 40, 90, 255 });
	CHECK(spotSpecular.B > spotSpecular.R && spotSpecular.B > 150,
		"spot light produces material-colored specular inside its cone");
	spotBase = spotSpecular = (GRErgb24){ 0, 0, 0 };
	pointLight->proper.spot.direct = (gre_fvector4d){ 1, 0, 0, 0 };
	YMGRE_PolygonLighting_ComponentsAdvanced(&polygon, &specularPoint,
		&specularNormal, pointLight, &spotBase, &spotSpecular, 1.0f, 30,
		(GRErgb24){ 40, 90, 255 });
	CHECK(spotSpecular.R == 0 && spotSpecular.G == 0 && spotSpecular.B == 0,
		"spot light produces no specular outside its outer cone");

	YMGRE_Free_Light(pointLight);
	YMGRE_Free_Light(ambient);
}

int main(void)
{
	testLightColors();
	GRE_Object4d object = makeTriangle();
	CHECK(offsetof(gre_vertex4d_wN, base) == 0, "extended vertex keeps legacy vertex at offset zero");
	object->renderMode = GRE_RenderMode_Pixel;
	CHECK(YMGRE_Object_GenerateVertexAttributes(object), "triangle generates advanced vertex attributes");
	CHECK(object->renderMode == GRE_RenderMode_Pixel,
		"generating vertex attributes preserves the selected render mode");
	CHECK(object->pointList_wN != NULL && object->pointList_wN[0].normal.z < -0.9f,
		"generated vertex normal follows triangle winding");
	CHECK(YMGRE_Fvector4d_Len1(&object->pointList_wN[0].tangent) > 0.9f,
		"generated tangent has a stable fallback for degenerate UVs");
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
	YMGRE_Camera_TanglePipline_wN(camera1, &lights, &objects, &materials, independent);
	CHECK(depthWasWritten(camera1->img.zbuff, TEST_W * TEST_H, camera1->frustum.Zfar),
		"advanced vertex pipeline renders with a dynamic workspace");
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, independent);
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == hash0, "independent workspace matches shared workspace");
	object->isVisible = 0;
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, independent);
	CHECK(frameIsColor(camera1->img.data, TEST_W * TEST_H, (GRErgb24){ 50, 50, 50 }),
		"hidden submesh is skipped by triangle workspace pipeline");
	YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, independent);
	CHECK(frameIsColor(camera1->img.data, TEST_W * TEST_H, (GRErgb24){ 50, 50, 50 }),
		"hidden submesh is skipped by polygon workspace pipeline");
	object->isVisible = 1;

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
	gre_vertex4d_wN advancedPoints[3];
	YMGRE_RenderWorkspace_BindVertexAttributes(&external, advancedPoints, 3);
	YMGRE_Camera_TanglePipline_wN(camera1, &lights, &objects, &materials, &external);
	CHECK(depthWasWritten(camera1->img.zbuff, TEST_W * TEST_H, camera1->frustum.Zfar),
		"advanced vertex pipeline accepts caller-owned fixed memory");
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera1, &lights, &objects, &materials, &external);
	CHECK(frameHash(camera1->img.data, TEST_W * TEST_H) == hash0, "external static workspace matches dynamic workspace");
	static GRE_FramePixel externalColor[TEST_W * TEST_H];
	static float32 externalDepth[TEST_W * TEST_H];
	gre_render_target externalTarget;
	YMGRE_RenderTarget_Init(&externalTarget, TEST_W, TEST_H, externalColor, externalDepth);
	GRE_Camera4d camera2 = YMGRE_Creat_CameraFromTarget(2, &externalTarget, 45.0f, 45.0f, 45.0f, 45.0f);
	CHECK(camera2->img.data == externalColor && camera2->img.zbuff == externalDepth,
		"external target camera exposes its target framebuffer");
	CHECK(camera2->ownsImageBuffers == 0,
		"external target camera does not own caller buffers");
	CHECK(YMGRE_Camera_GetRenderTarget(camera2) == &externalTarget,
		"external target camera reports its bound target");
	YMGRE_Camera_Frustum_Init(camera2, 1.0f, 500.0f);
	matrixIdentity(&camera2->move.TMat);
	YMGRE_CameraImage_Init(camera2, (GRErgb24){ 12, 34, 56 });
	uint32 clearHash = frameHash(externalColor, TEST_W * TEST_H);
	CHECK(GRE_FramePixel_Equals(externalColor[0], GRE_FramePixel_From_RGB24((GRErgb24){ 12, 34, 56 })) &&
		externalDepth[0] == camera2->frustum.Zfar,
		"camera clear writes its currently bound external target");
	gre_line3d line = {
		(gre_fvector4d){ -20, 0, 100, 1 },
		(gre_fvector4d){ 20, 0, 100, 1 },
		(GRErgb24){ 240, 40, 40 }
	};
	YMGRE_Camera_LineList_Rendering(camera2, &line, 1, 1);
	CHECK(frameHash(externalColor, TEST_W * TEST_H) != clearHash,
		"independent 3D line renders into the active target");
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera2, &lights, &objects, &materials, shared);
	CHECK(frameHash(externalColor, TEST_W * TEST_H) == hash0, "camera renders directly into an external target");
	CHECK(camera0->ownsImageBuffers == 1 && camera1->ownsImageBuffers == 1,
		"regular cameras own the buffers they create");
	static GRE_FramePixel reboundColor[TEST_W * TEST_H];
	static float32 reboundDepth[TEST_W * TEST_H];
	gre_render_target reboundTarget;
	YMGRE_RenderTarget_Init(&reboundTarget, TEST_W, TEST_H, reboundColor, reboundDepth);
	YMGRE_Camera_BindRenderTarget(camera0, &reboundTarget);
	CHECK(camera0->ownsImageBuffers == 1,
		"rebinding a regular camera preserves ownership of its original buffers");
	YMGRE_CameraImage_Init(camera0, (GRErgb24){ 21, 43, 65 });
	CHECK(GRE_FramePixel_Equals(reboundColor[0], GRE_FramePixel_From_RGB24((GRErgb24){ 21, 43, 65 })) &&
		reboundDepth[0] == camera0->frustum.Zfar,
		"rebound camera clears the active target without transferring ownership");

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
