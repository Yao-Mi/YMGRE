#ifndef SCENE_TEXTURE_PAINT_H
#define SCENE_TEXTURE_PAINT_H
#include "YMGUI_Obj.h"
#include "YMGRE_OBJ.h"
typedef void (*SceneTexturePaintDone)(int accepted,const GRErgb24* pixels,unsigned width,unsigned height,void* user);
/* Finish flattens layers over white; cancel leaves the supplied image untouched. */
void SceneTexturePaint_Open(GYCTX ctx,const GRErgb24* seed,unsigned width,unsigned height,SceneTexturePaintDone done,void* user);
void SceneTexturePaint_Tick(void);
void SceneTexturePaint_Close(void);
void SceneTexturePaint_Shutdown(void);
#endif
