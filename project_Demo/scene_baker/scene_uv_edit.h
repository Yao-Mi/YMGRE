#ifndef SCENE_UV_EDIT_H
#define SCENE_UV_EDIT_H
#include "YMGRE_OBJ.h"
typedef struct {
    GRE_Object4d mesh;unsigned index,group,island;
    float u,v,startU,startV;unsigned char selected;
} SceneUvEditPoint;
typedef struct {unsigned count;SceneUvEditPoint* points;} SceneUvEdit;
SceneUvEdit* SceneUvEdit_Create(GRE_Object4d mesh);
void SceneUvEdit_Free(SceneUvEdit* edit);
void SceneUvEdit_Select(SceneUvEdit* edit,unsigned point,int island,int add);
void SceneUvEdit_Start(SceneUvEdit* edit);
int SceneUvEdit_Transform(SceneUvEdit* edit,float du,float dv,float degrees,float su,float sv);
int SceneUvEdit_Commit(SceneUvEdit* edit);
void SceneUvEdit_Cancel(SceneUvEdit* edit);
#endif
