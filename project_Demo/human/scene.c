#include "scene.h"
#include "YMCS_File_IO.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int loadFile(const char *path, const ske_io_storage *storage, ske_io_kind kind, ske_io_view *out)
{
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Cannot open human asset: %s\n", path);
		return 0;
	}
	ske_io_info info;
	ske_io_encoding encoding;
	ske_io_result result = YMSKE_IO_Probe(f, &info, &encoding);
	if (result == SKE_IO_OK && info.kind != kind)
		result = SKE_IO_MISMATCH;
	if (result == SKE_IO_OK && kind == SKE_IO_ANIMATION && info.frameCount > DEMO_FRAMES)
		result = SKE_IO_CAPACITY;
	if (result == SKE_IO_OK)
		result = YMSKE_IO_Read(f, storage, out);
	if (fclose(f) != 0 && result == SKE_IO_OK)
		result = SKE_IO_STREAM;
	if (result != SKE_IO_OK)
		fprintf(stderr, "Invalid human asset: %s (IO error %d)\n", path, (int)result);
	return result == SKE_IO_OK;
}
static int meshMatches(const char *path, uint64_t expected)
{
	FILE *f = fopen(path, "rb");
	if (!f)
	{
		fprintf(stderr, "Cannot open human mesh: %s\n", path);
		return 0;
	}
	uint64_t hash = UINT64_C(14695981039346656037);
	unsigned char buffer[16384];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), f)) != 0)
		for (size_t i = 0; i < n; ++i)
			hash = (hash ^ buffer[i]) * UINT64_C(1099511628211);
	int ok = !ferror(f) && hash == expected;
	if (fclose(f) != 0)
		ok = 0;
	if (!ok)
		fprintf(stderr, "Human mesh fingerprint mismatch or read error: %s\n", path);
	return ok;
}
int Demo_Init(demo_scene *s)
{
	return Demo_InitFiles(s, HUMAN_ASSET_ROOT, 0);
}
int Demo_InitFiles(demo_scene *s, const char *root, int text)
{
	if (!s)
		return 0;
	memset(s, 0, sizeof(*s));
	if (!root || !*root)
		return 0;
	Demo_ViewReset(s);
	s->selected = 23;
	s->axis = 0;
	const char *names[] = {"human.mesh", "human.material", text ? "human.ske.txt" : "human.ske",
						   text ? "human.ska.txt" : "human.ska"};
	char paths[4][4096];
	for (int i = 0; i < 4; ++i)
	{
		int n = snprintf(paths[i], sizeof(paths[i]), "%s/%s", root, names[i]);
		if (n < 0 || (size_t)n >= sizeof(paths[i]))
		{
			fprintf(stderr, "Human asset path too long: %s\n", names[i]);
			return 0;
		}
	}
	ske_io_storage storage = {s->bindBones, s->boneNames, s->tails,	   s->vertices,
							  s->samples,	DEMO_BONES,	  DEMO_POINTS, DEMO_BONES * DEMO_FRAMES};
	if (!loadFile(paths[2], &storage, SKE_IO_SKELETON, &s->asset) ||
		!loadFile(paths[3], &storage, SKE_IO_ANIMATION, &s->animation))
		return 0;
	if (YMSKE_IO_Match(&s->asset.info, &s->animation.info) != SKE_IO_OK)
	{
		fprintf(stderr, "Human skeleton/animation mismatch: %s and %s\n", paths[2], paths[3]);
		return 0;
	}
	if (!meshMatches(paths[0], s->asset.info.meshId))
		return 0;
	for (int i = 1; i < 2; ++i)
	{
		FILE *f = fopen(paths[i], "rb");
		if (!f)
		{
			fprintf(stderr, "Missing human asset: %s\n", paths[i]);
			return 0;
		}
		fclose(f);
	}
	if (YMSKE_Skeleton_Init(&s->skeleton, s->asset.bones, s->asset.info.boneCount, s->bones, s->order,
							DEMO_BONES) != SKE_OK ||
		YMSKE_Pose_Init(&s->pose, &s->skeleton, s->poseBones, DEMO_BONES) != SKE_OK)
		return 0;
	gre_scence imported = {0};
	s->object = YMGRE_LoadOgreMeshAndMaterial(&imported, paths[0]);
	s->loadedMaterials = imported.MaterialList;
	s->materials = s->loadedMaterials;
	if (s->object && (s->object->pointNum != (int)s->asset.info.vertexCount ||
					  s->object->polygonNum > DEMO_FACES || s->object->nextObject))
		return 0;
	if (s->object)
		for (int i = 0; i < (int)s->asset.info.vertexCount; ++i)
		{
			gre_fvector4d p = s->object->pointList[i].pos;
			ske_vec3 q = s->asset.vertices[i].position;
			if (fabsf(p.x - q.x) + fabsf(p.y - q.y) + fabsf(p.z - q.z) > 1e-5f)
				return 0;
		}
	if (!s->object || !YMGRE_Object_EnableVertexAttributes(s->object))
		return 0;
	for (int i = 0; i < (int)s->asset.info.vertexCount; ++i)
		s->object->pointList_wN[i].color = (GRErgb24){255, 255, 255};
	s->object->boundType = GRE_Bounding_Sphere_R;
	s->object->renderMode = GRE_RenderMode_Vertex;
	if (YMSKE_Skin_Init(&s->skin, &s->skeleton, s->asset.vertices, s->asset.info.vertexCount) != SKE_OK ||
		YMSKE_GRE_Init(&s->binding, &s->skin, s->object) != SKE_OK)
		return 0;
	s->objectNode = (gre_listnode){sizeof(*s->object), s->object, NULL};
	s->objects = (gre_list){&s->objectNode, 1};
	s->ambient = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){255, 255, 255}, .55f);
	s->key = YMGRE_Creat_Light(1, GRE_PointLight, (GRErgb24){255, 250, 235}, .7f);
	if (!s->ambient || !s->key)
		return 0;
	s->key->pos = (gre_fvector4d){8, 12, 15, 1};
	s->key->proper.kc0 = 1;
	s->key->proper.kc1 = s->key->proper.kc2 = 0;
	s->lightNodes[0] = (gre_listnode){sizeof(*s->ambient), s->ambient, &s->lightNodes[1]};
	s->lightNodes[1] = (gre_listnode){sizeof(*s->key), s->key, NULL};
	s->lights = (gre_list){s->lightNodes, 2};
	s->camera = YMGRE_Creat_Camera(0, DEMO_WIDTH, DEMO_HEIGHT, 28, 28, 21, 21);
	if (!s->camera)
		return 0;
	gre_fvector4d eye = {2.4f, 1.7f, 4.4f, 1}, lookAt = {0, -.25f, 0, 1};
	YMGRE_Camera_Frustum_Init(s->camera, .1f, 80);
	YMGRE_UVNCamera_PositionInit(s->camera, &eye, &lookAt, NULL, 0);
	YMGRE_RenderWorkspace_Init(&s->workspace, s->workPoints, DEMO_POINTS, s->workHide, s->workColor,
							   DEMO_FACES, s->workLights, 2);
	YMGRE_RenderWorkspace_BindVertexAttributes(&s->workspace, s->workNormals, DEMO_POINTS);
	return Demo_Update(s, 0);
}
void Demo_ViewReset(demo_scene *s)
{
	s->orbit = .20f;
	s->elevation = .16f;
	s->distance = 23.f;
}
void Demo_ViewMove(demo_scene *s, float yaw, float pitch, float zoom)
{
	if (!isfinite(yaw + pitch + zoom))
		return;
	s->orbit = remainderf(s->orbit + yaw, 6.283185307f);
	s->elevation = fminf(1.35f, fmaxf(-1.35f, s->elevation + pitch));
	s->distance = fminf(45, fmaxf(8.f, s->distance * expf(fminf(2, fmaxf(-2, zoom)))));
}
/* The source SKA has one bind frame. A selected-joint offset is a demo control. */
int Demo_Update(demo_scene *s, float degrees)
{
	if (!s || !s->animation.samples || !isfinite(degrees))
		return 0;
	for (size_t i = 0; i < s->asset.info.boneCount; ++i)
		s->poseBones[i].local = s->animation.samples[i];
	if (s->selected >= 0 && s->selected < (int)s->asset.info.boneCount)
	{
		float half = degrees * .00872664626f;
		ske_quat q = {0, 0, 0, cosf(half)};
		if (s->axis == 0)
			q.x = sinf(half);
		else if (s->axis == 1)
			q.y = sinf(half);
		else
			q.z = sinf(half);
		ske_mat34 rotation;
		if (YMSKE_Mat_TRS((ske_vec3){0, 0, 0}, q, (ske_vec3){1, 1, 1}, &rotation) != SKE_OK)
			return 0;
		s->poseBones[s->selected].local = YMSKE_Mat_Multiply(s->poseBones[s->selected].local, rotation);
	}
	return YMSKE_Pose_Update(&s->pose) == SKE_OK && YMSKE_GRE_Update(&s->binding, &s->pose) == SKE_OK;
}
void Demo_Render(demo_scene *s, int wire, int bones)
{
	float radius = s->distance * cosf(s->elevation);
	gre_fvector4d eye = {radius * sinf(s->orbit), -.25f + s->distance * sinf(s->elevation),
						 radius * cosf(s->orbit), 1},
				  lookAt = {0, -.25f, 0, 1};
	YMGRE_UVNCamera_PositionInit(s->camera, &eye, &lookAt, NULL, 0);
	s->camera->wireFrame = GRE_Render_Solid;
	s->object->wireFrame = (uint8)(wire != 0);
	YMGRE_Camera_TanglePipline_wN(s->camera, &s->lights, &s->objects, &s->materials, &s->workspace);
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(s->camera);
	for (size_t i = 0; i < DEMO_WIDTH * DEMO_HEIGHT; ++i)
		if (target->zbuff[i] >= s->camera->frustum.Zfar)
			target->data[i] = GRE_FramePixel_From_RGB24((GRErgb24){15, 21, 31});
	if (bones)
	{
		gre_line3d lines[DEMO_BONES];
		int count = 0;
		for (int i = 0; i < (int)s->asset.info.boneCount; ++i)
		{
			/* Skip unattached rig control handles; keep the actual body hierarchy. */
			if (i < 11)
				continue;
			ske_vec3 a = YMSKE_Mat_Point(s->poseBones[i].global, (ske_vec3){0, 0, 0});
			ske_vec3 b = YMSKE_Mat_Point(s->poseBones[i].global, s->asset.tails[i]);
			lines[count++] =
				(gre_line3d){{a.x, a.y, a.z, 1},
							 {b.x, b.y, b.z, 1},
							 i == s->selected ? (GRErgb24){255, 185, 65} : (GRErgb24){90, 220, 255},
							 2};
		}
		YMGRE_Camera_LineList_Rendering(s->camera, lines, (uint32)count, 0);
	}
}
int Demo_Save(demo_scene *s, const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return 0;
	fprintf(f, "P6\n%d %d\n255\n", DEMO_WIDTH, DEMO_HEIGHT);
	/* 仅离屏导出时做 RGB888 转换；实时显示直接借用 GRE framebuffer。 */
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(s->camera);
	unsigned char row[DEMO_WIDTH * 3];
	int ok = 1;
	for (size_t y = 0; y < DEMO_HEIGHT && ok; ++y)
	{
		for (size_t x = 0; x < DEMO_WIDTH; ++x)
		{
			GRErgb24 c = GRE_FramePixel_To_RGB24(target->data[y * DEMO_WIDTH + x]);
			row[x * 3] = c.R;
			row[x * 3 + 1] = c.G;
			row[x * 3 + 2] = c.B;
		}
		ok = fwrite(row, 1, sizeof(row), f) == sizeof(row);
	}
	if (fclose(f) != 0)
		ok = 0;
	return ok;
}
int Demo_Validate(demo_scene *s)
{
	for (int i = 0; i < (int)s->asset.info.vertexCount; ++i)
	{
		gre_fvector4d n = s->object->pointList_wN[i].normal;
		gre_fvector4d p = s->object->pointList[i].pos;
		if (!isfinite(p.x + p.y + p.z + n.x + n.y + n.z) ||
			fabsf(n.x * n.x + n.y * n.y + n.z * n.z - 1) > 1e-4f)
			return 0;
	}
	return 1;
}
void Demo_Destroy(demo_scene *s)
{
	if (s->camera)
		YMGRE_Free_Camera(s->camera);
	if (s->ambient)
		YMGRE_Free_Light(s->ambient);
	if (s->key)
		YMGRE_Free_Light(s->key);
	if (s->material)
		YMGRE_Free_Material(s->material);
	YMGRE_List_Clear(&s->loadedMaterials, YMGRE_Free_Material);
	if (s->object)
		YMGRE_Free_Object(s->object);
	memset(s, 0, sizeof(*s));
}
