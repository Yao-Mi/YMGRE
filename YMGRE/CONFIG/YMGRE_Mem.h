#ifndef YMGRE_Mem_H
#define YMGRE_Mem_H

#include "../CONFIG/YMGRE_PubType.h"

void * GRE_malloc0(size_t size);//小数据缓冲区申请，在较高速度的缓冲区申请
void GRE_free0(void * ptr);//释放内存

void * GRE_malloc1(size_t size);//大数据缓冲区申请，在较低速度的缓冲区申请
void GRE_free1(void * ptr);//释放内存

void GRE_memset(void* ptr, int val, size_t size);//内存值设置
void GRE_memcpy(void* dst, const void* src, size_t size);//内存拷贝

//图像数据申请和释放
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
