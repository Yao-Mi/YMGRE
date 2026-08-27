#ifndef YMGRE_RASTERIZATION_H
#define YMGRE_RASTERIZATION_H
#include "../OPOBJ/YMGRE_OBJ.h"

//基础直线光栅化，不包含窗口裁剪
void YMGRE_Img_SetBrushColor(GRErgb24 color);
void YMGRE_Img_Line(GRE_FrameBuffer data, uint16 width, uint16 height, int16 x1, int16 y1, int16 x2, int16 y2);
void YMGRE_Img_LineDepth(GRE_FrameBuffer data, float32* zbuff, uint16 width, uint16 height,
	int16 x1, int16 y1, float32 z1, int16 x2, int16 y2, float32 z2, GRErgb24 color, uint8 depthTest);
//多边形扫描线填充，输入边必须已经位于图像范围内
void YMGRE_Img_Scanline_AreaFill(GRE_FrameBuffer data, uint16 width, uint16 height, GRE_LinesList ring,
	GRE_Fvector4d plane, float32* zbuff, GRErgb24 fillColor);

//图元光栅化
void YMGRE_PolygonObject_Primitive_Rasterization(GRE_Object4d myobj, GRE_Camera4d mycam, uint8 showLines);
void YMGRE_PolygonObject_Primitive_RasterizationTo(GRE_Object4d myobj, GRE_Vertex4d points, uint8* polygonHide,
	GRErgb24* polygonColor, GRE_Camera4d mycam, uint8 showLines);//使用外部相机工作区绘制多边形
//三角图元光栅化
void YMGRE_TrangleObject_Primitive_Rasterization(GRE_Object4d myTrangleObj, GRE_Material mymaterial, GRE_Camera4d mycam);
void YMGRE_TrangleObject_Primitive_RasterizationTo(GRE_Object4d myTrangleObj, GRE_Vertex4d points, uint8* polygonHide,
	GRErgb24* polygonColor, GRE_Material mymaterial, GRE_Camera4d mycam);//使用外部相机工作区绘制三角形

//三角图元线框模型绘制
void YMGRE_TrangleObject_Wires(GRE_Object4d myTrangleObj, GRE_Camera4d mycam);
void YMGRE_TrangleObject_WiresTo(GRE_Object4d myTrangleObj, GRE_Vertex4d points, uint8* polygonHide, GRE_Camera4d mycam);//使用外部相机工作区绘制线框

//灯光光栅化绘制
void YMGRE_Light_Primitive_Rasterization(GRE_Light4d mylight, GRE_Camera4d mycam, uint8 showLightSize);


#endif // !YMGRE_RASTERIZATION_H

