#ifndef YMGRE_COORDINATES_TRANSFORM_H
#define YMGRE_COORDINATES_TRANSFORM_H
#include "../OPOBJ/YMGRE_OBJ.h"

void YMGRE_Object_LocalToWorld(GRE_Object4d myobj);//局部坐标变换到世界坐标
void YMGRE_Object_WorldToCamera(GRE_Object4d myobj, GRE_FMat4x4 camera);//物体变换到相机坐标系
void YMGRE_Object_CameraToViewPlane(GRE_Object4d myobj, float32 viewPlaneDis);//相机坐标变换到视平面
void YMGRE_Object_ViewPlaneToWindows(GRE_Object4d myobj, GRE_Camera4d mycam);//视平面到窗口的变换
void YMGRE_Object_WorldToCameraTo(GRE_Object4d myobj, GRE_FMat4x4 camera, GRE_Vertex4d out);//物体变换到外部相机顶点缓存
void YMGRE_VertexList_CameraToViewPlane(GRE_Vertex4d points, uint32 pointNum, float32 viewPlaneDis);//外部顶点缓存变换到视平面
void YMGRE_VertexList_ViewPlaneToWindows(GRE_Vertex4d points, uint32 pointNum, GRE_Camera4d mycam);//外部顶点缓存变换到窗口
void YMGRE_Object_WorldToCameraTo_wN(GRE_Object4d myobj, GRE_FMat4x4 camera, GRE_Vertex4d_wN out);
void YMGRE_VertexList_CameraToViewPlane_wN(GRE_Vertex4d_wN points, uint32 pointNum, float32 viewPlaneDis);
void YMGRE_VertexList_ViewPlaneToWindows_wN(GRE_Vertex4d_wN points, uint32 pointNum, GRE_Camera4d mycam);

void YMGRE_Point_WorldToCamera(GRE_Fvector4d point, GRE_Fvector4d out, GRE_FMat4x4 camera);//点 变换到相机坐标系
void YMGRE_Point_CameraToViewPlane(GRE_Fvector4d point, float32 viewPlaneDis);//点变换到视平面
void YMGRE_Point_ViewPlaneToWindows(GRE_Fvector4d point, GRE_Camera4d mycam);//点变换到窗口

#endif // !YMGRE_COORDINATES_TRANSFORM_H
