#ifndef YMGRE_TRIANGLE_RASTER_H
#define YMGRE_TRIANGLE_RASTER_H

#include "../OPOBJ/YMGRE_OBJ.h"

//内部三角片元填充入口，线框由物体完成全部填充后统一绘制
void YMGRE_TriangleRaster_Fill(GRE_Vertex4d vertexList, GRE_Polygon4d polygon,
	GRErgb24 planeColor, GRE_Material material, GRE_Camera4d camera);

#endif // !YMGRE_TRIANGLE_RASTER_H
