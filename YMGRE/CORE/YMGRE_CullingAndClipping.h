#ifndef YMGRE_CULLING_H
#define YMGRE_CULLING_H
#include "../OPOBJ/YMGRE_OBJ.h"

#define YMGRE_FRUSTUM_CLIP_VERTEX_MAX 16

void YMGRE_Object_FrustumCulling(GRE_Object4d myobj, GRE_Camera4d mycam);//视景体对物体进行剔除
void YMGRE_ObjectPoly_FrustumCulling(GRE_Object4d myobj, GRE_Camera4d mycam); //视景体对多边形进行剔除
void YMGRE_Backface_Remove(GRE_Object4d myobj, GRE_Fvector4d camPos);//背景剔除
uint8 YMGRE_Object_FrustumCullingCal(GRE_Object4d myobj, GRE_Camera4d mycam);//计算物体是否位于视景体外，不修改物体
void YMGRE_ObjectPoly_FrustumCullingTo(GRE_Object4d myobj, GRE_Vertex4d points, GRE_Camera4d mycam, uint8* polygonHide);//将多边形剔除结果写入外部缓存
uint16 YMGRE_Polygon_FrustumClip(GRE_Vertex4d input, uint16 inputNum, GRE_Vertex4d output,
	uint16 outputMax, GRE_Camera4d camera);//在相机空间裁剪多边形，并插值生成交点
void YMGRE_Backface_RemoveTo(GRE_Object4d myobj, GRE_Fvector4d camPos, uint8* polygonHide);//将背面剔除结果写入外部缓存
void YMGRE_Polygon_clip2D(GRE_fLinesList thislines, GRE_LinesList olines, GRE_FRECT winRect);//多边形边界裁剪

#endif // !YMGRE_CULLING_H
