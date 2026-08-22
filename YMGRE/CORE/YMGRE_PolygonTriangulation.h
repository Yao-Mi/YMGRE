#ifndef YMGRE_POLYGON_TRIANGULATION_H
#define YMGRE_POLYGON_TRIANGULATION_H

#include "../OPOBJ/YMGRE_OBJ.h"

// 简单二维多边形三角剖分，使用耳切法。
// 顶点使用 x/y 分量，输出为连续的三角形顶点索引。
// 输入非法、退化或输出容量不足时返回0。
uint16 YMGRE_Polygon_Triangulate(const gre_fvector4d* vertices, uint16 vertexNum,
	uint16* triangleIndices, uint16 triangleCapacity);

#endif
