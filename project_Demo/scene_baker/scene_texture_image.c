#include "scene_texture_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>
#include <jpeglib.h>
#include <SDL.h>
void SceneTextureImage_Free(SceneTextureImage* image){if(image){free(image->pixels);memset(image,0,sizeof(*image));}}
static int dimensions(unsigned w,unsigned h){return w&&h&&w<=4096&&h<=4096;}
static GRErgb24 opaque(const unsigned char* rgba)
{unsigned a=rgba[3];return (GRErgb24){(rgba[0]*a+255*(255-a)+127)/255,(rgba[1]*a+255*(255-a)+127)/255,(rgba[2]*a+255*(255-a)+127)/255};}
static int readPng(const char* path,SceneTextureImage* out)
{
    png_image png={.version=PNG_IMAGE_VERSION};unsigned char* rgba=NULL;int ok=0;
    if(!png_image_begin_read_from_file(&png,path)||!dimensions(png.width,png.height))goto done;
    png.format=PNG_FORMAT_RGBA;rgba=malloc(PNG_IMAGE_SIZE(png));if(!rgba||!png_image_finish_read(&png,NULL,rgba,0,NULL))goto done;
    out->pixels=malloc((size_t)png.width*png.height*sizeof(*out->pixels));if(!out->pixels)goto done;
    out->width=png.width;out->height=png.height;
    for(size_t i=0;i<(size_t)out->width*out->height;i++)out->pixels[i]=opaque(rgba+i*4);ok=1;
done:free(rgba);png_image_free(&png);return ok;
}
typedef struct {struct jpeg_error_mgr base;jmp_buf jump;} JpegError;
static void jpegError(j_common_ptr c){JpegError* error=(JpegError*)c->err;longjmp(error->jump,1);}
/* Do not silently turn truncated input into a partially decoded texture. */
static void jpegMessage(j_common_ptr c,int level){if(level<0)jpegError(c);}
static int readJpeg(const char* path,SceneTextureImage* out)
{
    FILE* file=fopen(path,"rb");if(!file)return 0;
    /* Heap state remains defined across libjpeg's longjmp error recovery. */
    struct jpeg_decompress_struct* jpg=calloc(1,sizeof(*jpg));JpegError* error=calloc(1,sizeof(*error));
    if(!jpg||!error){free(jpg);free(error);fclose(file);return 0;}
    jpg->err=jpeg_std_error(&error->base);error->base.error_exit=jpegError;error->base.emit_message=jpegMessage;int ok=0;
    if(setjmp(error->jump))goto done;
    jpeg_create_decompress(jpg);jpeg_stdio_src(jpg,file);jpeg_read_header(jpg,TRUE);
    if(!dimensions(jpg->image_width,jpg->image_height))goto done;
    jpg->out_color_space=JCS_RGB;jpeg_start_decompress(jpg);
    out->width=jpg->output_width;out->height=jpg->output_height;
    out->pixels=malloc((size_t)out->width*out->height*sizeof(*out->pixels));if(!out->pixels)goto done;
    JSAMPARRAY row=(*jpg->mem->alloc_sarray)((j_common_ptr)jpg,JPOOL_IMAGE,out->width*3,1);
    while(jpg->output_scanline<jpg->output_height){unsigned y=jpg->output_scanline;jpeg_read_scanlines(jpg,row,1);
        for(unsigned x=0;x<out->width;x++)out->pixels[(size_t)y*out->width+x]=(GRErgb24){row[0][x*3],row[0][x*3+1],row[0][x*3+2]};}
    jpeg_finish_decompress(jpg);ok=1;
done:jpeg_destroy_decompress(jpg);free(jpg);free(error);fclose(file);return ok;
}
int SceneTextureImage_Read(const char* path,SceneTextureImage* image,char* error,size_t capacity)
{
    SceneTextureImage next={0};unsigned char magic[26]={0};FILE* f=path?fopen(path,"rb"):NULL;int ok=0;
    if(!f)goto done;size_t n=fread(magic,1,sizeof(magic),f);fclose(f);
    if(n>=8&&!png_sig_cmp(magic,0,8))ok=readPng(path,&next);
    else if(n>=2&&magic[0]==0xFF&&magic[1]==0xD8)ok=readJpeg(path,&next);
    else if(n>=26&&magic[0]=='B'&&magic[1]=='M') {
        /* Reject oversized headers before SDL allocates its decoding surface. */
        uint32_t w=(uint32_t)magic[18]|((uint32_t)magic[19]<<8)|((uint32_t)magic[20]<<16)|((uint32_t)magic[21]<<24);
        int32_t h=(uint32_t)magic[22]|((uint32_t)magic[23]<<8)|((uint32_t)magic[24]<<16)|((uint32_t)magic[25]<<24);
        if(!dimensions(w,h<0?-(int64_t)h:h))goto done;
        SDL_Surface* bmp=SDL_LoadBMP(path);if(!bmp)goto done;
        SDL_Surface* rgba=SDL_ConvertSurfaceFormat(bmp,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(bmp);if(!rgba)goto done;
        next.width=rgba->w;next.height=rgba->h;next.pixels=malloc((size_t)next.width*next.height*sizeof(*next.pixels));
        if(next.pixels){for(unsigned y=0;y<next.height;y++)for(unsigned x=0;x<next.width;x++)next.pixels[(size_t)y*next.width+x]=opaque((unsigned char*)rgba->pixels+y*rgba->pitch+x*4);ok=1;}
        SDL_FreeSurface(rgba);
    }
done:
    if(ok){SceneTextureImage_Free(image);*image=next;return 1;}
    SceneTextureImage_Free(&next);if(error&&capacity)snprintf(error,capacity,"无法读取图片：支持 4096 像素以内的 PNG/JPEG/BMP");return 0;
}
