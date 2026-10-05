#include "YMCS_File_IO.h"
#include "../CORE/YMGRE_Material.h"
#include "image_decode.h"
#include <stddef.h>

/* The decoder returns packed R,G,B bytes directly into the engine image heap. */
typedef char ImageRGBLayout[sizeof(GRErgb24)==3?1:-1];
static const ImageDecodeAllocator imageAllocator={GRE_ImageBuff_Malloc,GRE_ImageBuff_Free};
int YMGRE_Image_LoadRGB(const char* path,GRErgb24** pixels,uint16* width,uint16* height)
{
 uint8_t* data=NULL;
 if(!pixels||!image_decode_load(path,3,&imageAllocator,&data,width,height))return 0;
 /* Engine colors use bit-fields; assign channels without assuming byte order. */
 GRErgb24* rgb=(GRErgb24*)data;
 for(size_t i=0;i<(size_t)*width * *height;i++){
  uint8_t r=data[3*i],g=data[3*i+1],b=data[3*i+2];
  rgb[i]=(GRErgb24){r,g,b};
 }
 *pixels=rgb;return 1;
}
int YMGRE_Image_LoadGray(const char* path,uint8** pixels,uint16* width,uint16* height)
{
 return image_decode_load(path,1,&imageAllocator,pixels,width,height);
}
#if YMGRE_ENABLE_TRANSPARENCY
int YMGRE_Material_LoadOpacity(GRE_Material material,const char* path,int mipmaps)
{
 if(!material)return 0;uint8* pixels=NULL;uint16 w=0,h=0;
 if(!YMGRE_Image_LoadGray(path,&pixels,&w,&h))return 0;
 int ok=YMGRE_Material_SetOpacity(material,pixels,w,h,mipmaps);GRE_ImageBuff_Free(pixels);return ok;
}
#endif
