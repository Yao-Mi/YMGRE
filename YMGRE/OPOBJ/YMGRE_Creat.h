#ifndef _YM_CREAT_H
#define _YM_CREAT_H
#include"../CONFIG/YMGRE_PubDefine.h"
#include"./YMGRE_OBJ.h"

GRE_Fvector2d YMGRE_Creat_VectorF2d(void);
GRE_Fvector3d YMGRE_Creat_VectorF3d(void);
GRE_Fvector4d YMGRE_Creat_VectorF4d(void);

GRE_FMat4x4 YMGRE_Creat_FMAT4x4(void);

GRE_Light4d YMGRE_Creat_Light(int16 id, GRE_LightType type, GRErgb24 color, float32 strength);
GRE_Camera4d YMGRE_Creat_Camera(int16 id, uint16 imgW, uint16 imgH, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy);
GRE_Camera4d YMGRE_Creat_CameraFromTarget(int16 id, GRE_RenderTarget target, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy);//使用外部或共享目标创建相机
GRE_Object4d YMGRE_Creat_Object(int pointNum, int polygonNum, char* name, char* materiaName);
int YMGRE_Object_EnableVertexAttributes(GRE_Object4d object);
// Call only when every polygon index array is owned by the object.
int YMGRE_Object_CompactTopology(GRE_Object4d object);
int YMGRE_Object_GenerateVertexAttributes(GRE_Object4d object);//由三角网格和 UV 生成法线/切线
GRE_Material YMGRE_Creat_Material(char* name);
GRE_Terrain YMGRE_Creat_Terrain(uint16 x_width, uint16 z_width, float32 blockSize, char* name);

#endif
