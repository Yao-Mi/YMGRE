#ifndef YMGRE_TRIANGLE_RASTER_H
#define YMGRE_TRIANGLE_RASTER_H

#include "../OPOBJ/YMGRE_OBJ.h"
#include "YMGRE_List.h"

//内部三角片元填充入口，线框由物体完成全部填充后统一绘制
void YMGRE_TriangleRaster_Fill(GRE_Vertex4d vertexList, GRE_Polygon4d polygon,
	GRErgb24 planeColor, GRE_Material material, GRE_Camera4d camera);
void YMGRE_TriangleRaster_FillVertexColor_wN(GRE_Vertex4d_wN vertexList,
	GRE_Polygon4d polygon,GRE_Camera4d camera);
void YMGRE_TriangleRaster_ComputeVertexLighting_wN(GRE_Vertex4d_wN vertices,
	uint16 vertexCount, GRE_Polygon4d polygon, GRE_Material material,
	GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs);
void YMGRE_TriangleRaster_FillVertexLit_wN(GRE_Vertex4d_wN vertexList,
	GRE_Polygon4d polygon, GRE_Material material, GRE_Camera4d camera);
void YMGRE_TriangleRaster_Fill_wN(GRE_Vertex4d_wN vertexList, GRE_Polygon4d polygon,
	GRE_Material material, GRE_List lights, gre_fvector4d* lightPos, GRE_FMat4x4 worldToCamera,
	float32 mirrorKs, GRE_Camera4d camera);

#endif // !YMGRE_TRIANGLE_RASTER_H
