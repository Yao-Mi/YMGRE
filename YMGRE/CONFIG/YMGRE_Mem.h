#ifndef YMGRE_Mem_H
#define YMGRE_Mem_H

#include "../CONFIG/YMGRE_PubType.h"

void * GRE_malloc0(size_t size);//小数据缓冲区申请，在较高速度的缓冲区申请
void GRE_free0(void * ptr);//释放内存

void * GRE_malloc1(size_t size);//大数据缓冲区申请，在较低速度的缓冲区申请
void GRE_free1(void * ptr);//释放内存

void GRE_memset(void* ptr, int val, size_t size);//内存值设置
void GRE_memcpy(void* dst, const void* src, size_t size);//内存拷贝

/* Render buffers use fast memory first and fall back to large memory.
   Record the backend so a fallback allocation is freed by its owner. */
typedef union {
	void (*release)(void *);
	long double alignment;
} GRE_RenderMemoryHeader;
static inline void *GRE_RenderBuff_Malloc(size_t size)
{
	if(size>SIZE_MAX-sizeof(GRE_RenderMemoryHeader)) return NULL;
	GRE_RenderMemoryHeader *block=GRE_malloc0(size+sizeof(*block));
	if(block) block->release=GRE_free0;
	else {
		block=GRE_malloc1(size+sizeof(*block));
		if(!block) return NULL;
		block->release=GRE_free1;
	}
	return block+1;
}
static inline void *GRE_RenderTargetBuff_Malloc(size_t size)
{
	return GRE_RenderBuff_Malloc(size);
}
/* Optional independent color arena (for memory-bank placement). */
#if defined(YMGRE_RENDER_COLOR_MALLOC)
void *YMGRE_RENDER_COLOR_MALLOC(size_t size);
void YMGRE_RENDER_COLOR_FREE(void *data);
#endif
static inline void *GRE_RenderColorBuff_Malloc(size_t size)
{
#if defined(YMGRE_RENDER_COLOR_MALLOC)
 if(size>SIZE_MAX-sizeof(GRE_RenderMemoryHeader)) return NULL;
 GRE_RenderMemoryHeader *block=YMGRE_RENDER_COLOR_MALLOC(size+sizeof(*block));
 if(!block) return GRE_RenderTargetBuff_Malloc(size);
 block->release=YMGRE_RENDER_COLOR_FREE;
 return block+1;
#else
 return GRE_RenderTargetBuff_Malloc(size);
#endif
}
static inline void GRE_RenderBuff_Free(void *data)
{
	if(data) { GRE_RenderMemoryHeader *block=(GRE_RenderMemoryHeader*)data-1;block->release(block); }
}

//图像数据申请和释放
static inline void *GRE_GeometryBuff_Malloc(size_t size)
{
 return GRE_malloc1(size);
}
static inline void GRE_GeometryBuff_Free(void *data)
{
 GRE_free1(data);
}
static inline void* GRE_ImageBuff_Malloc(size_t tsize)
{
	return GRE_malloc1(tsize);
}
static inline void GRE_ImageBuff_Free(void* dap)
{
	GRE_free1(dap);
}
//多边形索引数据申请和释放
static inline void* GRE_PolyIndex_Malloc(size_t tsize)
{
	return GRE_malloc1(tsize);
}
static inline void GRE_PolyIndex_Free(void* dap)
{
	GRE_free1(dap);
}

//地形障碍数据申请和释放
static inline void* GRE_TerrainObstacles_Malloc(size_t tsize)
{
	return GRE_malloc1(tsize);
}
static inline void GRE_TerrainObstacles_Free(void* dap)
{
	GRE_free1(dap);
}

#endif
