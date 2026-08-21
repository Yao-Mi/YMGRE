#ifndef YMGRE_CULLING_H
#define YMGRE_CULLING_H
#include "../OPOBJ/YMGRE_OBJ.h"

void YMGRE_Object_FrustumCulling(GRE_Object4d myobj, GRE_Camera4d mycam);//视景体对物体进行剔除
void YMGRE_ObjectPoly_FrustumCulling(GRE_Object4d myobj, GRE_Camera4d mycam); //视景体对多边形进行剔除
void YMGRE_Backface_Remove(GRE_Object4d myobj, GRE_Fvector4d camPos);//背景剔除
void YMGRE_Polygon_clip2D(GRE_fLinesList thislines, GRE_LinesList olines, GRE_FRECT winRect);//多边形边界裁剪

#endif // !YMGRE_CULLING_H
