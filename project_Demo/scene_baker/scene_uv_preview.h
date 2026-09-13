#ifndef SCENE_UV_PREVIEW_H
#define SCENE_UV_PREVIEW_H
#include "scene_uv_edit.h"
#include "YMGRE_List.h"
/* checker: 0=current material, 1=square grid, 2=2:1 map grid.
   Read-only albedo inspection. Uses staged UVs, never the object's baked lightmap. */
const GRErgb24* SceneUvPreview_Checker(int wide);
int SceneUvPreview_Render(const SceneUvEdit* edit,GRE_List materials,int checker,GRErgb24 fallback,
    float yaw,float pitch,float zoom,unsigned size,GRErgb24* output);
int SceneUvPreview_RenderImage(const SceneUvEdit* edit,const GRErgb24* pixels,unsigned width,unsigned height,
    float yaw,float pitch,float zoom,unsigned size,GRErgb24* output);
#endif
