#include "image_decode.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static int remaining, live;
static void* allocate(size_t n)
{
 if(remaining--==0)return NULL;
 void* p=malloc(n);if(p)live++;return p;
}
static void release(void* p){if(p){live--;free(p);}}
int main(int argc,char** argv)
{
 assert(argc==2);
 const char* names[]={"rgb.bmp",
#if IMAGE_DECODE_ENABLE_PNG
 "rgb.png","interlaced.png",
#endif
#if IMAGE_DECODE_ENABLE_JPEG
 "rgb.jpg","progressive.jpg",
#endif
 };
 ImageDecodeAllocator a={allocate,release};
 for(size_t n=0;n<sizeof(names)/sizeof(*names);n++){
  char path[4096];snprintf(path,sizeof(path),"%s/%s",argv[1],names[n]);
  for(int channels=1;channels<=3;channels+=2){
   int completed=0;
   for(int limit=0;limit<16;limit++){
    uint8_t sentinel,*p=&sentinel;uint16_t w=7,h=9;remaining=limit;live=0;
    int ok=image_decode_load(path,channels,&a,&p,&w,&h);
    if(ok){assert(w==3&&h==2);assert(live==1);release(p);completed=1;}
    else assert(p==&sentinel&&w==7&&h==9);
    assert(live==0);if(ok)break;
   }
   assert(completed);
   uint8_t* p=NULL;uint16_t w=0,h=0;
   assert(image_decode_load(path,channels,NULL,&p,&w,&h));free(p);
  }
 }
 puts("PASS: independent decoder, allocation failure cleanup and ownership");
}
