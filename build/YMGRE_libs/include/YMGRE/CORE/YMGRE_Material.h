#ifndef _YMGRE_MATERIAL_H
#define _YMGRE_MATERIAL_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_PubDefine.h"

static inline GRErgb24 getPixel(GRE_Material mat, float u, float v)
{
	static GRErgb24 color = { .R = 128,.G = 128,.B = 128 };
	if (mat && mat->valid && mat->pixel && mat->width > 0 && mat->height > 0)
	{
		// 这是一个更精确的方法, 但是效率低一点
		// 纹理u,v在[0,1]区间
		int x = YMGRE_Fabs(u - (int)u) * mat->width;
		int y = YMGRE_Fabs(v - (int)v) * mat->height;
		return mat->pixel[y * mat->width + x];
	}
	else
		return color;
}




/* Copies input pixels. Replacing maps releases the previous owned map/mips. */
GRE_MaterialAdvanced YMGRE_Material_EnsureAdvanced(GRE_Material material);
/* Build optional RGB mip levels from material->pixel after loading it.
   Level 0 remains caller-owned by the material; rebuilding is transactional.
   Call ClearColorMips before replacing/mutating the base texture, then rebuild. */
int YMGRE_Material_BuildColorMips(GRE_Material material);
void YMGRE_Material_ClearColorMips(GRE_Material material);
#if YMGRE_ENABLE_TRANSPARENCY
int YMGRE_Material_SetOpacity(GRE_Material material, const uint8* pixels, uint16 width, uint16 height, int mipmaps);
int YMGRE_Material_LoadOpacityBMP(GRE_Material material,const char* path,int mipmaps);
int YMGRE_Material_LoadOpacity(GRE_Material material,const char* path,int mipmaps);
void YMGRE_Material_ClearOpacity(GRE_Material material);
#endif
#endif // !_YMGRE_MATERIAL_H

