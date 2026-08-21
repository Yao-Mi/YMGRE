#ifndef YMGRE_COORDINATES_TRANSFORM_H
#define YMGRE_COORDINATES_TRANSFORM_H
#include "../OPOBJ/YMGRE_OBJ.h"

void YMGRE_Object_LocalToWorld(GRE_Object4d myobj);//局部坐标变换到世界坐标
void YMGRE_Object_WorldToCamera(GRE_Object4d myobj, GRE_FMat4x4 camera);//物体变换到相机坐标系
void YMGRE_Object_CameraToViewPlane(GRE_Object4d myobj, float32 viewPlaneDis);//相机坐标变换到视平面
void YMGRE_Object_ViewPlaneToWindows(GRE_Object4d myobj, GRE_Camera4d mycam);//视平面到窗口的变换

void YMGRE_Point_WorldToCamera(GRE_Fvector4d point, GRE_Fvector4d out, GRE_FMat4x4 camera);//点 变换到相机坐标系
void YMGRE_Point_CameraToViewPlane(GRE_Fvector4d point, float32 viewPlaneDis);//点变换到视平面
void YMGRE_Point_ViewPlaneToWindows(GRE_Fvector4d point, GRE_Camera4d mycam);//点变换到窗口

#endif // !YMGRE_COORDINATES_TRANSFORM_H
