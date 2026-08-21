#ifndef YMGRE_RASTERIZATION_H
#define YMGRE_RASTERIZATION_H
#include "../OPOBJ/YMGRE_OBJ.h"

//图元光栅化
void YMGRE_PolygonObject_Primitive_Rasterization(GRE_Object4d myobj, GRE_Camera4d mycam, uint8 showLines);
//三角图元光栅化
void YMGRE_TrangleObject_Primitive_Rasterization(GRE_Object4d myTrangleObj, GRE_Material mymaterial, GRE_Camera4d mycam);

//三角图元线框模型绘制
void YMGRE_TrangleObject_Wires(GRE_Object4d myTrangleObj, GRE_Camera4d mycam);

//灯光光栅化绘制
void YMGRE_Light_Primitive_Rasterization(GRE_Light4d mylight, GRE_Camera4d mycam, uint8 showLightSize);


#endif // !YMGRE_RASTERIZATION_H

