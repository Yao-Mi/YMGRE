#ifndef _YMGRE_MATERIAL_H
#define _YMGRE_MATERIAL_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_PubDefine.h"

static inline GRErgb24 getPixel(GRE_Material mat, float u, float v)
{
	static GRErgb24 color = { .R = 128,.G = 128,.B = 128 };
	if (mat && mat->valid)
	{
		// 这是一个更精确的方法, 但是效率低一点
		// 纹理u,v在[0,1]区间
		int x = YMGRE_Abs(u - (int)u) * mat->width;
		int y = YMGRE_Abs(v - (int)v) * mat->height;
		return mat->pixel[y * mat->width + x];
	}
	else
		return color;
}




#endif // !_YMGRE_MATERIAL_H

