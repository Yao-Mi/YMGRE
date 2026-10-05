#include "../CORE/YMGRE_Material.h"
#include "../CONFIG/YMGRE_Mem.h"
#include <stdint.h>
#include <stdio.h>
#if YMGRE_ENABLE_TRANSPARENCY
static uint32 le32(const unsigned char* p){return (uint32)p[0]|((uint32)p[1]<<8)|((uint32)p[2]<<16)|((uint32)p[3]<<24);}
static unsigned le16(const unsigned char* p){return p[0]|((unsigned)p[1]<<8);}
int YMGRE_Material_LoadOpacityBMP(GRE_Material material,const char* path,int mipmaps)
{
 if(!material||!path)return 0;
 FILE* f=fopen(path,"rb");if(!f)return 0;
 unsigned char head[54],palette[256],entry[4];uint8* pixels=NULL;uint8* row=NULL;int ok=0;
 if(fread(head,1,54,f)!=54||head[0]!='B'||head[1]!='M')goto done;
 uint32 offset=le32(head+10),dib=le32(head+14),compression=le32(head+30);
 int32_t w=(int32_t)le32(head+18),sh=(int32_t)le32(head+22);
 unsigned bpp=le16(head+28);if(dib<40||w<=0||w>65535||sh==0||sh==INT32_MIN||le16(head+26)!=1||compression||(bpp!=8&&bpp!=24&&bpp!=32))goto done;
 unsigned h=sh<0?(unsigned)-sh:(unsigned)sh;if(h>65535)goto done;
 size_t stride=(((size_t)w*bpp+31)/32)*4;
 if(fseek(f,0,SEEK_END))goto done;long length=ftell(f);
 if(length<0||offset<(uint64_t)14+dib||(uint64_t)offset+(uint64_t)stride*h>(uint64_t)length)goto done;
 if(bpp==8){
  uint32 colors=le32(head+46);if(!colors)colors=256;
  if(colors>256||(uint64_t)14+dib+4*colors>offset||fseek(f,14L+dib,SEEK_SET))goto done;
  for(uint32 i=0;i<colors;i++){if(fread(entry,1,4,f)!=4)goto done;palette[i]=entry[2];}
  for(uint32 i=colors;i<256;i++)palette[i]=0;
 }
 pixels=GRE_ImageBuff_Malloc((size_t)w*h);row=GRE_malloc1(stride);if(!pixels||!row||fseek(f,offset,SEEK_SET))goto done;
 for(unsigned y=0;y<h;y++){
  if(fread(row,1,stride,f)!=stride)goto done;
  unsigned dstY=sh<0?y:h-1-y;
  for(unsigned x=0;x<(unsigned)w;x++)pixels[(size_t)dstY*w+x]=bpp==8?palette[row[x]]:row[x*(bpp/8)+2];
 }
 ok=YMGRE_Material_SetOpacity(material,pixels,(uint16)w,(uint16)h,mipmaps);
 done:GRE_ImageBuff_Free(pixels);GRE_free1(row);fclose(f);return ok;
}
#endif
