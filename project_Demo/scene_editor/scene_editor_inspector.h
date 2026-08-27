#ifndef SCENE_EDITOR_INSPECTOR_H
#define SCENE_EDITOR_INSPECTOR_H

#include "scene_editor_model.h"
#include "YMGUI_Obj.h"

typedef void (*SceneEditorInspectorChangedCb)(const char* status, void* userData);
typedef uint8 (*SceneEditorInspectorRemeshCb)(SceneEditorObject* object,
	uint16 detailA, uint16 detailB, void* userData);

void SceneEditorInspector_Build(GYOBJ parent, SceneEditorInspectorChangedCb changedCb,
	SceneEditorInspectorRemeshCb remeshCb, void* userData);
void SceneEditorInspector_SetObject(SceneEditorObject* object);
void SceneEditorInspector_Tick(uint16 elapsedMs);
void SceneEditorInspector_Shutdown(void);

#endif
