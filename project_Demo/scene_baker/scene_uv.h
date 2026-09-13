#ifndef SCENE_UV_H
#define SCENE_UV_H
#include "YMGRE_OBJ.h"
#include <stddef.h>
/* Replace UV0 transactionally across all submeshes; retain geometry, normals and other UV sets. */
int SceneUv_Generate(GRE_Object4d mesh, int* islands, char* error, size_t capacity);
int SceneUv_Sphere(GRE_Object4d mesh,unsigned latitude,unsigned longitude,char* error,size_t capacity);
/* Built-in shape IDs match SceneEditorPlaceKind (0..7); sphere uses SceneUv_Sphere. */
int SceneUv_Primitive(GRE_Object4d mesh,unsigned shape,const gre_fvector4d* center,
    const gre_fvector4d axes[3],float scale,int* islands,char* error,size_t capacity);
int SceneUv_GenerateAtlas(GRE_Object4d mesh,const gre_fvector4d* center,const gre_fvector4d axes[3],int* islands,char* error,size_t capacity);
/* Reproduce old autoUv=3 scenes; new unwraps use the proportion-preserving function above. */
int SceneUv_GenerateAtlasLegacy(GRE_Object4d mesh,const gre_fvector4d* center,const gre_fvector4d axes[3],int* islands,char* error,size_t capacity);
#endif
