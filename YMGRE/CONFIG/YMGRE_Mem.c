#include <stdlib.h>
#include <string.h>
#include "./YMGRE_Mem.h"
//小型数据用这个
//在较高速度的缓冲区申请
void * GRE_malloc0(size_t size)
{
	void* myptr = malloc(size);

	return myptr;
}
//释放内存
void GRE_free0(void * ptr)
{
	free(ptr);
}

//大型数据用这个
//在较低速度的缓冲区申请
void * GRE_malloc1(size_t size)
{
	void* myptr = malloc(size);

	return myptr;
}
//释放内存
void GRE_free1(void * ptr)
{
	free(ptr);
}

//内存值设置
void GRE_memset(void* ptr, int val , size_t size)
{
	memset(ptr, val, size);
}

//内存值拷贝
void GRE_memcpy(void* dst, const void* src, size_t size)
{
	memcpy(dst, src, size);
}
