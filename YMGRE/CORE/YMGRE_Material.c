#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMGRE_List.h"
#include <string.h>

GRE_Material YMGRE_Material_Find(GRE_List materialList, char* materialName)
{
	if (materialList == NULL || materialName == NULL)
		return NULL;
	uint32 nameLen = (uint32)strlen(materialName) + 1;
	for (GRE_ListNode node = materialList->listhead; node != NULL; node = node->next)
	{
		GRE_Material material = node->data;
		if (material != NULL && material->nameLen == nameLen &&
			YMGRE_Memcmp(material->name, materialName, nameLen) == 0)
			return material;
	}
	return NULL;
}

GRE_MaterialAdvanced YMGRE_Material_EnsureAdvanced(GRE_Material material)
{
 if(!material)return NULL;
 if(!material->advanced){
  material->advanced=GRE_malloc0(sizeof(*material->advanced));
  if(material->advanced){
   memset(material->advanced,0,sizeof(*material->advanced));
#if YMGRE_ENABLE_PBR
   material->advanced->pbrIOR=1.45f;material->advanced->pbrNormalStrength=1;
#endif
  }
 }
 return material->advanced;
}
#if YMGRE_ENABLE_TRANSPARENCY
void YMGRE_Material_ClearOpacity(GRE_Material material)
{
 if(!material||!material->advanced)return;
 GRE_MaterialAdvanced a=material->advanced;
#if YMGRE_ENABLE_OPACITY_MIPMAP
 for(int i=1;i<a->opacityMipCount;i++)GRE_ImageBuff_Free(a->opacityMipPixels[i]);
 memset(a->opacityMipPixels,0,sizeof(a->opacityMipPixels));
 memset(a->opacityMipWidth,0,sizeof(a->opacityMipWidth));
 memset(a->opacityMipHeight,0,sizeof(a->opacityMipHeight));
 a->opacityMipCount=0;a->opacityUseMip=0;
#endif
 GRE_ImageBuff_Free(a->opacityPixel);a->opacityPixel=NULL;a->opacityWidth=a->opacityHeight=0;
}
int YMGRE_Material_SetOpacity(GRE_Material material,const uint8* pixels,uint16 w,uint16 h,int mipmaps)
{
 if(!material||!pixels||!w||!h)return 0;
 uint8* copy=GRE_ImageBuff_Malloc((size_t)w*h);if(!copy)return 0;
 memcpy(copy,pixels,(size_t)w*h);
 GRE_MaterialAdvanced a=YMGRE_Material_EnsureAdvanced(material);
 if(!a){GRE_ImageBuff_Free(copy);return 0;}
 YMGRE_Material_ClearOpacity(material);a->opacityPixel=copy;a->opacityWidth=w;a->opacityHeight=h;
#if YMGRE_ENABLE_OPACITY_MIPMAP
 a->opacityMipPixels[0]=copy;a->opacityMipWidth[0]=w;a->opacityMipHeight[0]=h;a->opacityMipCount=1;
 a->opacityUseMip=!!mipmaps;
 while(mipmaps&&(w>1||h>1)&&a->opacityMipCount<16){
  uint16 nw=w>1?(w+1)/2:1,nh=h>1?(h+1)/2:1;
  uint8* next=GRE_ImageBuff_Malloc((size_t)nw*nh);
  if(!next){YMGRE_Material_ClearOpacity(material);return 0;}
  for(unsigned y=0;y<nh;y++)for(unsigned x=0;x<nw;x++){
   unsigned sum=0,n=0;
   for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)if(2*x+dx<w&&2*y+dy<h){sum+=copy[(size_t)(2*y+dy)*w+2*x+dx];n++;}
   next[(size_t)y*nw+x]=(uint8)((sum+n/2)/n);
  }
  int level=a->opacityMipCount++;a->opacityMipPixels[level]=next;a->opacityMipWidth[level]=nw;a->opacityMipHeight[level]=nh;
  copy=next;w=nw;h=nh;
 }
#else
 (void)mipmaps;
#endif
 return 1;
}
#endif
