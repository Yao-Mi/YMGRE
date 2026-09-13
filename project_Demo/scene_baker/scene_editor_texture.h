#ifndef SCENE_EDITOR_TEXTURE_H
#define SCENE_EDITOR_TEXTURE_H
#include "scene_editor_model.h"
#include "YMGRE_List.h"
int SceneEditorTexture_Save(const GRErgb24* pixels,unsigned width,unsigned height,const char* root,char* path,size_t capacity,char* error,size_t errorCapacity);
/* Stage all resources before changing mesh material names; material ownership moves to the list. */
int SceneEditorTexture_Load(SceneEditorObject* object,GRE_List materials,const char* path,char* error,size_t capacity);
#endif
