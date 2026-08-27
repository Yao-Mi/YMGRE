#ifndef SCENE_EDITOR_PLACE_H
#define SCENE_EDITOR_PLACE_H

#include "YMGUI_Obj.h"
#include "YMGRE_OBJ.h"

typedef enum {
	SCENE_PLACE_PLANE,
	SCENE_PLACE_CUBE,
	SCENE_PLACE_BOX,
	SCENE_PLACE_SPHERE,
	SCENE_PLACE_CYLINDER,
	SCENE_PLACE_CONE,
	SCENE_PLACE_TORUS,
	SCENE_PLACE_CAPSULE,
	SCENE_PLACE_POINT_LIGHT,
	SCENE_PLACE_SPOT_LIGHT,
	SCENE_PLACE_CAMERA
} SceneEditorPlaceKind;

typedef struct {
	SceneEditorPlaceKind kind;
	char name[64];
	char type[24];
	float32 x, y, z;
	float32 targetX, targetY, targetZ;
	float32 rotationY;
	float32 scale;
	float32 strength;
	uint16 detailA, detailB;
	GYcolor color;
	uint8 wireframe;
	uint8 shadowsEnabled;
	GRE_Object4d mesh;
} SceneEditorPlaceResult;

typedef void (*SceneEditorPlaceCreatedCb)(const SceneEditorPlaceResult* result, void* userData);

void SceneEditorPlace_Build(GYOBJ toolbar, GYcoord buttonX, GYCTX context,
	SceneEditorPlaceCreatedCb createdCb, void* userData);
GRE_Object4d SceneEditorPlace_CreateMesh(const SceneEditorPlaceResult* result);
void SceneEditorPlace_TransformMesh(GRE_Object4d mesh, float32 x, float32 y, float32 z,
	float32 rotationY, float32 scale, uint8 wireframe);

#endif
