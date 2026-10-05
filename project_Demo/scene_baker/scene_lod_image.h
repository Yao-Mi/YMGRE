#ifndef SCENE_LOD_IMAGE_H
#define SCENE_LOD_IMAGE_H

#include "../nature_preview/grass_impostor.h"
#include <stddef.h>

/* Eight azimuth views, with a separate coverage mask for cutout pixels. */
int SceneLodImage_Bake(grass_impostor *image,GRE_Object4d source,
    GRE_List materials,GRE_List lights);
int SceneLodImage_Save(const grass_impostor *image,const char *directory,
    const gre_fvector4d *origin,char *error,size_t errorCapacity);
int SceneLodImage_Load(grass_impostor *image,const char *path);

#endif
