#ifndef SCENE_BAKE_H
#define SCENE_BAKE_H
#include "YMGRE_OBJ.h"
#include "YMGRE_List.h"
#include <stddef.h>
#include <stdint.h>

/* UV-space ambient + direct diffuse bake. The returned map belongs to caller. */
GRE_Lightmap SceneBake_Create(GRE_Object4d mesh, GRE_List lights, uint16 size,
    char* error, size_t capacity);
GRE_Lightmap SceneBake_CreateMode(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,char* error,size_t capacity);
GRE_Lightmap SceneBake_CreateView(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,const gre_fvector4d* view,float32 mirrorKs,uint8 power,
    GRErgb24 specularColor,char* error,size_t capacity);
GRE_Lightmap SceneBake_CreateSurface(GRE_Object4d mesh,GRE_List lights,uint16 size,
    uint8 includeLighting,const gre_fvector4d* view,GRE_Material base,char* error,size_t capacity);
uint64_t SceneBake_ModeFingerprint(GRE_Object4d mesh,GRE_List lights,uint8 materialOnly);
uint64_t SceneBake_Fingerprint(GRE_Object4d mesh, GRE_List lights);
/* Each export creates a unique directory, preserving earlier scene/history references. */
int SceneBake_Save(GRE_Lightmap map, uint64_t fingerprint, const char* root,
    char* resourcePath, size_t pathCapacity, char* error, size_t capacity);
GRE_Lightmap SceneBake_Load(const char* path, GRE_Object4d mesh, GRE_List lights,
    char* error, size_t capacity);
/* Material entry point: references BMP images and per-corner UV1. Returned
   base material and lightmap have independent ownership; caller frees both. */
int SceneBake_SaveMaterial(GRE_Lightmap map, uint64_t fingerprint, GRE_Material base,
    const char* root, char* resourcePath, size_t pathCapacity, char* error, size_t capacity);
GRE_Lightmap SceneBake_LoadMaterial(const char* path, GRE_Object4d mesh, GRE_List lights,
    GRE_Material* baseMaterial, char* error, size_t capacity);
#endif
