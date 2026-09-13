#ifndef SCENE_UV_DATA_H
#define SCENE_UV_DATA_H
#include "YMGRE_OBJ.h"
#include <stddef.h>
typedef struct {
    unsigned parts, triangles;
    unsigned counts[64];
    float corners[];
} SceneUvData;
SceneUvData* SceneUvData_Capture(GRE_Object4d mesh);
SceneUvData* SceneUvData_Parse(const char* text);
char* SceneUvData_Text(const SceneUvData* data);
int SceneUvData_Apply(GRE_Object4d mesh,const SceneUvData* data);
#endif
