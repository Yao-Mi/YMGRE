#include "YMGRE_Mem.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"policy line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned fast,slow,freeFast,freeSlow;static int rejectFast;
void *GRE_malloc0(size_t n){fast++;return rejectFast?NULL:malloc(n);}
void *GRE_malloc1(size_t n){slow++;return malloc(n);}
void GRE_free0(void *p){if(p){freeFast++;free(p);}}
void GRE_free1(void *p){if(p){freeSlow++;free(p);}}
void GRE_memset(void *p,int c,size_t n){memset(p,c,n);}
void GRE_memcpy(void *d,const void *s,size_t n){memcpy(d,s,n);}
#define ALIGNED(p) ((uintptr_t)(p)%offsetof(struct {char c;long double x;},x)==0)
int main(void){
 void *p=GRE_RenderBuff_Malloc(64);CHECK(p&&ALIGNED(p));CHECK(fast==YMGRE_RENDER_PREFER_FAST_MEMORY);CHECK(slow==!YMGRE_RENDER_PREFER_FAST_MEMORY);GRE_RenderBuff_Free(p);
 unsigned f=fast,s=slow;p=GRE_RenderTargetBuff_Malloc(23);CHECK(p&&ALIGNED(p));CHECK(fast-f==(YMGRE_RENDER_PREFER_FAST_MEMORY&&YMGRE_RENDER_FAST_TARGET));CHECK(slow-s==!(YMGRE_RENDER_PREFER_FAST_MEMORY&&YMGRE_RENDER_FAST_TARGET));GRE_RenderBuff_Free(p);
 rejectFast=1;f=freeSlow;p=GRE_RenderBuff_MallocWithPreference(13,1);CHECK(p&&ALIGNED(p));GRE_RenderBuff_Free(p);CHECK(freeSlow==f+1);
 rejectFast=0;f=fast;s=slow;p=GRE_GeometryBuff_Malloc(9);CHECK(p);GRE_GeometryBuff_Free(p);CHECK(fast-f==YMGRE_GEOMETRY_PREFER_FAST_MEMORY);CHECK(slow-s==!YMGRE_GEOMETRY_PREFER_FAST_MEMORY);
 f=fast;s=slow;p=GRE_ImageBuff_Malloc(7);CHECK(p);GRE_ImageBuff_Free(p);CHECK(fast-f==YMGRE_IMAGE_PREFER_FAST_MEMORY);CHECK(slow-s==!YMGRE_IMAGE_PREFER_FAST_MEMORY);
 f=fast;s=slow;CHECK(!GRE_RenderBuff_Malloc(SIZE_MAX));CHECK(fast==f&&slow==s);GRE_RenderBuff_Free(NULL);
 puts("PASS memory preference, fallback ownership, alignment and overflow");return 0;
}
