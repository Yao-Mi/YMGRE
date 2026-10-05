#ifndef IMAGE_DECODE_H
#define IMAGE_DECODE_H
#include <stddef.h>
#include <stdint.h>
#ifndef IMAGE_DECODE_ENABLE_PNG
#define IMAGE_DECODE_ENABLE_PNG 0
#endif
#ifndef IMAGE_DECODE_ENABLE_JPEG
#define IMAGE_DECODE_ENABLE_JPEG 0
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Callbacks must accept release(NULL). NULL allocator uses malloc/free.
 * These callbacks cover wrapper/output buffers; codecs manage their own internals. */
typedef struct ImageDecodeAllocator {
 void* (*allocate)(size_t bytes);
 void (*release)(void* pixels);
} ImageDecodeAllocator;
/* File signature detection: BMP, optional PNG/JPEG. channels is 3 (packed RGB)
 * or 1 (red channel, NOT luminance). Top row first, no row padding or metadata
 * color transforms. Alpha is discarded. Success returns 1 and transfers buffer
 * ownership; release with the same allocator (free when allocator was NULL).
 * Failure returns 0 with all outputs unchanged. Existing buffers aren't freed.
 * No global allocator state; independent calls can run concurrently. */
int image_decode_load(const char* path,int channels,const ImageDecodeAllocator* allocator,
                      uint8_t** pixels,uint16_t* width,uint16_t* height);
#ifdef __cplusplus
}
#endif
#endif
