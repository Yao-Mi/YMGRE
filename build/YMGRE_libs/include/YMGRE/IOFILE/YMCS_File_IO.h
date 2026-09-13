#ifndef _YMCS_FILE_IO_H
#define _YMCS_FILE_IO_H

#include "../CONFIG/YMGRE_Mem.h"
#include "../CORE/YMGRE_ScenceManager.h"


GRE_Object4d YMGRE_LoadOgreMeshAndMaterial(GRE_Scence mysc, const char* meshpath);//加载网格模型及材质
GRE_Terrain YMGRE_Load_SceneTerrainAndMaterial(GRE_Scence mysc, const char* mapPath);//加载地形及材质
void YMGRE_ParseMaterialScript(GRE_Scence mysc, const char* scriptName);//加载材质脚本及其引用的贴图

//加载BMP文件
void YMGRE_Bmp_File_LoadTo_Image(const char* file_path, GRErgb24** image, uint16* col, uint16* row);
//将BMP保存文件
void YMGRE_Image_LoadTo_Bmp_File(const char* file_path, GRErgb24* image, uint32 width, uint32 height);


#endif //_YMCS_FILE_IO_H
