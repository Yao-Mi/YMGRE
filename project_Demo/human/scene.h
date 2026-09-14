#ifndef SKE_HUMAN_SCENE_H
#define SKE_HUMAN_SCENE_H
#include "YMGRE_Rendering_Pipeline.h"
#include "YMSKE/BRIDGE/YMSKE_GRE.h"
#include "YMSKE/IO/YMSKE_IO.h"
enum
{
	DEMO_POINTS = 7168, /* Preallocated capacities; actual counts come from files. */
	DEMO_FACES = 3584,
	DEMO_BONES = 63,
	DEMO_FRAMES = 1,
	DEMO_WIDTH = 960,
	DEMO_HEIGHT = 640
};
typedef struct demo_scene_
{
	ske_io_view asset, animation;
	ske_bone_desc bindBones[DEMO_BONES];
	ske_io_name boneNames[DEMO_BONES];
	ske_vec3 tails[DEMO_BONES];
	ske_vertex vertices[DEMO_POINTS];
	ske_mat34 samples[DEMO_FRAMES * DEMO_BONES];
	ske_skeleton skeleton;
	ske_bone bones[DEMO_BONES];
	uint16_t order[DEMO_BONES];
	ske_pose pose;
	ske_pose_bone poseBones[DEMO_BONES];
	float orbit, elevation, distance;
	int selected, axis;
	GRE_Material material;
	gre_listnode materialNode;
	ske_skin skin;
	ske_gre_binding binding;
	GRE_Object4d object;
	GRE_Camera4d camera;
	GRE_Light4d ambient, key;
	gre_list objects, lights, materials;
	gre_list loadedMaterials; /* 原生网格加载器创建的材质链表。 */
	gre_listnode objectNode, lightNodes[2];
	gre_render_workspace workspace;
	gre_vertex4d workPoints[DEMO_POINTS];
	gre_vertex4d_wN workNormals[DEMO_POINTS];
	uint8 workHide[DEMO_FACES];
	GRErgb24 workColor[DEMO_FACES];
	gre_fvector4d workLights[2];
} demo_scene;
int Demo_Init(demo_scene *s);
/* root contains human.mesh/material and human.ske/ska (or .txt with text != 0).
 * Caller calls Demo_Destroy even on failure, before retrying initialization. */
int Demo_InitFiles(demo_scene *s, const char *root, int text);
int Demo_Update(demo_scene *s, float frame);
void Demo_ViewReset(demo_scene *s);
void Demo_ViewMove(demo_scene *s, float yaw, float pitch, float zoom);
void Demo_Render(demo_scene *s, int wire, int bones);
int Demo_Save(demo_scene *s, const char *path);
int Demo_Validate(demo_scene *s);
void Demo_Destroy(demo_scene *s);
#endif
