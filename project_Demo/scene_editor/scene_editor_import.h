#ifndef SCENE_EDITOR_IMPORT_H
#define SCENE_EDITOR_IMPORT_H

#include "YMGUI_Obj.h"
#include "YMGRE_OBJ.h"

typedef uint8 (*SceneEditorImportCb)(const char* path, uint8 wireframe, void* userData);

void SceneEditorImport_Build(GYCTX context, SceneEditorImportCb importCb, void* userData);
void SceneEditorImport_Open(void);
uint8 SceneEditorImport_Validate(const char* meshPath, char* message, size_t capacity);
uint8 SceneEditorImport_ValidateLoadedMaterials(const char* meshPath, GRE_Object4d mesh,
	char* message, size_t capacity);

#endif
