#ifndef SCENE_UV_EDITOR_H
#define SCENE_UV_EDITOR_H
#include "scene_editor_model.h"
#include "YMGRE_List.h"
typedef void (*SceneUvEditorChanged)(const char*,void*);
typedef SceneEditorObject* (*SceneUvEditorHistory)(SceneEditorObject*,int,void*);
typedef int (*SceneUvEditorApply)(SceneEditorObject*,const GRErgb24*,unsigned,unsigned,char*,size_t,void*);
void SceneUvEditor_Open(GYOBJ parent,SceneEditorObject* object,GRE_List materials,SceneUvEditorChanged changed,SceneUvEditorHistory history,SceneUvEditorApply apply,void* user);
void SceneUvEditor_Close(void);
int SceneUvEditor_IsOpen(void);
void SceneUvEditor_Shutdown(void);
void SceneUvEditor_Tick(void);
#endif
