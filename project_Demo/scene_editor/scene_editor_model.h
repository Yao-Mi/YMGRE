#ifndef SCENE_EDITOR_MODEL_H
#define SCENE_EDITOR_MODEL_H

#include "YMGUI_PubType.h"
#include "YMGUI_TreeView.h"
#include "YMGRE_OBJ.h"

typedef enum {
	SCENE_OBJECT_MESH,
	SCENE_OBJECT_LIGHT,
	SCENE_OBJECT_CAMERA
} SceneEditorObjectKind;

typedef struct {
	uint8 active;
	char name[64];
	char type[24];
	SceneEditorObjectKind kind;
	float32 x, y, z;
	float32 targetX, targetY, targetZ;
	float32 scale;
	float32 rotY;
	float32 strength;
	GYcolor color;
	uint8 visible;
	uint8 fixed;
	uint8 wireframe;
	uint8 shadowsEnabled;
	uint8 primitiveKind;
	uint16 detailA, detailB;
	GRE_LightType lightType;
	GRE_Object4d mesh;
	GRE_Light4d light;
	GRE_Camera4d camera;
	GYTREENODE treeNode;
} SceneEditorObject;

void SceneEditorObject_Translate(SceneEditorObject* object, float32 dx, float32 dy, float32 dz);
void SceneEditorObject_SetScale(SceneEditorObject* object, float32 scale);
void SceneEditorObject_SetRotationY(SceneEditorObject* object, float32 rotationY);
void SceneEditorObject_SetColor(SceneEditorObject* object, GYcolor color);
void SceneEditorObject_SyncHandle(SceneEditorObject* object);

#endif
