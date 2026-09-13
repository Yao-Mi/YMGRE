#ifndef SCENE_TEXTURE_IMAGE_H
#define SCENE_TEXTURE_IMAGE_H
#include "YMGRE_OBJ.h"
#include <stddef.h>
typedef struct { GRErgb24* pixels;unsigned width,height; } SceneTextureImage;
void SceneTextureImage_Free(SceneTextureImage* image);
/* PNG/JPEG/BMP, maximum 4096 per side; alpha is composited over white for RGB materials. */
int SceneTextureImage_Read(const char* path,SceneTextureImage* image,char* error,size_t capacity);
#endif
