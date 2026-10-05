#include "image_decode.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#if IMAGE_DECODE_ENABLE_PNG
#include <png.h>
#endif
#if IMAGE_DECODE_ENABLE_JPEG
#include <jpeglib.h>
#endif

/* Bound decoded allocations independently of compressed file size. */
#ifndef IMAGE_DECODE_MAX_BYTES
#define IMAGE_DECODE_MAX_BYTES ((size_t)512*1024*1024)
#endif
typedef struct { uint8_t R,G,B; } RGB;
static int validSize(uint32_t w,uint32_t h,int gray)
{
 size_t stride=gray?1:3;
 return w&&h&&w<=65535&&h<=65535&&(size_t)w<=SIZE_MAX/h/stride&&
  (size_t)w*h*stride<=IMAGE_DECODE_MAX_BYTES;
}
static uint32_t read32(const uint8_t* p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static unsigned read16(const uint8_t* p){return p[0]|((unsigned)p[1]<<8);}
static void storeRow(void* dest,size_t offset,const uint8_t* rgb,unsigned width,int gray)
{
 if(gray)for(unsigned x=0;x<width;x++)((uint8_t*)dest)[offset+x]=rgb[3*x];
 else memcpy((uint8_t*)dest+offset*3,rgb,(size_t)width*3);
}
static void* loadBMP(FILE* f,int gray,uint16_t* width,uint16_t* height,const ImageDecodeAllocator* a)
{
 uint8_t head[54],entry[4],*row=NULL;RGB palette[256];void* pixels=NULL;int ok=0;
 if(fread(head,1,54,f)!=54||head[0]!='B'||head[1]!='M')return NULL;
 uint32_t offset=read32(head+10),dib=read32(head+14);
 int32_t w=(int32_t)read32(head+18),sh=(int32_t)read32(head+22);
 unsigned bpp=read16(head+28);
 if(dib<40||w<=0||!sh||sh==INT32_MIN||read16(head+26)!=1||read32(head+30)||(bpp!=8&&bpp!=24&&bpp!=32))return NULL;
 uint32_t h=sh<0?(uint32_t)-sh:(uint32_t)sh;
 if(!validSize((uint32_t)w,h,gray))return NULL;
 size_t stride=(((size_t)w*bpp+31)/32)*4;
 if(fseek(f,0,SEEK_END))return NULL;long length=ftell(f);
 if(length<0||offset<(uint64_t)14+dib||(uint64_t)offset+stride*(uint64_t)h>(uint64_t)length)return NULL;
 if(bpp==8){
  uint32_t colors=read32(head+46);if(!colors)colors=256;
  if(colors>256||(uint64_t)14+dib+colors*4>offset||fseek(f,14L+dib,SEEK_SET))return NULL;
  for(unsigned i=0;i<colors;i++){if(fread(entry,1,4,f)!=4)return NULL;palette[i]=(RGB){entry[2],entry[1],entry[0]};}
  for(unsigned i=colors;i<256;i++)palette[i]=(RGB){0,0,0};
 }
 pixels=a->allocate((size_t)w*h*(gray?1:3));row=a->allocate(stride);
 if(!pixels||!row||fseek(f,offset,SEEK_SET))goto done;
 for(unsigned y=0;y<h;y++){
  if(fread(row,1,stride,f)!=stride)goto done;
  size_t base=(size_t)(sh<0?y:h-1-y)*w;
  for(unsigned x=0;x<(unsigned)w;x++){
   RGB c=bpp==8?palette[row[x]]:(RGB){row[x*(bpp/8)+2],row[x*(bpp/8)+1],row[x*(bpp/8)]};
   if(gray)((uint8_t*)pixels)[base+x]=c.R;else {uint8_t* d=(uint8_t*)pixels+3*(base+x);d[0]=c.R;d[1]=c.G;d[2]=c.B;}
  }
 }
 *width=(uint16_t)w;*height=(uint16_t)h;ok=1;
done:
 a->release(row);if(!ok){a->release(pixels);pixels=NULL;}return pixels;
}

#if IMAGE_DECODE_ENABLE_PNG
typedef struct {png_structp png;png_infop info;void* pixels;uint8_t* row;int ok;} PNGState;
static void pngFail(png_structp png,png_const_charp message){(void)message;longjmp(png_jmpbuf(png),1);}
static void pngWarn(png_structp png,png_const_charp message){(void)png;(void)message;}
static void* loadPNG(FILE* f,int gray,uint16_t* width,uint16_t* height,const ImageDecodeAllocator* a)
{
 PNGState* s=a->allocate(sizeof(*s));if(!s)return NULL;memset(s,0,sizeof(*s));
 s->png=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,pngFail,pngWarn);
 if(!s->png)goto done;
 if(setjmp(png_jmpbuf(s->png)))goto done;
 s->info=png_create_info_struct(s->png);if(!s->info)goto done;
 png_set_user_limits(s->png,65535,65535);
 png_set_chunk_malloc_max(s->png,8*1024*1024);
 png_init_io(s->png,f);png_read_info(s->png,s->info);
 png_uint_32 w=png_get_image_width(s->png,s->info),h=png_get_image_height(s->png,s->info);
 if(!validSize(w,h,gray))goto done;
 int type=png_get_color_type(s->png,s->info),bits=png_get_bit_depth(s->png,s->info);
 if(bits==16)png_set_strip_16(s->png);
 if(type==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(s->png);
 if(type==PNG_COLOR_TYPE_GRAY&&bits<8)png_set_expand_gray_1_2_4_to_8(s->png);
 if(png_get_valid(s->png,s->info,PNG_INFO_tRNS))png_set_tRNS_to_alpha(s->png);
 if(type==PNG_COLOR_TYPE_GRAY||type==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(s->png);
 png_set_strip_alpha(s->png);
 /* Deliberately do not apply gAMA/sRGB/iCCP transforms to texture data. */
 int passes=png_set_interlace_handling(s->png);png_read_update_info(s->png,s->info);
 if(png_get_channels(s->png,s->info)!=3||png_get_rowbytes(s->png,s->info)!=(size_t)w*3)goto done;
 /* Interlacing needs retained RGB rows across passes. Non-interlaced grayscale
    (our opacity assets) uses just one temporary RGB scanline. */
 size_t bytes=(size_t)w*h*(gray?1:3);
 s->pixels=a->allocate(bytes);if(!s->pixels)goto done;
 if(passes>1){
  if(!validSize(w,h,0))goto done;
  s->row=a->allocate((size_t)w*h*3);if(!s->row)goto done;memset(s->row,0,(size_t)w*h*3);
  for(int pass=0;pass<passes;pass++)for(unsigned y=0;y<h;y++)png_read_row(s->png,s->row+(size_t)y*w*3,NULL);
  for(unsigned y=0;y<h;y++)storeRow(s->pixels,(size_t)y*w,s->row+(size_t)y*w*3,w,gray);
 }else{
  s->row=a->allocate((size_t)w*3);if(!s->row)goto done;
  for(unsigned y=0;y<h;y++){png_read_row(s->png,s->row,NULL);storeRow(s->pixels,(size_t)y*w,s->row,w,gray);}
 }
 png_read_end(s->png,s->info);*width=(uint16_t)w;*height=(uint16_t)h;s->ok=1;
done:
 if(s->png)png_destroy_read_struct(&s->png,&s->info,NULL);
 a->release(s->row);void* pixels=s->pixels;if(!s->ok){a->release(pixels);pixels=NULL;}
 a->release(s);return pixels;
}
#endif

#if IMAGE_DECODE_ENABLE_JPEG
typedef struct {struct jpeg_error_mgr base;jmp_buf jump;} JPEGError;
typedef struct {struct jpeg_decompress_struct jpeg;JPEGError error;void* pixels;uint8_t* row;int created,ok;} JPEGState;
static void jpegFail(j_common_ptr jpeg){JPEGError* e=(JPEGError*)jpeg->err;longjmp(e->jump,1);}
static void jpegMessage(j_common_ptr jpeg,int level){if(level<0)jpegFail(jpeg);}
static void* loadJPEG(FILE* f,int gray,uint16_t* width,uint16_t* height,const ImageDecodeAllocator* a)
{
 JPEGState* s=a->allocate(sizeof(*s));if(!s)return NULL;memset(s,0,sizeof(*s));
 s->jpeg.err=jpeg_std_error(&s->error.base);s->error.base.error_exit=jpegFail;s->error.base.emit_message=jpegMessage;
 if(setjmp(s->error.jump))goto done;
 /* jpeg_destroy_decompress is valid even if creation reports an error. */
 s->created=1;jpeg_create_decompress(&s->jpeg);jpeg_stdio_src(&s->jpeg,f);
 if(jpeg_read_header(&s->jpeg,TRUE)!=JPEG_HEADER_OK||!validSize(s->jpeg.image_width,s->jpeg.image_height,gray))goto done;
 if(s->jpeg.jpeg_color_space!=JCS_GRAYSCALE&&s->jpeg.jpeg_color_space!=JCS_RGB&&s->jpeg.jpeg_color_space!=JCS_YCbCr)goto done;
 s->jpeg.out_color_space=JCS_RGB;
 if(!jpeg_start_decompress(&s->jpeg)||s->jpeg.output_components!=3)goto done;
 unsigned w=s->jpeg.output_width,h=s->jpeg.output_height;
 if(!validSize(w,h,gray))goto done;
 s->pixels=a->allocate((size_t)w*h*(gray?1:3));s->row=a->allocate((size_t)w*3);
 if(!s->pixels||!s->row)goto done;
 while(s->jpeg.output_scanline<h){
  unsigned y=s->jpeg.output_scanline;JSAMPROW row=s->row;
  if(jpeg_read_scanlines(&s->jpeg,&row,1)!=1)goto done;
  storeRow(s->pixels,(size_t)y*w,s->row,w,gray);
 }
 if(!jpeg_finish_decompress(&s->jpeg))goto done;
 *width=(uint16_t)w;*height=(uint16_t)h;s->ok=1;
done:
 if(s->created)jpeg_destroy_decompress(&s->jpeg);
 a->release(s->row);void* pixels=s->pixels;if(!s->ok){a->release(pixels);pixels=NULL;}
 a->release(s);return pixels;
}
#endif

static void* loadImage(const char* path,int gray,uint16_t* width,uint16_t* height,const ImageDecodeAllocator* a)
{
 if(!path||!width||!height)return NULL;
 FILE* f=fopen(path,"rb");if(!f)return NULL;uint8_t signature[8];void* result=NULL;
 size_t n=fread(signature,1,sizeof(signature),f);rewind(f);
 if(n>=2&&signature[0]=='B'&&signature[1]=='M')result=loadBMP(f,gray,width,height,a);
#if IMAGE_DECODE_ENABLE_PNG
 else if(n==8&&!png_sig_cmp(signature,0,8))result=loadPNG(f,gray,width,height,a);
#endif
#if IMAGE_DECODE_ENABLE_JPEG
 else if(n>=2&&signature[0]==0xff&&signature[1]==0xd8)result=loadJPEG(f,gray,width,height,a);
#endif
 fclose(f);return result;
}
int image_decode_load(const char* path,int channels,const ImageDecodeAllocator* allocator,
                      uint8_t** pixels,uint16_t* width,uint16_t* height)
{
 const ImageDecodeAllocator standard={malloc,free};
 const ImageDecodeAllocator* a=allocator?allocator:&standard;
 if(!pixels||!a->allocate||!a->release||(channels!=1&&channels!=3))return 0;
 void* data=loadImage(path,channels==1,width,height,a);
 if(!data)return 0;
 *pixels=data;return 1;
}
