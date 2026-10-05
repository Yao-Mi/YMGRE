#ifndef GIRL_VIEWER_RASTER_POOL_H
#define GIRL_VIEWER_RASTER_POOL_H
#include "YMGRE_RenderContext.h"
typedef struct RasterPool RasterPool;
RasterPool* raster_pool_create(int threads);
void raster_pool_destroy(RasterPool* pool);
void raster_pool_dispatch(void* pool,GRE_RasterJob job,void* context,uint32 count);
#endif
