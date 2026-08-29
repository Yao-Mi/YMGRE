#include "YMGRE_TriangleRaster.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_W 32
#define TEST_H 32

static void noFree(void* data)
{
	(void)data;
}

static void identity(GRE_FMat4x4 matrix)
{
	memset(matrix, 0, sizeof(*matrix));
	for (int i = 0; i < 4; i++) matrix->val[i][i] = 1.0f;
}

static void renderHandedness(GRE_FrameBuffer frame, float32 w0, float32 w1, float32 w2)
{
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img = (gre_render_target){ TEST_W, TEST_H, frame, depth };
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	camera.perspectPlane.Dis = 1.0f;
	camera.perspectPlane.pL = camera.perspectPlane.pD = -1.0f;
	camera.perspectPlane.pR = camera.perspectPlane.pU = 1.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });

	gre_vertex4d_wN vertices[3] = { 0 };
	vertices[0].base = (gre_vertex4d){ { 4, 4, 100, 1 }, 0, 0 };
	vertices[1].base = (gre_vertex4d){ { 4, 28, 100, 1 }, 0, 1 };
	vertices[2].base = (gre_vertex4d){ { 28, 28, 100, 1 }, 1, 1 };
	for (int i = 0; i < 3; i++)
	{
		vertices[i].normal = (gre_fvector4d){ 0, 0, 1, 0 };
		vertices[i].tangent = (gre_fvector4d){ 1, 0, 0, 0 };
		vertices[i].color = (GRErgb24){ 255, 255, 255 };
	}
	vertices[0].tangentW = w0;
	vertices[1].tangentW = w1;
	vertices[2].tangentW = w2;
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3;
	polygon.index = indices;
	polygon.planeColor = (GRErgb24){ 255, 255, 255 };

	GRE_Material material = YMGRE_Creat_Material("normal_test");
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
	material->advanced->normalWidth = material->advanced->normalHeight = 1;
	material->advanced->normalPixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->advanced->normalPixel[0] = (GRErgb24){ 128, 230, 200 };

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_PointLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	light->proper.shadowK = 0;
	gre_list lights = { 0 };
	YMGRE_List_Append(&lights, sizeof(gre_light4d), light);
	gre_fvector4d lightPos[1] = { { 0, 80, 180, 1 } };
	gre_fmat4x4 worldToCamera;
	identity(&worldToCamera);
	YMGRE_TriangleRaster_Fill_wN(vertices, &polygon, material, &lights,
		lightPos, &worldToCamera, 0, &camera);
	YMGRE_List_Clear(&lights, noFree);
	YMGRE_Free_Light(light);
	YMGRE_Free_Material(material);
}

static void renderVertexIdentity(GRE_FrameBuffer frame, uint8 withWhiteMaterial)
{
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img = (gre_render_target){ TEST_W, TEST_H, frame, depth };
	camera.frustum.Znear = 1.0f; camera.frustum.Zfar = 500.0f;
	camera.perspectPlane.Dis = 1.0f;
	camera.perspectPlane.pL = camera.perspectPlane.pD = -1.0f;
	camera.perspectPlane.pR = camera.perspectPlane.pU = 1.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });
	gre_vertex4d_wN vertices[3] = { 0 };
	vertices[0].base = (gre_vertex4d){ { 4, 4, 100, 1 }, 0, 0 };
	vertices[1].base = (gre_vertex4d){ { 4, 28, 100, 1 }, 0, 1 };
	vertices[2].base = (gre_vertex4d){ { 28, 28, 100, 1 }, 1, 1 };
	vertices[0].color = (GRErgb24){ 255, 35, 35 };
	vertices[1].color = (GRErgb24){ 35, 255, 70 };
	vertices[2].color = (GRErgb24){ 40, 90, 255 };
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3; polygon.index = indices;
	GRE_Material material = NULL;
	GRE_Light4d light = NULL;
	gre_list lights = { 0 };
	gre_fvector4d lightPos[1] = { 0 };
	gre_fmat4x4 matrix;
	identity(&matrix);
	if (withWhiteMaterial)
	{
		material = YMGRE_Creat_Material("white");
		material->width = material->height = 1;
		material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
		material->pixel[0] = (GRErgb24){ 255, 255, 255 };
		light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){ 255, 255, 255 }, 1.0f);
		YMGRE_List_Append(&lights, sizeof(gre_light4d), light);
	}
	if(withWhiteMaterial)
		YMGRE_TriangleRaster_Fill_wN(vertices,&polygon,material,&lights,
			lightPos,&matrix,0,&camera);
	else
		YMGRE_TriangleRaster_FillVertexColor_wN(vertices,&polygon,&camera);
	if (withWhiteMaterial)
	{
		YMGRE_List_Clear(&lights, noFree);
		YMGRE_Free_Light(light);
		YMGRE_Free_Material(material);
	}
}

static int textureBoundaryIsStable(void)
{
	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img = (gre_render_target){ TEST_W, TEST_H, frame, depth };
	camera.frustum.Znear = 1.0f; camera.frustum.Zfar = 500.0f;
	camera.perspectPlane.Dis = 1.0f;
	camera.perspectPlane.pL = camera.perspectPlane.pD = -1.0f;
	camera.perspectPlane.pR = camera.perspectPlane.pU = 1.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });

	gre_vertex4d_wN vertices[3] = { 0 };
	vertices[0].base = (gre_vertex4d){ { 4, 28, 100, 1 }, 0, 1 };
	vertices[1].base = (gre_vertex4d){ { 16, 4, 100, 1 }, 0.5f, 0 };
	vertices[2].base = (gre_vertex4d){ { 28, 28, 100, 1 }, 1, 1 };
	for (int i = 0; i < 3; i++)
	{
		vertices[i].normal = (gre_fvector4d){ 0, 0, 1, 0 };
		vertices[i].color = (GRErgb24){ 255, 255, 255 };
	}
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3; polygon.index = indices;

	GRE_Material material = YMGRE_Creat_Material("boundary");
	material->width = material->height = 16;
	material->pixel = GRE_ImageBuff_Malloc(16 * 16 * sizeof(GRErgb24));
	GRErgb24 dark = { 35, 45, 55 }, lightColor = { 215, 225, 235 };
	for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
		material->pixel[y * 16 + x] = (x % 4 == 0 || y % 4 == 0) ? dark : lightColor;
	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 },1.0f);
	gre_list lights = { 0 };
	YMGRE_List_Append(&lights,sizeof(gre_light4d),light);
	gre_fvector4d lightPos[1] = { 0 };
	gre_fmat4x4 matrix;
	identity(&matrix);
	YMGRE_TriangleRaster_Fill_wN(vertices,&polygon,material,&lights,
		lightPos,&matrix,0,&camera);

	int stable = 1;
	for (int y = 5; y <= 27; y++)
	{
		GRErgb24 got = frame[y * TEST_W + 16];
		if (abs((int)got.R - dark.R) > 1 || abs((int)got.G - dark.G) > 1 ||
			abs((int)got.B - dark.B) > 1)
			stable = 0;
	}
	YMGRE_List_Clear(&lights,noFree);
	YMGRE_Free_Light(light);
	YMGRE_Free_Material(material);
	return stable;
}

static int vertexLitRasterUsesCachedLighting(void)
{
	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img = (gre_render_target){ TEST_W, TEST_H, frame, depth };
	camera.frustum.Znear = 1.0f; camera.frustum.Zfar = 500.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });
	gre_vertex4d_wN vertices[3] = { 0 };
	vertices[0].base = (gre_vertex4d){ { 4, 28, 100, 1 }, 0, 1 };
	vertices[1].base = (gre_vertex4d){ { 16, 4, 100, 1 }, .5f, 0 };
	vertices[2].base = (gre_vertex4d){ { 28, 28, 100, 1 }, 1, 1 };
	for (int i = 0; i < 3; i++)
	{
		vertices[i].color = (GRErgb24){ 255, 255, 255 };
		vertices[i].vertexLighting = (GRErgb24){ 210, 70, 35 };
	}
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3; polygon.index = indices;
	GRE_Material material = YMGRE_Creat_Material("vertex_lit");
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 255, 255 };
	YMGRE_TriangleRaster_FillVertexLit_wN(vertices, &polygon, material, &camera);
	GRE_FramePixel expected = GRE_FramePixel_From_RGB24((GRErgb24){ 210, 70, 35 });
	int passed = GRE_FramePixel_Equals(frame[16 * TEST_W + 16], expected);
	YMGRE_Free_Material(material);
	return passed;
}

static int vertexSpecularBypassesBlackAlbedo(void)
{
	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img = (gre_render_target){ TEST_W, TEST_H, frame, depth };
	camera.frustum.Znear = 1.0f; camera.frustum.Zfar = 500.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });
	gre_vertex4d_wN vertices[3] = { 0 };
	vertices[0].base = (gre_vertex4d){ { 4, 28, 100, 1 }, 0, 1 };
	vertices[1].base = (gre_vertex4d){ { 16, 4, 100, 1 }, .5f, 0 };
	vertices[2].base = (gre_vertex4d){ { 28, 28, 100, 1 }, 1, 1 };
	for (int i = 0; i < 3; i++)
	{
		vertices[i].color = (GRErgb24){ 255, 255, 255 };
		vertices[i].vertexLighting = (GRErgb24){ 200, 200, 200 };
		vertices[i].vertexSpecular = (GRErgb24){ 25, 80, 220 };
	}
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3; polygon.index = indices;
	GRE_Material material = YMGRE_Creat_Material("black_albedo");
	material->width = material->height = 1;
	material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 0, 0, 0 };
	YMGRE_TriangleRaster_FillVertexLit_wN(vertices, &polygon, material, &camera);
	GRE_FramePixel expected = GRE_FramePixel_From_RGB24((GRErgb24){ 25, 80, 220 });
	int passed = GRE_FramePixel_Equals(frame[16 * TEST_W + 16], expected);
	YMGRE_Free_Material(material);
	return passed;
}

int main(void)
{
	GRE_FramePixel pure[TEST_W * TEST_H];
	GRE_FramePixel whiteIdentity[TEST_W * TEST_H];
	renderVertexIdentity(pure, 0);
	renderVertexIdentity(whiteIdentity, 1);
	if (memcmp(pure, whiteIdentity, sizeof(pure)) != 0)
	{
		printf("test_advanced_rasterization: white material/light is not identity FAILED\n");
		return 1;
	}
	GRE_FramePixel uniform[TEST_W * TEST_H];
	GRE_FramePixel mixed[TEST_W * TEST_H];
	renderHandedness(uniform, 1, 1, 1);
	renderHandedness(mixed, 1, -1, -1);
	if (memcmp(uniform, mixed, sizeof(uniform)) != 0)
	{
		printf("test_advanced_rasterization: tangent handedness changed pixels FAILED\n");
		return 1;
	}
	if (!textureBoundaryIsStable())
	{
		printf("test_advanced_rasterization: texture boundary changed columns FAILED\n");
		return 1;
	}
	if (!vertexLitRasterUsesCachedLighting())
	{
		printf("test_advanced_rasterization: vertex lighting cache was not rasterized FAILED\n");
		return 1;
	}
	if (!vertexSpecularBypassesBlackAlbedo())
	{
		printf("test_advanced_rasterization: black albedo removed cached specular FAILED\n");
		return 1;
	}
	printf("test_advanced_rasterization: PASS\n");
	return 0;
}
