#ifndef SCENE_EDITOR_COLOR_H
#define SCENE_EDITOR_COLOR_H

#include "YMGUI_Obj.h"

typedef void (*SceneEditorColorSelectedCb)(GYcolor color, void* userData);

void SceneEditorColor_Build(GYCTX context);
void SceneEditorColor_Open(GYcolor initialColor, SceneEditorColorSelectedCb selectedCb, void* userData);

#endif
