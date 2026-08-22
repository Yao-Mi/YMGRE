#ifndef YMGRE_BASICMESH_GENER_H
#define YMGRE_BASICMESH_GENER_H

#include "../OPOBJ/YMGRE_OBJ.h"

GRE_Object4d YMGRE_MeshGener_RectPlane(float32 width, float32 depth, uint16 rows, uint16 columns,
	GRErgb24 color, char* name, char* materiaName);//创建三角网格矩形平面
GRE_Object4d YMGRE_MeshGener_Cube(float32 side, GRErgb24 color, char* name, char* materiaName);//创建三角网格立方体
GRE_Object4d YMGRE_MeshGener_Box(float32 width, float32 height, float32 depth,
	GRErgb24 color, char* name, char* materiaName);//创建三角网格长方体
GRE_Object4d YMGRE_MeshGener_Cylinder(float32 radius, float32 height, uint16 segments,
	GRErgb24 color, char* name, char* materiaName);//创建三角网格圆柱
GRE_Object4d YMGRE_MeshGener_Cone(float32 radius, float32 height, uint16 segments,
	GRErgb24 color, char* name, char* materiaName);//创建三角网格圆锥
GRE_Object4d YMGRE_MeshGener_Sphere(float32 radius, uint16 latitude, uint16 longitude,
	GRErgb24 color, char* name, char* materiaName);//创建三角网格球体
GRE_Object4d YMGRE_MeshGener_Torus(float32 majorRadius, float32 tubeRadius,
	uint16 majorSegments, uint16 tubeSegments, GRErgb24 color,
	char* name, char* materiaName);//创建三角网格圆环
GRE_Object4d YMGRE_MeshGener_Capsule(float32 radius, float32 cylinderHeight,
	uint16 hemisphereSegments, uint16 longitude, GRErgb24 color,
	char* name, char* materiaName);//创建三角网格胶囊体
GRE_Object4d YMGRE_MeshGener_Tetrahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName);//创建正四面体
GRE_Object4d YMGRE_MeshGener_Octahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName);//创建正八面体
GRE_Object4d YMGRE_MeshGener_Dodecahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName);//创建正十二面体三角网格
GRE_Object4d YMGRE_MeshGener_Icosahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName);//创建正二十面体

#endif // !YMGRE_BASICMESH_GENER_H
