#include "YMGRE_TriangleRaster.h"
#include "YMGRE_Rasterization.h"
#include "YMGRE_Camera.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define W 64
#define CHECK(c)                                                                                   \
	do                                                                                             \
	{                                                                                              \
		if (!(c))                                                                                  \
		{                                                                                          \
			fprintf(stderr, "line %d: %s\n", __LINE__, #c);                                        \
			exit(1);                                                                               \
		}                                                                                          \
	} while (0)
static float qAt(float x, float y) { return .004f + .00015f * x + .00008f * y; }
static const GRErgb24 color = {180, 110, 65};
static float edge(gre_fvector4d a, gre_fvector4d b, float x, float y)
{
	return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
static void triangle(GRE_Camera4d camera, const float xy[6], int textured)
{
	gre_vertex4d v[3] = {0}, saved[3];
	GRE_Index indices[3] = {0, 1, 2};
	gre_polygon4d p = {0};
	p.num = 3;
	p.index = indices;
	GRErgb24 texels[16];
	for (int i = 0; i < 16; ++i)
		texels[i] = (GRErgb24){(uint8)(20 + i * 10), 40, 60};
	gre_material m = {0};
	m.valid = 1;
	m.width = 4;
	m.height = 4;
	m.pixel = texels;
	for (int i = 0; i < 3; ++i)
	{
		float x = xy[2 * i], y = xy[2 * i + 1];
		v[i].pos = (gre_fvector4d){x, y, 1 / qAt(x, y), 1};
		v[i].u = .1f + .7f * (i == 1);
		v[i].v = .1f + .7f * (i == 2);
	}
	memcpy(saved, v, sizeof v);
	YMGRE_CameraImage_Init(camera, (GRErgb24){0, 0, 0});
	YMGRE_TriangleRaster_Fill(v, &p, color, textured ? &m : NULL, camera);
	CHECK(!memcmp(saved, v, sizeof v));
	int checked = 0;
	float area = edge(v[0].pos, v[1].pos, v[2].pos.x, v[2].pos.y);
	for (int y = 0; y < W; ++y)
		for (int x = 0; x < W; ++x)
		{
			float w0 = edge(v[1].pos, v[2].pos, x, y) / area;
			float w1 = edge(v[2].pos, v[0].pos, x, y) / area;
			float w2 = 1 - w0 - w1;
			if (w0 < .02f || w1 < .02f || w2 < .02f)
				continue;
			float q = qAt(x, y), z = 1 / q;
			CHECK(fabsf(camera->img.zbuff[y * W + x] - z) < .004f);
			if (textured)
			{
				float u = (w0 * v[0].u / v[0].pos.z + w1 * v[1].u / v[1].pos.z +
						   w2 * v[2].u / v[2].pos.z) /
						  q;
				float t = (w0 * v[0].v / v[0].pos.z + w1 * v[1].v / v[1].pos.z +
						   w2 * v[2].v / v[2].pos.z) /
						  q;
				/* Avoid testing texel-boundary rounding rather than interpolation. */
				if (fabsf(u * 4 - roundf(u * 4)) > .001f && fabsf(t * 4 - roundf(t * 4)) > .001f)
				{
					GRErgb24 c = texels[(int)(t * 4) * 4 + (int)(u * 4)];
					c.R = (uint8)GREMin(c.R * color.R / DefaultPolygonClv, 255);
					c.G = (uint8)GREMin(c.G * color.G / DefaultPolygonClv, 255);
					c.B = (uint8)GREMin(c.B * color.B / DefaultPolygonClv, 255);
					CHECK(GRE_FramePixel_Equals(camera->img.data[y * W + x],
												GRE_FramePixel_From_RGB24(c)));
				}
			}
			++checked;
		}
	CHECK(checked > 100);
}
static void wall(GRE_Camera4d camera, int diagonal)
{
	gre_vertex4d v[4] = {0};
	const float xy[8] = {8, 8, 56, 8, 56, 56, 8, 56};
	GRE_Index ids[2][6] = {{0, 1, 2, 0, 2, 3}, {0, 1, 3, 1, 2, 3}};
	gre_polygon4d p[2] = {0};
	for (int i = 0; i < 4; i++)
		v[i].pos = (gre_fvector4d){xy[i * 2], xy[i * 2 + 1], 1 / qAt(xy[i * 2], xy[i * 2 + 1]), 1};
	YMGRE_CameraImage_Init(camera, (GRErgb24){0, 0, 0});
	for (int i = 0; i < 2; i++)
	{
		p[i].num = 3;
		p[i].index = &ids[diagonal][i * 3];
		YMGRE_TriangleRaster_Fill(v, &p[i], color, NULL, camera);
	}
	for (int y = 10; y < 55; y++)
		for (int x = 10; x < 55; x++)
			CHECK(fabsf(camera->img.zbuff[y * W + x] - 1 / qAt(x, y)) < .004f);
	GRErgb24 laser = {40, 255, 80};
	int at = 32 * W + 32;
	float z = 1 / qAt(32, 32);
	GRE_FramePixel surface = camera->img.data[at];
	YMGRE_Img_LineDepth(camera->img.data, camera->img.zbuff, W, W, 32, 32, z + 1, 32, 32, z + 1,
						laser, 1);
	CHECK(GRE_FramePixel_Equals(camera->img.data[at], surface));
	YMGRE_Img_LineDepth(camera->img.data, camera->img.zbuff, W, W, 32, 32, z + 1, 32, 32, z + 1,
						laser, 0);
	CHECK(GRE_FramePixel_Equals(camera->img.data[at], GRE_FramePixel_From_RGB24(laser)));
	YMGRE_Img_LineDepth(camera->img.data, camera->img.zbuff, W, W, 32, 32, z - 1, 32, 32, z - 1,
						color, 1);
	CHECK(GRE_FramePixel_Equals(camera->img.data[at], surface));
	camera->img.zbuff[at] = z;
	/* Wire diagonals lie on the same plane, regardless of triangulation. */
	gre_object4d obj = {0};
	obj.pointNum = 4;
	obj.polygonNum = 2;
	obj.polygonList = p;
	uint8 hide[2] = {0};
	YMGRE_Img_SetBrushColor(laser);
	YMGRE_TrangleObject_WiresTo(&obj, v, hide, camera);
	int count = 0;
	for (int y = 10; y < 55; y++)
		for (int x = 10; x < 55; x++)
			if (GRE_FramePixel_Equals(camera->img.data[y * W + x],
									  GRE_FramePixel_From_RGB24(laser)))
			{
				CHECK(fabsf(camera->img.zbuff[y * W + x] - 1 / qAt(x, y)) < .004f);
				++count;
			}
	CHECK(count > 30);
	for (int i = 0; i < W * W; i++)
	{
		camera->img.data[i] = surface;
		camera->img.zbuff[i] = 20;
	}
	YMGRE_TrangleObject_WiresTo(&obj, v, hide, camera);
	for (int i = 0; i < W * W; i++)
		CHECK(GRE_FramePixel_Equals(camera->img.data[i], surface));
}
static void polygonDepth(GRE_Camera4d camera, int external)
{
	gre_vertex4d v[4] = {0};
	GRE_Index indices[4] = {0, 1, 2, 3};
	const float xy[8] = {8, 8, 56, 8, 56, 56, 8, 56};
	for (int i = 0; i < 4; i++)
		v[i].pos = (gre_fvector4d){xy[2*i], xy[2*i+1], 1/qAt(xy[2*i], xy[2*i+1]), 1};
	gre_polygon4d polygon = {0};
	polygon.num = 4; polygon.index = indices; polygon.planeColor_ = color;
	gre_object4d object = {0};
	object.pointNum = 4; object.pointList_ = v;
	object.polygonNum = 1; object.polygonList = &polygon;
	uint8 hidden = 0;
	GRErgb24 fill = color, wire = {40, 255, 80};
	YMGRE_Img_SetBrushColor(wire);
	YMGRE_CameraImage_Init(camera, (GRErgb24){0, 0, 0});
	if (external)
		YMGRE_PolygonObject_Primitive_RasterizationTo(&object, v, &hidden, &fill, camera, 1);
	else
		YMGRE_PolygonObject_Primitive_Rasterization(&object, camera, 1);
	int wires = 0;
	for (int y = 8; y < 56; y++)
		for (int x = 8; x < 56; x++)
		{
			CHECK(fabsf(camera->img.zbuff[y*W+x] - 1/qAt(x+.5f, y+.5f)) < .004f);
			if (GRE_FramePixel_Equals(camera->img.data[y*W+x], GRE_FramePixel_From_RGB24(wire)))
				wires++;
		}
	CHECK(wires > 60);
	/* A fragment in front of the true plane must pass; one behind it must fail. */
	int at = 32*W+32;
	float z = 1/qAt(32.5f, 32.5f);
	YMGRE_Img_LineDepth(camera->img.data, camera->img.zbuff, W, W, 32, 32, z+1, 32, 32, z+1, wire, 1);
	CHECK(GRE_FramePixel_Equals(camera->img.data[at], GRE_FramePixel_From_RGB24(color)));
	YMGRE_Img_LineDepth(camera->img.data, camera->img.zbuff, W, W, 32, 32, z-1, 32, 32, z-1, wire, 1);
	CHECK(GRE_FramePixel_Equals(camera->img.data[at], GRE_FramePixel_From_RGB24(wire)));
}
static void advancedDepth(GRE_Camera4d camera, int mode)
{
	gre_vertex4d_wN vertices[3] = {0};
	const float xy[6] = {8, 6, 55, 12, 20, 56};
	GRErgb24 white = {255, 255, 255};
	for (int i = 0; i < 3; i++)
	{
		float x = xy[2 * i], y = xy[2 * i + 1];
		vertices[i].base.pos = (gre_fvector4d){x, y, 1 / qAt(x, y), 1};
		vertices[i].normal = (gre_fvector4d){0, 0, -1, 0};
		vertices[i].tangent = (gre_fvector4d){1, 0, 0, 0};
		vertices[i].tangentW = 1;
		vertices[i].color = vertices[i].vertexLighting = white;
	}
	GRE_Index indices[3] = {0, 1, 2};
	gre_polygon4d polygon = {0};
	polygon.num = 3; polygon.index = indices;
	gre_material material = {0};
	material.valid = 1; material.width = material.height = 1;
	material.pixel = &white; material.diffuse = material.ambient = white;
	gre_lightmap lightmap = {0};
	lightmap.width = lightmap.height = 1; lightmap.pixels = &white;
	lightmap.colorsBaked = 1;
	gre_list lights = {0};
	gre_fmat4x4 matrix = {0};
	for (int i = 0; i < 4; i++) matrix.val[i][i] = 1;
	YMGRE_CameraImage_Init(camera, (GRErgb24){0, 0, 0});
	switch (mode)
	{
	case 0: YMGRE_TriangleRaster_FillVertexColor_wN(vertices, &polygon, camera); break;
	case 1: YMGRE_TriangleRaster_FillVertexLit_wN(vertices, &polygon, &material, camera); break;
	case 2: YMGRE_TriangleRaster_Fill_wN(vertices, &polygon, &material, &lights,
		NULL, &matrix, 0, camera); break;
	case 3: YMGRE_TriangleRaster_FillLightmap_wN(vertices, &polygon, &material, &lightmap, camera); break;
	}
	float area = edge(vertices[0].base.pos, vertices[1].base.pos, xy[4], xy[5]);
	int checked = 0;
	for (int y = 0; y < W; y++)
		for (int x = 0; x < W; x++)
		{
			float w0 = edge(vertices[1].base.pos, vertices[2].base.pos, x, y) / area;
			float w1 = edge(vertices[2].base.pos, vertices[0].base.pos, x, y) / area;
			if (w0 < .02f || w1 < .02f || 1 - w0 - w1 < .02f) continue;
			CHECK(fabsf(camera->img.zbuff[y * W + x] - 1 / qAt(x, y)) < .004f);
			checked++;
		}
	CHECK(checked > 100);
}
int main(void)
{
	static GRE_FramePixel pixels[W * W];
	static float depth[W * W];
	gre_camera4d camera = {0};
	camera.img.width = W;
	camera.img.height = W;
	camera.img.data = pixels;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1;
	camera.frustum.Zfar = 500;
	camera.perspectPlane.Dis = 100;
	camera.perspectPlane.pL = camera.perspectPlane.pD = -100;
	camera.perspectPlane.pR = camera.perspectPlane.pU = 100;
	const float triangles[][6] = {{8, 6, 55, 12, 20, 56},
								  {8, 6, 55, 56, 20, 56},
								  {8, 6, 55, 6, 20, 56},
								  {-12, -6, 55, 12, 20, 70}};
	for (int t = 0; t < 2; ++t)
		for (int i = 0; i < 4; ++i)
			triangle(&camera, triangles[i], t);
	wall(&camera, 0);
	wall(&camera, 1);
	polygonDepth(&camera, 0);
	polygonDepth(&camera, 1);
	for (int mode = 0; mode < 4; mode++) advancedDepth(&camera, mode);
	puts("Perspective depth PASS: split/flat/clipped triangles, texture, wall diagonals, laser "
		 "occlusion, polygon/wire depth and advanced shading modes");
	return 0;
}
