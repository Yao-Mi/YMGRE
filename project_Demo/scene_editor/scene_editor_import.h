#ifndef SCENE_EDITOR_IMPORT_H
#define SCENE_EDITOR_IMPORT_H

#include "YMGUI_Obj.h"

typedef uint8 (*SceneEditorImportCb)(const char* path, uint8 wireframe, void* userData);

void SceneEditorImport_Build(GYCTX context, SceneEditorImportCb importCb, void* userData);
void SceneEditorImport_Open(void);

#endif
