#ifndef _YM_CREAT_H
#define _YM_CREAT_H
#include"../CONFIG/YMGRE_PubDefine.h"
#include"./YMGRE_OBJ.h"

GRE_Fvector2d YMGRE_Creat_VectorF2d();
GRE_Fvector3d YMGRE_Creat_VectorF3d();
GRE_Fvector4d YMGRE_Creat_VectorF4d();

GRE_FMat4x4 YMGRE_Creat_FMAT4x4();

GRE_Light4d YMGRE_Creat_Light(int16 id, GRE_LightType type, GRErgb24 color, float32 strength);
GRE_Camera4d YMGRE_Creat_Camera(int16 id, uint16 imgW, uint16 imgH, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy);
GRE_Object4d YMGRE_Creat_Object(int pointNum, int polygonNum, char* name, char* materiaName);
GRE_Material YMGRE_Creat_Material(char* name);
GRE_Terrain YMGRE_Creat_Terrain(uint16 x_width, uint16 z_width, float32 blockSize, char* name);

#endif
