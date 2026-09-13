#ifndef SCENE_EDITOR_INSPECTOR_H
#define SCENE_EDITOR_INSPECTOR_H

#include "scene_editor_model.h"
#include "YMGRE_List.h"
#include "YMGUI_Obj.h"

typedef void (*SceneEditorInspectorChangedCb)(const char* status, void* userData);
typedef uint8 (*SceneEditorInspectorRemeshCb)(SceneEditorObject* object,
	uint16 detailA, uint16 detailB, void* userData);

typedef SceneEditorObject* (*SceneEditorInspectorUvHistoryCb)(SceneEditorObject*,int,void*);
typedef int (*SceneEditorInspectorUvApplyCb)(SceneEditorObject*,const GRErgb24*,unsigned,unsigned,char*,size_t,void*);
void SceneEditorInspector_SetUvApplyCallback(SceneEditorInspectorUvApplyCb cb);
void SceneEditorInspector_SetUvMaterials(GRE_List materials);
void SceneEditorInspector_SetUvHistoryCallback(SceneEditorInspectorUvHistoryCb cb);

void SceneEditorInspector_Build(GYOBJ parent, SceneEditorInspectorChangedCb changedCb,
	SceneEditorInspectorRemeshCb remeshCb, void* userData);
void SceneEditorInspector_SetObject(SceneEditorObject* object);
void SceneEditorInspector_SetRayTracing(uint8 enabled);
void SceneEditorInspector_Shutdown(void);

#endif
