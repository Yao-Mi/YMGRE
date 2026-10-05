#include "YMCS_File_IO.h"
#include "YMGRE_Material.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const uint8 rgbExpected[]={0,20,250,40,200,90,255,120,33,66,70,80,90,100,110,240,230,220};
static const uint8 grayExpected[]={0,1,63,128,200,255};
static void checkImage(const char* path,const uint8* expected,int enabled)
{
 GRErgb24 sentinel={0},*rgb=&sentinel;uint8 gs=0,*gray=&gs;uint16 w=123,h=456;
 int ok=YMGRE_Image_LoadRGB(path,&rgb,&w,&h);assert(ok==enabled);
 if(!enabled){assert(rgb==&sentinel&&w==123&&h==456);assert(!YMGRE_Image_LoadGray(path,&gray,&w,&h));assert(gray==&gs&&w==123&&h==456);return;}
 assert(w==3&&h==2&&rgb!=&sentinel);
 assert(YMGRE_Image_LoadGray(path,&gray,&w,&h)&&w==3&&h==2);
 for(int i=0;i<6;i++){
  if(expected){assert(rgb[i].R==expected[3*i]);assert(rgb[i].G==expected[3*i+1]);assert(rgb[i].B==expected[3*i+2]);}
  assert(gray[i]==rgb[i].R);
 }
 GRE_ImageBuff_Free(rgb);GRE_ImageBuff_Free(gray);
}
static void badFile(const char* path)
{
 GRErgb24 sentinel={0},*rgb=&sentinel;uint8 gs=0,*gray=&gs;uint16 w=7,h=9;
 assert(!YMGRE_Image_LoadRGB(path,&rgb,&w,&h));assert(rgb==&sentinel&&w==7&&h==9);
 assert(!YMGRE_Image_LoadGray(path,&gray,&w,&h));assert(gray==&gs&&w==7&&h==9);
}
int main(int argc,char** argv)
{
 assert(argc==3);char path[4096],temporary[4096];uint8 grayscaleRGB[18];
 for(int i=0;i<6;i++)for(int k=0;k<3;k++)grayscaleRGB[3*i+k]=grayExpected[i];
 struct {const char* name;const uint8* expected;int enabled;} cases[]={
  {"rgb.bmp",rgbExpected,1},{"gray.bmp",grayscaleRGB,1},
  {"rgb.png",rgbExpected,YMGRE_ENABLE_PNG},{"gamma.png",rgbExpected,YMGRE_ENABLE_PNG},
  {"rgba.png",rgbExpected,YMGRE_ENABLE_PNG},{"palette.png",rgbExpected,YMGRE_ENABLE_PNG},
  {"interlaced.png",rgbExpected,YMGRE_ENABLE_PNG},{"gray.png",grayscaleRGB,YMGRE_ENABLE_PNG},
  {"gray16.png",grayscaleRGB,YMGRE_ENABLE_PNG},
  {"rgb.jpg",NULL,YMGRE_ENABLE_JPEG},{"progressive.jpg",NULL,YMGRE_ENABLE_JPEG},{"gray.jpg",NULL,YMGRE_ENABLE_JPEG}
 };
 for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
  snprintf(path,sizeof(path),"%s/%s",argv[1],cases[i].name);checkImage(path,cases[i].expected,cases[i].enabled);
  if(!cases[i].enabled)continue;
  /* Every truncated prefix must fail without terminating the caller or changing outputs. */
  FILE* f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long length=ftell(f);assert(length>0&&length<4096);rewind(f);
  uint8 data[4096];assert(fread(data,1,length,f)==(size_t)length);fclose(f);
  snprintf(temporary,sizeof(temporary),"%s/truncated-texture",argv[2]);
  for(long cut=0;cut<length;cut++){
   f=fopen(temporary,"wb");assert(f);assert(fwrite(data,1,cut,f)==(size_t)cut);fclose(f);badFile(temporary);
  }
  remove(temporary);
 }
#if YMGRE_ENABLE_JPEG
 snprintf(path,sizeof(path),"%s/jpeg_rgb_expected.bin",argv[1]);FILE* f=fopen(path,"rb");assert(f);uint8 expected[18];assert(fread(expected,1,18,f)==18);fclose(f);
 snprintf(path,sizeof(path),"%s/rgb.jpg",argv[1]);checkImage(path,expected,1);
#endif
 snprintf(path,sizeof(path),"%s/cmyk.jpg",argv[1]);badFile(path);
 snprintf(path,sizeof(path),"%s/oversized.png",argv[1]);badFile(path);
 snprintf(path,sizeof(path),"%s/absent-image",argv[1]);badFile(path);
#if YMGRE_ENABLE_TRANSPARENCY
 GRE_Material m=YMGRE_Creat_Material("opacity");
 snprintf(path,sizeof(path),"%s/gray.bmp",argv[1]);assert(YMGRE_Material_LoadOpacity(m,path,1));
 assert(!memcmp(m->advanced->opacityPixel,grayExpected,6));uint8* before=m->advanced->opacityPixel;
 assert(!YMGRE_Material_LoadOpacity(m,"/no-such-image",1)&&before==m->advanced->opacityPixel);
#if YMGRE_ENABLE_PNG
 snprintf(path,sizeof(path),"%s/gray.png",argv[1]);assert(YMGRE_Material_LoadOpacity(m,path,1));assert(!memcmp(m->advanced->opacityPixel,grayExpected,6));
#endif
 YMGRE_Free_Material(m);
#endif
 puts("PASS: BMP/PNG/JPEG, raw sample values, palette/alpha/interlace/16-bit, grayscale, malformed input and feature trimming");return 0;
}
