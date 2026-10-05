#ifndef SCENE_MODEL_EXPORT_H
#define SCENE_MODEL_EXPORT_H
#include "YMGRE_OBJ.h"
#include "YMGRE_List.h"
#include "scene_lod_image.h"
#include <stddef.h>
/* Exports static Ogre v1.40 mesh, companion material and copied bitmap resources.
   origin is subtracted from world vertices; rotation/scale remain in geometry. */
int SceneModel_Export(GRE_Object4d mesh,GRE_List materials,GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,const char* stem,
    char* meshPath,size_t pathCapacity,char* error,size_t errorCapacity);
/* Write one model and its companion files into an existing package directory. */
int SceneModel_ExportIntoDirectory(GRE_Object4d mesh,GRE_List materials,GRE_Material fallback,
    const gre_fvector4d* origin,const char* directory,const char* stem,
    char* meshPath,size_t pathCapacity,char* error,size_t errorCapacity);
int SceneModel_ExportLOD(GRE_Object4d levels[3],GRE_List materials[3],GRE_Material fallback,
    const gre_fvector4d* origin,const char* root,float nearPixels,float middlePixels,
    float hysteresis,char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity);
int SceneModel_ExportLODWithImage(GRE_Object4d levels[3],GRE_List materials[3],
    const grass_impostor *farImage,GRE_Material fallback,const gre_fvector4d* origin,
    const char* root,float nearPixels,float middlePixels,float hysteresis,
    char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity);
int SceneModel_ExportLODWithImageNamed(GRE_Object4d levels[3],GRE_List materials[3],
    const grass_impostor *farImage,GRE_Material fallback,const gre_fvector4d* origin,
    const char* root,const char* objectName,float nearPixels,float middlePixels,float hysteresis,
    char* lodPath,size_t pathCapacity,char* error,size_t errorCapacity);
int SceneModel_ExportBaked(GRE_Object4d source,GRE_Lightmap map,GRE_Material base,
    const gre_fvector4d* origin,const char* directory,char* path,size_t pathCapacity,
    char* error,size_t capacity);
#endif
