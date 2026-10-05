#ifndef YMGRE_FEATURES_H
#define YMGRE_FEATURES_H
#ifndef YMGRE_ENABLE_PNG
#define YMGRE_ENABLE_PNG 1
#endif
#ifndef YMGRE_ENABLE_JPEG
#define YMGRE_ENABLE_JPEG 1
#endif
/* Build and consumer must use identical feature definitions (public struct ABI). */
#ifndef YMGRE_ENABLE_TRANSPARENCY
#define YMGRE_ENABLE_TRANSPARENCY 1
#endif
#ifndef YMGRE_ENABLE_OPACITY_MIPMAP
#define YMGRE_ENABLE_OPACITY_MIPMAP 1
#endif
#ifndef YMGRE_ENABLE_PBR
#define YMGRE_ENABLE_PBR 1
#endif
#ifndef YMGRE_ENABLE_LINEAR_COLOR
#define YMGRE_ENABLE_LINEAR_COLOR 1
#endif
#ifndef YMGRE_ENABLE_RASTER_DISPATCH
#define YMGRE_ENABLE_RASTER_DISPATCH 1
#endif
#if YMGRE_ENABLE_RASTER_DISPATCH && (!YMGRE_ENABLE_PBR || !YMGRE_ENABLE_TRANSPARENCY)
#error "Raster dispatch requires PBR and transparency"
#endif
#if YMGRE_ENABLE_OPACITY_MIPMAP && !YMGRE_ENABLE_TRANSPARENCY
#error "Opacity mipmaps require transparency"
#endif
#endif
