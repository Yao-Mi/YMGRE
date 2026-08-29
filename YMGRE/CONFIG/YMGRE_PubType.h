#ifndef _YMGRE_PUBTYPE_H
#define _YMGRE_PUBTYPE_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t int32;
typedef int16_t int16;
typedef int64_t int64;
typedef int8_t int8;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef float float32;
typedef double float64;

//自定义数据结构
typedef float32 _color32_f;
typedef int32 _color32_i;
typedef uint16 _color16_t;
typedef uint8 _color8_t;

#pragma pack(1)

typedef  struct _color24_t
{
	_color8_t R : 8;
	_color8_t G : 8;
	_color8_t B : 8;
}color24_t;

//通用颜色
typedef union
{
	_color8_t gray;
	_color16_t rgb16;
}GREcolor;
#pragma pack(4)

// Do not leak the legacy packing rule into external libraries such as YMGUI.
#pragma pack()

typedef color24_t GRErgb24;
typedef GRErgb24* GRERGB24;
typedef _color16_t GRErgb16;
typedef GRErgb16* GRERGB16;

// Camera output format. RGB565 is the native YMGUI/SDL format and the
// project default; alternate formats remain source-compatible for ports.
#ifndef YMGRE_CAMERA_COLOR_DEPTH
#define YMGRE_CAMERA_COLOR_DEPTH 16
#endif

#if YMGRE_CAMERA_COLOR_DEPTH == 16
typedef GRErgb16 GRE_FramePixel;
typedef GRERGB16 GRE_FrameBuffer;
#elif YMGRE_CAMERA_COLOR_DEPTH == 24
typedef GRErgb24 GRE_FramePixel;
typedef GRERGB24 GRE_FrameBuffer;
#else
#error "YMGRE_CAMERA_COLOR_DEPTH must be 16 or 24"
#endif

static inline GRErgb16 GRE_RGB24_To_RGB565(GRErgb24 color)
{
	return (GRErgb16)(((uint16)(color.R & 0xF8) << 8) |
		((uint16)(color.G & 0xFC) << 3) | ((uint16)color.B >> 3));
}

static inline GRE_FramePixel GRE_FramePixel_From_RGB24(GRErgb24 color)
{
#if YMGRE_CAMERA_COLOR_DEPTH == 16
	return GRE_RGB24_To_RGB565(color);
#else
	return color;
#endif
}

static inline GRErgb24 GRE_FramePixel_To_RGB24(GRE_FramePixel color)
{
#if YMGRE_CAMERA_COLOR_DEPTH == 16
	uint8 r5 = (uint8)((color >> 11) & 0x1F);
	uint8 g6 = (uint8)((color >> 5) & 0x3F);
	uint8 b5 = (uint8)(color & 0x1F);
	return (GRErgb24){ (uint8)((r5 << 3) | (r5 >> 2)),
		(uint8)((g6 << 2) | (g6 >> 4)), (uint8)((b5 << 3) | (b5 >> 2)) };
#else
	return color;
#endif
}

static inline uint8 GRE_FramePixel_Equals(GRE_FramePixel left, GRE_FramePixel right)
{
#if YMGRE_CAMERA_COLOR_DEPTH == 16
	return (uint8)(left == right);
#else
	return (uint8)(left.R == right.R && left.G == right.G && left.B == right.B);
#endif
}

//矩形
typedef struct gre_frect_
{
	float32 x0;
	float32 y0;
	float32 x1;
	float32 y1;
}gre_frect;
typedef gre_frect* GRE_FRECT;
//浮点线段
typedef struct gre_fline_
{
	float32 x0;
	float32 y0;
	float32 x1;
	float32 y1;
}gre_fline;
typedef gre_fline* GRE_FLINE;
//线段列表
typedef struct gre_flineslist_
{
	uint16 lineMax;
	uint16 lineNum;
	GRE_FLINE data;
}gre_flineslist;
typedef gre_flineslist* GRE_fLinesList;

//整形线段
typedef struct gre_line_
{
	int16 x0;
	int16 y0;
	int16 x1;
	int16 y1;
}gre_line;
typedef gre_line* GRE_LINE;
//线段列表
typedef struct gre_lineslist_
{
	uint16 lineNum;
	GRE_LINE data;
}gre_lineslist;
typedef gre_lineslist* GRE_LinesList;


#endif
