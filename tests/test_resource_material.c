#include "YMCS_File_IO.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_Material.h"
#include "YMGRE_Rasterization.h"
#include <stdio.h>
#include <string.h>

#define TEST_W 32
#define TEST_H 32

static int fails;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static int colorEquals(GRErgb24 left, GRErgb24 right)
{
	return left.R == right.R && left.G == right.G && left.B == right.B;
}

static void testMaterialBasics(void)
{
	gre_list materials = { 0 };
	GRE_Material material = YMGRE_Creat_Material("checker");
	CHECK(material != NULL, "material is created");
	CHECK(material->valid == 1, "material is enabled by default");
	CHECK(colorEquals(material->ambient, (GRErgb24){ 255, 255, 255 }),
		"default ambient color is white");
	CHECK(colorEquals(material->diffuse, (GRErgb24){ 255, 255, 255 }),
		"default diffuse color is white");
	CHECK(colorEquals(material->specular, (GRErgb24){ 255, 255, 255 }),
		"default specular color is white");

	material->width = 2;
	material->height = 2;
	material->pixel = GRE_ImageBuff_Malloc(4 * sizeof(GRErgb24));
	material->pixel[0] = (GRErgb24){ 255, 0, 0 };
	material->pixel[1] = (GRErgb24){ 0, 255, 0 };
	material->pixel[2] = (GRErgb24){ 0, 0, 255 };
	material->pixel[3] = (GRErgb24){ 255, 255, 0 };
	YMGRE_List_Append(&materials, sizeof(gre_material), material);

	CHECK(YMGRE_Material_Find(&materials, "checker") == material,
		"material lookup finds the matching name");
	CHECK(YMGRE_Material_Find(&materials, "missing") == NULL,
		"material lookup rejects an unknown name");
	CHECK(YMGRE_Material_Find(NULL, "checker") == NULL,
		"material lookup accepts a null list");
	CHECK(colorEquals(getPixel(material, 0.1f, 0.1f), material->pixel[0]),
		"UV sampling reads the upper-left texel");
	CHECK(colorEquals(getPixel(material, 0.6f, 0.1f), material->pixel[1]),
		"UV sampling reads the upper-right texel");
	CHECK(colorEquals(getPixel(material, 1.1f, 1.1f), material->pixel[0]),
		"UV sampling repeats outside the unit interval");
	material->valid = 0;
	CHECK(colorEquals(getPixel(material, 0.1f, 0.1f), (GRErgb24){ 128, 128, 128 }),
		"disabled material returns the fallback color");
	material->valid = 1;

	GRE_FramePixel frame[TEST_W * TEST_H];
	float32 depth[TEST_W * TEST_H];
	gre_camera4d camera = { 0 };
	camera.img.width = TEST_W;
	camera.img.height = TEST_H;
	camera.img.data = frame;
	camera.img.zbuff = depth;
	camera.frustum.Znear = 1.0f;
	camera.frustum.Zfar = 500.0f;
	YMGRE_CameraImage_Init(&camera, (GRErgb24){ 0, 0, 0 });

	gre_vertex4d points[3] = { 0 };
	points[0].pos = (gre_fvector4d){ 4.0f, 4.0f, 100.0f, 1.0f };
	points[0].u = 0.01f;
	points[0].v = 0.01f;
	points[1].pos = (gre_fvector4d){ 4.0f, 28.0f, 100.0f, 1.0f };
	points[1].u = 0.01f;
	points[1].v = 0.99f;
	points[2].pos = (gre_fvector4d){ 28.0f, 28.0f, 100.0f, 1.0f };
	points[2].u = 0.99f;
	points[2].v = 0.99f;
	uint16 indices[3] = { 0, 1, 2 };
	gre_polygon4d polygon = { 0 };
	polygon.num = 3;
	polygon.index = indices;
	gre_object4d object = { 0 };
	object.pointNum = 3;
	object.polygonNum = 1;
	object.polygonList = &polygon;
	uint8 hidden[1] = { 0 };
	GRErgb24 litWhite[1] = { { 255, 255, 255 } };
	YMGRE_TrangleObject_Primitive_RasterizationTo(&object, points, hidden,
		litWhite, material, &camera);

	GRE_FramePixel red = GRE_FramePixel_From_RGB24(material->pixel[0]);
	GRE_FramePixel blue = GRE_FramePixel_From_RGB24(material->pixel[2]);
	GRE_FramePixel yellow = GRE_FramePixel_From_RGB24(material->pixel[3]);
	int redCount = 0;
	int blueCount = 0;
	int yellowCount = 0;
	for (int i = 0; i < TEST_W * TEST_H; i++)
	{
		redCount += GRE_FramePixel_Equals(frame[i], red);
		blueCount += GRE_FramePixel_Equals(frame[i], blue);
		yellowCount += GRE_FramePixel_Equals(frame[i], yellow);
	}
	CHECK(redCount > 0 && blueCount > 0 && yellowCount > 0,
		"textured rasterization writes sampled texels to the framebuffer");
	YMGRE_List_Clear(&materials, YMGRE_Free_Material);
}

static void testBmpRoundTrip(const char* path)
{
	GRErgb24 source[6] = {
		{ 1, 2, 3 }, { 40, 50, 60 }, { 70, 80, 90 },
		{ 110, 120, 130 }, { 140, 150, 160 }, { 250, 240, 230 }
	};
	GRErgb24* loaded = NULL;
	uint16 width = 0;
	uint16 height = 0;
	remove(path);
	YMGRE_Image_LoadTo_Bmp_File(path, source, 3, 2);
	YMGRE_Bmp_File_LoadTo_Image(path, &loaded, &width, &height);
	CHECK(width == 3 && height == 2, "BMP round-trip preserves dimensions");
	CHECK(loaded != NULL && memcmp(source, loaded, sizeof(source)) == 0,
		"BMP round-trip preserves padded rows and pixel orientation");
	GRE_ImageBuff_Free(loaded);
	remove(path);
}

static void testTrackedMaterial(const char* resourceRoot)
{
	char bodyPath[512];
	char headPath[512];
	gre_scence scene = { 0 };
	snprintf(bodyPath, sizeof(bodyPath), "%s/obj/Tank1_Body.material", resourceRoot);
	snprintf(headPath, sizeof(headPath), "%s/obj/Tank1_Head.material", resourceRoot);
	YMGRE_ParseMaterialScript(&scene, bodyPath);
	CHECK(scene.MaterialList.len == 1, "tracked material script adds one material");
	GRE_Material material = YMGRE_Material_Find(&scene.MaterialList, "Tank1");
	CHECK(material != NULL, "tracked material name is parsed");
	if (material != NULL)
	{
		CHECK(colorEquals(material->ambient, (GRErgb24){ 178, 178, 178 }),
			"tracked ambient color is parsed");
		CHECK(colorEquals(material->diffuse, (GRErgb24){ 178, 178, 178 }),
			"tracked diffuse color is parsed");
		CHECK(colorEquals(material->specular, (GRErgb24){ 25, 25, 25 }),
			"tracked specular color is parsed");
		CHECK(material->width == 512 && material->height == 512 && material->pixel != NULL,
			"tracked texture is loaded from the material directory");
		if (material->pixel != NULL)
		{
			GRErgb24 first = material->pixel[0];
			int varied = 0;
			for (uint32 i = 1; i < (uint32)material->width * material->height; i++)
			{
				if (!colorEquals(first, material->pixel[i]))
				{
					varied = 1;
					break;
				}
			}
			CHECK(varied, "tracked texture contains decoded image data");
		}
	}
	YMGRE_ParseMaterialScript(&scene, headPath);
	CHECK(scene.MaterialList.len == 1, "duplicate material name is not inserted twice");
	YMGRE_List_Clear(&scene.MaterialList, YMGRE_Free_Material);
}

static void testAdvancedMaterialScript(const char* tempPath)
{
	char scriptPath[512];
	snprintf(scriptPath, sizeof(scriptPath), "%s.material", tempPath);
	FILE* file = fopen(scriptPath, "w");
	CHECK(file != NULL, "temporary advanced material script is created");
	if (file == NULL) return;
	fputs("material GlossTest\n{\n specular_power 96\n}\n", file);
	fclose(file);
	gre_scence scene = { 0 };
	YMGRE_ParseMaterialScript(&scene, scriptPath);
	GRE_Material material = YMGRE_Material_Find(&scene.MaterialList, "GlossTest");
	CHECK(material != NULL && material->advanced != NULL &&
		material->advanced->specularPower == 96,
		"advanced material script parses specular_power");
	YMGRE_List_Clear(&scene.MaterialList, YMGRE_Free_Material);
	remove(scriptPath);
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		printf("usage: test_resource_material RESOURCE_ROOT TEMP_BMP\n");
		return 2;
	}
	testMaterialBasics();
	testBmpRoundTrip(argv[2]);
	testTrackedMaterial(argv[1]);
	testAdvancedMaterialScript(argv[2]);
	if (fails == 0)
		printf("test_resource_material: ALL PASS\n");
	else
		printf("test_resource_material: %d FAILED\n", fails);
	return fails ? 1 : 0;
}
