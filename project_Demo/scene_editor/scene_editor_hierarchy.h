#ifndef SCENE_EDITOR_HIERARCHY_H
#define SCENE_EDITOR_HIERARCHY_H

#include "scene_editor_model.h"

typedef struct {
	void (*renameObject)(SceneEditorObject* object, const char* name, void* userData);
	void (*deleteObject)(SceneEditorObject* object, void* userData);
	void (*switchCamera)(SceneEditorObject* object, void* userData);
} SceneEditorHierarchyOps;

void SceneEditorHierarchy_Build(GYCTX context, const SceneEditorHierarchyOps* ops, void* userData);
void SceneEditorHierarchy_Open(SceneEditorObject* object, GYcoord screenX, GYcoord screenY,
	uint8 canDelete);

#endif
