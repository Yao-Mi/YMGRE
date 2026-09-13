/* Adapted from YMGUI/project_Demo/image_edit/image_edit.c; retain its drawing and layer algorithms. */
#include "YMGUI_PubDefine.h"
#include "YMGUI_Debug.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Mem.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Font.h"
#include "YMGUI_Label.h"
#include "YMGUI_Button.h"
#include "YMGUI_Slider.h"
#include "YMGUI_Dropdown.h"
#include "YMGUI_Canvas.h"
#include "YMGUI_ColorPicker.h"
#include "YMGUI_DrawFill.h"
#include "YMGUI_DrawImg.h"
#include "scene_texture_paint.h"
#include "YMGUI_Event.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
  ***************************************************************************************************************************
  *	@FileName:    image_edit.c
  *	@Author:      yaomimaoren
  *	@Date:        2026-08-04
  *	@Description: 第 4 个基础验证项目——类 PS 画板。催生库控件 Canvas(可绘制位图视口)+ ColorPicker
  *	              (HSV 取色)。图层栈 / 融合 / 合成全在 app 侧(与库分层纪律一致:Canvas 不懂图层,
  *	              就像 Grid 不懂公式)。每图层 = RGBA 直存(color+alpha),含独立不透明度 / 混合模式 /
  *	              可见位;此集成版本在白底上自底向上合成到 Canvas 显示缓冲。工具:硬笔刷 / 橡皮 / 柔和笔刷
  *	              / 喷枪 / 直线 / 矩形 / 三角形 / 圆形 / 折线 / 多边形(逐点击) / 填充桶 / 吸管。形状拖动
  *	              时实时预览(每帧 composite 底图 + 叠形状,抬起才提交)。右侧自绘图层缩略图列表:点行
  *	              选中该层(才同步不透明度 / 混合 / 显隐等属性),点眼睛切显隐,面板下方增 / 删图层。
  *	              集成到 scene_baker，共用编辑器事件循环与字体，支持绘制撤销和贴图预览回调。
  *	@Version:     1.1
  *
  ***************************************************************************************************************************
  * Copyright (C), 2026-2036, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/

#define SCR_W 1024
#define SCR_H 816
#define BAND_H 64

//画布像素尺寸(app 侧图层与合成的分辨率)，保持图像宽高比，最长边不超过 512。
#define PAINT_MAX_PIXELS (512*512)
static int g_width=512,g_height=512;
#define CW g_width
#define CH g_height
#define MAX_LAYERS 4

//---- 右侧图层缩略图面板几何 ----
#define LP_X   816
#define LP_Y   112
#define LP_W   184
#define LP_ROW 54
#define LP_TW  64
#define LP_TH  48

//折线/多边形顶点缓冲
#define MAX_POLY 64

//每图层:RGBA 直存(直 alpha,非预乘),行优先 CW*CH。
typedef struct
{
	uint8 r[PAINT_MAX_PIXELS];
	uint8 g[PAINT_MAX_PIXELS];
	uint8 b[PAINT_MAX_PIXELS];
	uint8 a[PAINT_MAX_PIXELS];//0=全透明,255=不透明
}Layer;

//混合模式
enum { BLEND_NORMAL = 0, BLEND_MULTIPLY, BLEND_SCREEN, BLEND_ADD, BLEND_COUNT };
//工具(前 4 为笔刷族;LINE..POLYGON 为形状;FILL/PICK 为其它)
enum {
	TOOL_BRUSH = 0, TOOL_ERASER, TOOL_SOFT, TOOL_AIRBRUSH,
	TOOL_LINE, TOOL_RECT, TOOL_TRI, TOOL_CIRCLE, TOOL_POLYLINE, TOOL_POLYGON,
	TOOL_FILL, TOOL_PICK, TOOL_COUNT
};

static Layer  g_layers[MAX_LAYERS];
static uint8  g_visible[MAX_LAYERS];
static uint8  g_opacity[MAX_LAYERS];//0..255
static uint8  g_blend[MAX_LAYERS];
static int    g_layer_count = 1;
static int    g_active = 0;

//APP 全局
static GYCTX g_ctx;
static GYOBJ g_canvas;
static GYOBJ g_picker;
static GYOBJ g_status;
static GYOBJ g_layer_lbl;
static GYOBJ g_opa_slider;
static GYOBJ g_blend_dd;
static GYOBJ g_panel;      //右侧图层缩略图面板(自绘)

//当前绘制状态
static uint8  g_ink_r = 0xE0, g_ink_g = 0x40, g_ink_b = 0x40;
static int    g_tool = TOOL_BRUSH;
static int    g_size = 8;    //笔刷半径像素(1..30)
static int    g_grain = 4;   //喷枪粒度(1=细..10=粗)
static int32  g_start_x = -1, g_start_y = -1;//拖动形状(直线/矩形/三角/圆)起点

static int    g_dirty = 1;   //合成脏标记

//折线/多边形累积顶点(逐次点击)
static int32  g_poly_x[MAX_POLY], g_poly_y[MAX_POLY];
static int    g_poly_n = 0;

static void pushUndo(void);

//确定性 PRNG(xorshift32)——喷枪散点,selftest 可复现
static uint32 g_rng = 0x1234567u;
static void   rngSeed(uint32 s) { g_rng = s ? s : 1u; }
static uint32 rngNext(void) { uint32 x = g_rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; g_rng = x; return x; }

//整数平方根(圆半径用,免 libm/FPU)
static int32 isqrt32(int32 v)
{
	if (v <= 0) return 0;
	int32 x = v, y = (x + 1) / 2;
	while (y < x) { x = y; y = (x + v / x) / 2; }
	return x;
}

//===========================================================================
// 合成:白底 → 自底向上叠各可见图层(按不透明度 + 混合模式)→ 写 Canvas 缓冲
//===========================================================================

//单通道混合模式:base/over 均 0..255,返回混合后色(未计 alpha)
static uint8 blendChan(uint8 mode, uint8 base, uint8 over)
{
	switch (mode)
	{
	case BLEND_MULTIPLY: return (uint8)((base * over) / 255);
	case BLEND_SCREEN:   return (uint8)(255 - ((255 - base) * (255 - over)) / 255);
	case BLEND_ADD:      { int v = base + over; return (uint8)(v > 255 ? 255 : v); }
	default:             return over;//NORMAL
	}
}

static void composite(void)
{
	GYpx* out = YMGUI_Canvas_GetBuffer(g_canvas);
	for (int32 i = 0; i < CW * CH; i++)
	{
		// Scene materials are RGB: transparent paint reveals white.
		int32 rr = 255, gg = 255, bb = 255;
		for (int l = 0; l < g_layer_count; l++)
		{
			if (!g_visible[l]) continue;
			uint32 a = g_layers[l].a[i];
			if (a == 0) continue;
			//有效 alpha = 像素 alpha × 图层不透明度
			a = (a * g_opacity[l]) / 255;
			if (a == 0) continue;
			uint8 sr = g_layers[l].r[i], sg = g_layers[l].g[i], sb = g_layers[l].b[i];
			//先按混合模式算出"要叠上去的色",再按 alpha 与 base 线性插值
			uint8 mr = blendChan(g_blend[l], (uint8)rr, sr);
			uint8 mg = blendChan(g_blend[l], (uint8)gg, sg);
			uint8 mb = blendChan(g_blend[l], (uint8)bb, sb);
			rr = (mr * (int32)a + rr * (int32)(255 - a)) / 255;
			gg = (mg * (int32)a + gg * (int32)(255 - a)) / 255;
			bb = (mb * (int32)a + bb * (int32)(255 - a)) / 255;
		}
		out[i] = GY_ColorToPx(GY_ARGB(0xFF, rr, gg, bb));
	}
	g_dirty = 0;
	YMGUI_Canvas_Invalidate(g_canvas);
}

//===========================================================================
// 图层落笔:src-over 直 alpha 合成一个像素(cov = 覆盖度 0..255)
//===========================================================================
static void depositPx(Layer* L, int32 x, int32 y, uint8 cov)
{
	if (x < 0 || x >= CW || y < 0 || y >= CH || cov == 0) return;
	int32 i = y * CW + x;
	uint32 da = L->a[i];
	//out_a = cov + da*(1-cov)
	uint32 oa = cov + da * (255 - cov) / 255;
	if (oa == 0) { L->a[i] = 0; return; }
	//直 alpha 加权:out_c = (src*cov + dst*da*(1-cov)) / oa
	uint32 wsrc = cov;
	uint32 wdst = da * (255 - cov) / 255;
	L->r[i] = (uint8)((g_ink_r * wsrc + L->r[i] * wdst) / oa);
	L->g[i] = (uint8)((g_ink_g * wsrc + L->g[i] * wdst) / oa);
	L->b[i] = (uint8)((g_ink_b * wsrc + L->b[i] * wdst) / oa);
	L->a[i] = (uint8)oa;
}

//橡皮:按 cov 削减 alpha(RGB 不动)
static void erasePx(Layer* L, int32 x, int32 y, uint8 cov)
{
	if (x < 0 || x >= CW || y < 0 || y >= CH || cov == 0) return;
	int32 i = y * CW + x;
	uint32 da = L->a[i];
	L->a[i] = (uint8)(da * (255 - cov) / 255);
}

//硬笔刷:半径内实心圆,cov=255
static void stampHard(Layer* L, int32 cx, int32 cy, int rad)
{
	int32 r2 = rad * rad;
	for (int32 dy = -rad; dy <= rad; dy++)
		for (int32 dx = -rad; dx <= rad; dx++)
			if (dx * dx + dy * dy <= r2)
				depositPx(L, cx + dx, cy + dy, 255);
}

//柔和笔刷:cov 随到圆心距离线性衰减(边缘渐隐 → 过渡感)
static void stampSoft(Layer* L, int32 cx, int32 cy, int rad)
{
	if (rad < 1) rad = 1;
	for (int32 dy = -rad; dy <= rad; dy++)
		for (int32 dx = -rad; dx <= rad; dx++)
		{
			int32 d2 = dx * dx + dy * dy;
			if (d2 > rad * rad) continue;
			//线性:中心 255 → 边缘 0(用距离近似,免开方:cov=255*(rad-d)/rad,d≈sqrt)
			int32 d = 0; { int32 t = 0; while (t * t < d2) t++; d = t; }//整数 ceil(sqrt)
			int32 cov = 255 * (rad - d) / rad;
			if (cov > 0) depositPx(L, cx + dx, cy + dy, (uint8)cov);
		}
}

//橡皮圆
static void stampErase(Layer* L, int32 cx, int32 cy, int rad)
{
	int32 r2 = rad * rad;
	for (int32 dy = -rad; dy <= rad; dy++)
		for (int32 dx = -rad; dx <= rad; dx++)
			if (dx * dx + dy * dy <= r2)
				erasePx(L, cx + dx, cy + dy, 255);
}

/**
  * @brief 喷枪:固定喷口(cx,cy),按粒度把 N 个点散落在半径内。
  *        粒度越细(grain 小)→ 每点 coverage 越低 → 累积越柔和、色度越低(直 alpha 稀释)。
  *        粒度越粗(grain 大)→ 每点 coverage 越高、越集中 → 更实。
  */
static void stampAirbrush(Layer* L, int32 cx, int32 cy, int rad)
{
	if (rad < 1) rad = 1;
	int spray = rad * 4;
	if (spray > 400) spray = 400;
	uint8 cov = (uint8)(8 + (g_grain - 1) * 8);
	int32 r2 = rad * rad;
	for (int n = 0; n < spray; n++)
	{
		int32 dx = (int32)(rngNext() % (uint32)(2 * rad + 1)) - rad;
		int32 dy = (int32)(rngNext() % (uint32)(2 * rad + 1)) - rad;
		if (dx * dx + dy * dy > r2) continue;
		depositPx(L, cx + dx, cy + dy, cov);
	}
}

//沿 (x0,y0)->(x1,y1) 用当前笔刷补点(防断线)
static void strokeTo(int32 x0, int32 y0, int32 x1, int32 y1, int rad, int tool)
{
	Layer* L = &g_layers[g_active];
	int32 dx = x1 - x0, dy = y1 - y0;
	int32 adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
	int32 steps = adx > ady ? adx : ady;
	if (steps == 0) steps = 1;
	int32 steppx = rad / 2; if (steppx < 1) steppx = 1;
	for (int32 s = 0; s <= steps; s += steppx)
	{
		int32 x = x0 + dx * s / steps;
		int32 y = y0 + dy * s / steps;
		if (tool == TOOL_ERASER)      stampErase(L, x, y, rad);
		else if (tool == TOOL_SOFT)   stampSoft(L, x, y, rad);
		else if (tool == TOOL_AIRBRUSH) stampAirbrush(L, x, y, rad);
		else                          stampHard(L, x, y, rad);
	}
}

//===========================================================================
// 形状栅格化:与"落点动作"解耦。plot 决定像素落到哪:
//   plotLayer   → 用当前笔刷半径实心盖到当前图层(提交)
//   plotPreview → 直接把 g_size 半径的墨色实心圆写到 Canvas 显示缓冲(拖动预览,不改图层)
//===========================================================================
typedef void (*PlotFn)(int32 x, int32 y);

//提交:实心圆盖到当前图层
static void plotLayer(int32 x, int32 y) { stampHard(&g_layers[g_active], x, y, g_size); }

//预览:把 g_size 半径实心圆以墨色直接写到显示缓冲(合成之后叠上去)
static void plotPreview(int32 x, int32 y)
{
	GYpx* out = YMGUI_Canvas_GetBuffer(g_canvas);
	GYpx  ink = GY_ColorToPx(GY_ARGB(0xFF, g_ink_r, g_ink_g, g_ink_b));
	int rad = g_size, r2 = rad * rad;
	for (int32 dy = -rad; dy <= rad; dy++)
		for (int32 dx = -rad; dx <= rad; dx++)
		{
			if (dx * dx + dy * dy > r2) continue;
			int32 px = x + dx, py = y + dy;
			if (px < 0 || px >= CW || py < 0 || py >= CH) continue;
			out[py * CW + px] = ink;
		}
}

//直线(Bresenham,每点调 plot)
static void rasLine(int32 x0, int32 y0, int32 x1, int32 y1, PlotFn plot)
{
	int32 dx = x1 - x0, dy = y1 - y0;
	int32 sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
	int32 adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
	int32 err = adx - ady, x = x0, y = y0;
	for (;;)
	{
		plot(x, y);
		if (x == x1 && y == y1) break;
		int32 e2 = 2 * err;
		if (e2 > -ady) { err -= ady; x += sx; }
		if (e2 <  adx) { err += adx; y += sy; }
	}
}

//矩形描边(对角两点)
static void rasRect(int32 x0, int32 y0, int32 x1, int32 y1, PlotFn plot)
{
	rasLine(x0, y0, x1, y0, plot);
	rasLine(x1, y0, x1, y1, plot);
	rasLine(x1, y1, x0, y1, plot);
	rasLine(x0, y1, x0, y0, plot);
}

//三角形:以拖动外框(x0,y0)-(x1,y1) 定形,顶点在上边中点,底边两角
static void rasTri(int32 x0, int32 y0, int32 x1, int32 y1, PlotFn plot)
{
	int32 apex_x = (x0 + x1) / 2, apex_y = y0;
	rasLine(apex_x, apex_y, x0, y1, plot);
	rasLine(apex_x, apex_y, x1, y1, plot);
	rasLine(x0, y1, x1, y1, plot);
}

//圆:圆心(cx,cy),半径 = 到(px,py)距离。中点画圆(八对称)
static void rasCircle(int32 cx, int32 cy, int32 px, int32 py, PlotFn plot)
{
	int32 dx = px - cx, dy = py - cy;
	int32 r = isqrt32(dx * dx + dy * dy);
	if (r < 1) { plot(cx, cy); return; }
	int32 x = r, y = 0, err = 1 - r;
	while (x >= y)
	{
		plot(cx + x, cy + y); plot(cx + y, cy + x);
		plot(cx - y, cy + x); plot(cx - x, cy + y);
		plot(cx - x, cy - y); plot(cx - y, cy - x);
		plot(cx + y, cy - x); plot(cx + x, cy - y);
		y++;
		if (err < 0) err += 2 * y + 1;
		else { x--; err += 2 * (y - x) + 1; }
	}
}

//折线/多边形:顺次连顶点;closed=1 时首尾闭合
static void rasPoly(const int32* xs, const int32* ys, int n, int closed, PlotFn plot)
{
	if (n <= 0) return;
	if (n == 1) { plot(xs[0], ys[0]); return; }
	for (int i = 0; i + 1 < n; i++) rasLine(xs[i], ys[i], xs[i + 1], ys[i + 1], plot);
	if (closed && n >= 3) rasLine(xs[n - 1], ys[n - 1], xs[0], ys[0], plot);
}

//---- 提交包装:设好半径后栅格化到图层 ----
static void commitLine(int32 x0, int32 y0, int32 x1, int32 y1, int rad) { g_size = rad; rasLine(x0, y0, x1, y1, plotLayer); }
static void commitRect(int32 x0, int32 y0, int32 x1, int32 y1, int rad) { g_size = rad; rasRect(x0, y0, x1, y1, plotLayer); }
static void commitTri(int32 x0, int32 y0, int32 x1, int32 y1, int rad)  { g_size = rad; rasTri(x0, y0, x1, y1, plotLayer); }
static void commitCircle(int32 cx, int32 cy, int32 px, int32 py, int rad) { g_size = rad; rasCircle(cx, cy, px, py, plotLayer); }
static void commitPoly(int closed) { rasPoly(g_poly_x, g_poly_y, g_poly_n, closed, plotLayer); }

//4 邻域 flood fill:把与 (sx,sy) 同色连通域填成当前墨色(实心 alpha)。
static int32 g_fill_stack[PAINT_MAX_PIXELS];
static uint8 g_fill_seen[PAINT_MAX_PIXELS];
static void floodFill(int32 sx, int32 sy)
{
	if (sx < 0 || sx >= CW || sy < 0 || sy >= CH) return;
	Layer* L = &g_layers[g_active];
	int32 si = sy * CW + sx;
	uint8 tr = L->r[si], tg = L->g[si], tb = L->b[si], ta = L->a[si];
	for (int32 i = 0; i < CW * CH; i++) g_fill_seen[i] = 0;
	int32 top = 0;
	g_fill_stack[top++] = si;
	g_fill_seen[si] = 1;
	int tol = 24;
	while (top > 0)
	{
		int32 i = g_fill_stack[--top];
		int32 x = i % CW, y = i / CW;
		int dr = (int)L->r[i] - tr, dg = (int)L->g[i] - tg, db = (int)L->b[i] - tb, da = (int)L->a[i] - ta;
		if (dr < 0) { dr = -dr; }
		if (dg < 0) { dg = -dg; }
		if (db < 0) { db = -db; }
		if (da < 0) { da = -da; }
		if (dr > tol || dg > tol || db > tol || da > tol) continue;
		L->r[i] = g_ink_r; L->g[i] = g_ink_g; L->b[i] = g_ink_b; L->a[i] = 255;
		int32 nb[4] = { (x > 0 ? i - 1 : -1), (x < CW - 1 ? i + 1 : -1),
		                (y > 0 ? i - CW : -1), (y < CH - 1 ? i + CW : -1) };
		for (int k = 0; k < 4; k++)
		{
			int32 ni = nb[k];
			if (ni >= 0 && !g_fill_seen[ni]) { g_fill_seen[ni] = 1; g_fill_stack[top++] = ni; }
		}
	}
}

//吸管:从合成结果(Canvas 缓冲)取当前像素色 → 设墨色 + 同步 ColorPicker
static void eyedrop(int32 x, int32 y)
{
	if (x < 0 || x >= CW || y < 0 || y >= CH) return;
	GYpx* out = YMGUI_Canvas_GetBuffer(g_canvas);
	GYcolor c = GY_PxToColor(out[y * CW + x]);
	g_ink_r = (uint8)GY_COLOR_R(c);
	g_ink_g = (uint8)GY_COLOR_G(c);
	g_ink_b = (uint8)GY_COLOR_B(c);
	YMGUI_ColorPicker_SetColor(g_picker, GY_ARGB(0xFF, g_ink_r, g_ink_g, g_ink_b));
}

//某工具是否为"拖动成形"形状(按下起点 → 拖动预览 → 抬起提交)
static int isDragShape(int t)
{
	return t == TOOL_LINE || t == TOOL_RECT || t == TOOL_TRI || t == TOOL_CIRCLE;
}

//拖动预览:先重建底图,再把当前形状(墨色实心)叠到显示缓冲。不置 g_dirty → 预览留屏。
static void previewDragShape(int32 cx, int32 cy)
{
	composite();//重建底图(内部会 Invalidate + 清 g_dirty)
	switch (g_tool)
	{
	case TOOL_LINE:   rasLine(g_start_x, g_start_y, cx, cy, plotPreview); break;
	case TOOL_RECT:   rasRect(g_start_x, g_start_y, cx, cy, plotPreview); break;
	case TOOL_TRI:    rasTri(g_start_x, g_start_y, cx, cy, plotPreview); break;
	case TOOL_CIRCLE: rasCircle(g_start_x, g_start_y, cx, cy, plotPreview); break;
	default: break;
	}
	YMGUI_Canvas_Invalidate(g_canvas);
}

//折线/多边形预览:重建底图 + 画已定顶点段 + 从末点到光标的橡皮筋段
static void previewPoly(int32 cx, int32 cy)
{
	composite();
	rasPoly(g_poly_x, g_poly_y, g_poly_n, 0, plotPreview);
	if (g_poly_n > 0 && cx >= 0)
		rasLine(g_poly_x[g_poly_n - 1], g_poly_y[g_poly_n - 1], cx, cy, plotPreview);
	YMGUI_Canvas_Invalidate(g_canvas);
}

//画布绘制回调:按工具分派
static void paintTool(GYOBJ c, GYcanvas_phase phase, int32 cx, int32 cy, uint8 in)
{
	(void)c;
	int rad = g_size;
	if (phase == GY_CANVAS_DOWN)
	{
		if (isDragShape(g_tool)) { g_start_x = cx; g_start_y = cy; return; }
		switch (g_tool)
		{
		case TOOL_POLYLINE: case TOOL_POLYGON:
		{
			int need = (g_tool == TOOL_POLYGON) ? 3 : 2;
			//点回起点附近且已够点 → 闭合/结束并提交
			if (g_poly_n >= need)
			{
				int32 ddx = cx - g_poly_x[0], ddy = cy - g_poly_y[0];
				if (ddx * ddx + ddy * ddy <= 64)//8px 内
				{
					g_size = rad;
					commitPoly(g_tool == TOOL_POLYGON);
					g_poly_n = 0; g_dirty = 1; return;
				}
			}
			if (g_poly_n < MAX_POLY) { g_poly_x[g_poly_n] = cx; g_poly_y[g_poly_n] = cy; g_poly_n++; }
			previewPoly(cx, cy);//显示已加顶点(不置 dirty,留屏)
			return;
		}
		case TOOL_FILL: floodFill(cx, cy); g_dirty = 1; return;
		case TOOL_PICK: composite(); eyedrop(cx, cy); return;//先确保合成最新再取
		default:
			strokeTo(cx, cy, cx, cy, rad, g_tool); g_start_x = cx; g_start_y = cy; g_dirty = 1; return;
		}
	}
	else if (phase == GY_CANVAS_MOVE)
	{
		if (isDragShape(g_tool)) { if (g_start_x >= 0) previewDragShape(cx, cy); return; }
		switch (g_tool)
		{
		case TOOL_POLYLINE: case TOOL_POLYGON: previewPoly(cx, cy); return;//按住拖时橡皮筋跟随
		case TOOL_PICK: case TOOL_FILL: return;
		default:
			strokeTo(g_start_x, g_start_y, cx, cy, rad, g_tool);
			g_start_x = cx; g_start_y = cy; g_dirty = 1; return;
		}
	}
	else //UP
	{
		if (isDragShape(g_tool) && g_start_x >= 0)
		{
			switch (g_tool)
			{
			case TOOL_LINE:   commitLine(g_start_x, g_start_y, cx, cy, rad); break;
			case TOOL_RECT:   commitRect(g_start_x, g_start_y, cx, cy, rad); break;
			case TOOL_TRI:    commitTri(g_start_x, g_start_y, cx, cy, rad); break;
			case TOOL_CIRCLE: commitCircle(g_start_x, g_start_y, cx, cy, rad); break;
			default: break;
			}
			g_dirty = 1;
		}
		g_start_x = g_start_y = -1;
		(void)in;
	}
}

//===========================================================================
// UI 回调
//===========================================================================
static void updateLayerLabel(void)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "选中图层 %d/%d  %s  a=%d",
		g_active + 1, g_layer_count, g_visible[g_active] ? "显示" : "隐藏", (int)g_opacity[g_active]);
	YMGUI_Label_SetText(g_layer_lbl, buf);
}

//选中层变化后:把不透明度滑块 / 混合下拉 / 标签 / 缩略图面板同步到选中层
static void syncLayerControls(void)
{
	if (g_opa_slider) YMGUI_Slider_SetValue(g_opa_slider, g_opacity[g_active]);
	if (g_blend_dd)   YMGUI_Dropdown_SetSelected(g_blend_dd, g_blend[g_active]);
	updateLayerLabel();
	if (g_panel) YMGUI_Obj_Invalidate(g_panel);
}

static void onColor(GYOBJ picker, GYcolor color)
{
	(void)picker;
	g_ink_r = (uint8)GY_COLOR_R(color);
	g_ink_g = (uint8)GY_COLOR_G(color);
	g_ink_b = (uint8)GY_COLOR_B(color);
}

static void onTool(GYOBJ dd, uint16 sel)
{
	(void)dd;
	g_tool = sel;
	g_poly_n = 0;       //切工具丢弃未完成的折线/多边形
	g_start_x = g_start_y = -1;
	g_dirty = 1;        //擦掉可能残留的预览
}
static void onBlend(GYOBJ dd, uint16 sel) { pushUndo(); (void)dd; g_blend[g_active] = (uint8)sel; g_dirty = 1; if (g_panel) YMGUI_Obj_Invalidate(g_panel); }
static void onSize(GYOBJ s, int32 v) { (void)s; g_size = v < 1 ? 1 : v; }
static void onGrain(GYOBJ s, int32 v) { (void)s; g_grain = v < 1 ? 1 : (v > 10 ? 10 : v); }
static void onOpacity(GYOBJ s, int32 v)
{ pushUndo();
	(void)s;
	g_opacity[g_active] = (uint8)GYLimitMaxMin(0, v, 255);
	g_dirty = 1;
	updateLayerLabel();
	if (g_panel) YMGUI_Obj_Invalidate(g_panel);
}

static void onAddLayer(GYOBJ b)
{ pushUndo();
	(void)b;
	if (g_layer_count >= MAX_LAYERS) return;
	int n = g_layer_count++;
	for (int32 i = 0; i < CW * CH; i++) { g_layers[n].a[i] = 0; g_layers[n].r[i] = g_layers[n].g[i] = g_layers[n].b[i] = 0; }
	g_visible[n] = 1; g_opacity[n] = 255; g_blend[n] = BLEND_NORMAL;
	g_active = n;
	syncLayerControls();
}

//删除选中层:后面的层前移;至少保留 1 层
static void onDeleteLayer(GYOBJ b)
{ pushUndo();
	(void)b;
	if (g_layer_count <= 1) return;
	for (int l = g_active; l < g_layer_count - 1; l++)
	{
		g_layers[l] = g_layers[l + 1];//整层结构体拷贝
		g_visible[l] = g_visible[l + 1];
		g_opacity[l] = g_opacity[l + 1];
		g_blend[l] = g_blend[l + 1];
	}
	g_layer_count--;
	if (g_active >= g_layer_count) g_active = g_layer_count - 1;
	g_dirty = 1;
	syncLayerControls();
}

static void onToggleVis(GYOBJ b)
{ pushUndo();
	(void)b;
	g_visible[g_active] = !g_visible[g_active];
	g_dirty = 1;
	updateLayerLabel();
	if (g_panel) YMGUI_Obj_Invalidate(g_panel);
}

static void onClearLayer(GYOBJ b)
{ pushUndo();
	(void)b;
	Layer* L = &g_layers[g_active];
	for (int32 i = 0; i < CW * CH; i++) { L->a[i] = 0; L->r[i] = L->g[i] = L->b[i] = 0; }
	g_dirty = 1;
	if (g_panel) YMGUI_Obj_Invalidate(g_panel);
}

//初始化图层栈(全清空,层 0 可见不透明)
static void initLayers(void)
{
	for (int l = 0; l < MAX_LAYERS; l++)
	{
		for (int32 i = 0; i < CW * CH; i++)
		{
			g_layers[l].a[i] = 0; g_layers[l].r[i] = g_layers[l].g[i] = g_layers[l].b[i] = 0;
		}
		g_visible[l] = 1; g_opacity[l] = 255; g_blend[l] = BLEND_NORMAL;
	}
	g_layer_count = 1; g_active = 0;
}

//===========================================================================
// 右侧图层缩略图列表(app 侧自绘对象:单 draw_cb 画全部行 + event_cb 命中选中)
//   行序 = PS 序(顶层在最上)。行 i(自上而下)对应图层 g_layer_count-1-i。
//   每行:缩略图(降采样叠棋盘底) + 层号/混合 + 眼睛显隐块;选中行描高亮框。
//===========================================================================
static GYpx s_thumb[LP_TW * LP_TH];//缩略图临时缓冲

//把图层 l 降采样合成到棋盘底 → s_thumb
static void buildThumb(int l)
{
	for (int32 ty = 0; ty < LP_TH; ty++)
		for (int32 tx = 0; tx < LP_TW; tx++)
		{
			int32 sx = tx * CW / LP_TW, sy = ty * CH / LP_TH;
			int32 si = sy * CW + sx;
			uint8 chk = (((tx >> 3) + (ty >> 3)) & 1) ? 0x60 : 0x48;
			int32 rr = chk, gg = chk, bb = chk;
			uint32 a = g_layers[l].a[si];
			if (a)
			{
				rr = (g_layers[l].r[si] * (int32)a + rr * (int32)(255 - a)) / 255;
				gg = (g_layers[l].g[si] * (int32)a + gg * (int32)(255 - a)) / 255;
				bb = (g_layers[l].b[si] * (int32)a + bb * (int32)(255 - a)) / 255;
			}
			s_thumb[ty * LP_TW + tx] = GY_ColorToPx(GY_ARGB(0xFF, rr, gg, bb));
		}
}

//行 i(自上而下,0=最上=顶层)→ 图层下标
static int rowToLayer(int row) { return g_layer_count - 1 - row; }

static void panelDrawCb(GYOBJ obj, GYSURFACE s, const GYrect* abs)
{
	(void)obj;
	YMGUI_Draw_Fill(s, abs, GY_ARGB(0xFF, 0x1A, 0x1A, 0x22), GY_OPA_COVER);
	GYFONT font = &YMGUI_Font_Default;
	const char* blends[] = { "正常", "正片叠底", "滤色", "线性减淡" };

	for (int row = 0; row < g_layer_count; row++)
	{
		int l = rowToLayer(row);
		GYcoord ry = abs->y + row * LP_ROW;
		GYrect rrect = { abs->x, ry, LP_W, LP_ROW - 2 };
		//选中行高亮底
		GYcolor rbg = (l == g_active) ? GY_ARGB(0xFF, 0x30, 0x38, 0x50) : GY_ARGB(0xFF, 0x22, 0x22, 0x2A);
		YMGUI_Draw_Fill(s, &rrect, rbg, GY_OPA_COVER);

		//缩略图
		buildThumb(l);
		GYimg img = { s_thumb, LP_TW, LP_TH, 0, 0 };
		YMGUI_Draw_Img(s, &img, abs->x + 4, ry + 3);

		//文字:层号 + 混合模式 + 不透明度
		char buf[48];
		snprintf(buf, sizeof(buf), "层%d %s", l + 1, blends[g_blend[l]]);
		YMGUI_Draw_Text(s, font, abs->x + LP_TW + 8, ry + 6, buf,
			(l == g_active) ? GY_ARGB(0xFF, 0xF0, 0xE0, 0x80) : GY_ARGB(0xFF, 0xC0, 0xC0, 0xC8));
		snprintf(buf, sizeof(buf), "a=%d", (int)g_opacity[l]);
		YMGUI_Draw_Text(s, font, abs->x + LP_TW + 8, ry + 24, buf, GY_ARGB(0xFF, 0xA0, 0xA0, 0xA8));

		//眼睛显隐块(右下角小方)
		GYrect eye = { abs->x + LP_W - 20, ry + LP_TH - 14, 16, 16 };
		YMGUI_Draw_Fill(s, &eye, g_visible[l] ? GY_ARGB(0xFF, 0x40, 0xC0, 0x50) : GY_ARGB(0xFF, 0x50, 0x50, 0x58), GY_OPA_COVER);
		YMGUI_Draw_Text(s, font, eye.x + 3, eye.y, g_visible[l] ? "o" : "x", GY_ARGB(0xFF, 0x10, 0x10, 0x14));
	}
}

static void panelEventCb(GYOBJ obj, GYEvent e)
{
	if (e != GY_EVENT_Clicked) return;
	GYrect abs; YMGUI_Obj_GetAbsArea(obj, &abs);
	GYcoord lx = obj->ctx->point_x - abs.x;
	GYcoord ly = obj->ctx->point_y - abs.y;
	int row = ly / LP_ROW;
	if (row < 0 || row >= g_layer_count) return;
	int l = rowToLayer(row);
	GYcoord ry = row * LP_ROW;
	//点眼睛块 → 切显隐
	if (lx >= LP_W - 20 && ly >= ry + LP_TH - 14 && ly <= ry + LP_TH + 2)
	{
		pushUndo();g_visible[l] = !g_visible[l];
		g_dirty = 1;
		if (l == g_active) updateLayerLabel();
		YMGUI_Obj_Invalidate(obj);
		return;
	}
	//否则选中该层
	g_active = l;
	syncLayerControls();
}

static GYOBJ createLayerPanel(GYOBJ parent)
{
	GYcoord h = MAX_LAYERS * LP_ROW + 2;
	GYOBJ p = YMGUI_Creat_Obj_Creat(parent, LP_X, LP_Y, LP_W, h);
	p->draw_cb = panelDrawCb;
	p->event_cb = panelEventCb;
	YMGUI_Obj_Invalidate(p);
	return p;
}


/* Bounded full-layer history: one checkpoint per stroke, with redo. */
typedef struct {Layer layers[MAX_LAYERS];uint8 visible[MAX_LAYERS],opacity[MAX_LAYERS],blend[MAX_LAYERS];int count,active;} PaintSnapshot;
static PaintSnapshot* undoStack[8],*redoStack[8];static int undoCount,redoCount;
static GYOBJ dialog,toolDropdown;
static GYOBJ returnFocus;
static void (*canvasEvent)(GYOBJ,GYEvent);
static SceneTexturePaintDone doneCallback;static void* doneUser;
static PaintSnapshot* snapshot(void)
{
    PaintSnapshot* p=malloc(sizeof(*p));if(!p)return NULL;
    memcpy(p->layers,g_layers,sizeof(g_layers));memcpy(p->visible,g_visible,sizeof(g_visible));memcpy(p->opacity,g_opacity,sizeof(g_opacity));memcpy(p->blend,g_blend,sizeof(g_blend));p->count=g_layer_count;p->active=g_active;return p;
}
static void stackPush(PaintSnapshot** stack,int* count,PaintSnapshot* p)
{if(!p)return;if(*count==8){free(stack[0]);memmove(stack,stack+1,7*sizeof(*stack));(*count)--;}stack[(*count)++]=p;}
static void clearHistory(void){while(undoCount)free(undoStack[--undoCount]);while(redoCount)free(redoStack[--redoCount]);}
static void pushUndo(void){stackPush(undoStack,&undoCount,snapshot());while(redoCount)free(redoStack[--redoCount]);}
static void history(int redo)
{
    PaintSnapshot** from=redo?redoStack:undoStack,**to=redo?undoStack:redoStack;int* n=redo?&redoCount:&undoCount,*m=redo?&undoCount:&redoCount;
    if(!*n)return;PaintSnapshot* current=snapshot();if(!current)return;PaintSnapshot* p=from[--*n];stackPush(to,m,current);
    memcpy(g_layers,p->layers,sizeof(g_layers));memcpy(g_visible,p->visible,sizeof(g_visible));memcpy(g_opacity,p->opacity,sizeof(g_opacity));memcpy(g_blend,p->blend,sizeof(g_blend));g_layer_count=p->count;g_active=p->active;free(p);
    g_poly_n=0;g_start_x=g_start_y=-1;g_dirty=1;syncLayerControls();
}
static void undoPaint(GYOBJ b){(void)b;history(0);}static void redoPaint(GYOBJ b){(void)b;history(1);}
static void onPaint(GYOBJ c,GYcanvas_phase phase,int32 x,int32 y,uint8 in)
{
    if(phase==GY_CANVAS_DOWN){if(!in)return;YMGUI_SetFocus(c->ctx,c);if(g_tool!=TOOL_PICK)pushUndo();}
    x=GYLimitMaxMin(-32,x,CW+32);y=GYLimitMaxMin(-32,y,CH+32);paintTool(c,phase,x,y,in);
}
void SceneTexturePaint_Tick(void)
{
    if(!dialog||(dialog->state&GY_STATE_Hidden))return;
    int32 x,y;if(g_tool==TOOL_AIRBRUSH&&YMGUI_Canvas_IsDrawing(g_canvas,&x,&y)&&x>=0&&x<CW&&y>=0&&y<CH){stampAirbrush(&g_layers[g_active],x,y,g_size);g_dirty=1;}
    if(g_dirty){composite();YMGUI_Obj_Invalidate(g_panel);}
}
void SceneTexturePaint_Close(void)
{if(dialog&&!(dialog->state&GY_STATE_Hidden)){YMGUI_Dropdown_Close(toolDropdown);YMGUI_Dropdown_Close(g_blend_dd);YMGUI_Obj_SetHidden(dialog,1);YMGUI_SetFocus(g_ctx,returnFocus);}clearHistory();g_start_x=g_start_y=-1;g_poly_n=0;}
static void cancelPaint(GYOBJ b){(void)b;SceneTexturePaint_Close();if(doneCallback)doneCallback(0,NULL,0,0,doneUser);}
static void paintEvent(GYOBJ c,GYEvent e)
{
    if(e==GY_EVENT_Key){unsigned key=c->ctx->last_key;
        if(key==GY_KEY_TAB){c->ctx->key_handled=1;return;} /* Keep canvas focus out of the scene behind the modal. */
        if(key==GY_KEY_UNDO||key==0x110E||key==27){c->ctx->key_handled=1;if(key==27)cancelPaint(NULL);else history(key==0x110E);return;}}
    canvasEvent(c,e);
}
static void finishPaint(GYOBJ b)
{
    (void)b;GRErgb24* rgb=malloc((size_t)CW*CH*sizeof(*rgb));if(!rgb){YMGUI_Label_SetText(g_status,"内存不足，画板内容已保留");return;}
    composite();GYpx* pixels=YMGUI_Canvas_GetBuffer(g_canvas);
    for(int i=0;i<CW*CH;i++){GYcolor c=GY_PxToColor(pixels[i]);rgb[i]=(GRErgb24){c>>16,c>>8,c};}
    SceneTexturePaint_Close();if(doneCallback)doneCallback(1,rgb,CW,CH,doneUser);free(rgb);
}
static GYOBJ paintLabel(GYOBJ parent,int x,int y,int w,const char* text)
{GYOBJ p=YMGUI_Creat_Label_Creat(parent,x,y,w,24);YMGUI_Label_SetText(p,text);YMGUI_Label_SetTextColor(p,0xFFE0E9F2);return p;}
static void paintButton(GYOBJ p,int x,int y,int w,const char* text,GYbtn_clicked_cb cb)
{GYOBJ b=YMGUI_Creat_Button_Creat(p,x,y,w,32);YMGUI_Button_SetText(b,text);YMGUI_Button_SetClicked(b,cb);}
void SceneTexturePaint_Open(GYCTX ctx,const GRErgb24* seed,unsigned width,unsigned height,SceneTexturePaintDone done,void* user)
{
    SceneTexturePaint_Close();if(dialog)YMGUI_Free_ObjFree(dialog);dialog=NULL;
    doneCallback=done;doneUser=user;g_ctx=ctx;returnFocus=ctx->focus_obj;
    unsigned extent=width>height?width:height;if(!width||!height){width=height=512;extent=512;}
    g_width=extent>512?width*512/extent:width;g_height=extent>512?height*512/extent:height;if(!g_width)g_width=1;if(!g_height)g_height=1;
    initLayers();g_tool=TOOL_BRUSH;g_size=8;g_grain=4;g_poly_n=0;g_start_x=g_start_y=-1;g_dirty=1;
    if(seed)for(int y=0;y<CH;y++)for(int x=0;x<CW;x++){GRErgb24 c=seed[(size_t)(y*height/CH)*width+x*width/CW];int i=y*CW+x;g_layers[0].r[i]=c.R;g_layers[0].g[i]=c.G;g_layers[0].b[i]=c.B;g_layers[0].a[i]=255;}
    dialog=YMGUI_Creat_Obj_Creat(YMGUI_Ctx_GetTopLayer(ctx),0,0,1024,816);YMGUI_Obj_SetBgColor(dialog,0xFF273142);
    paintLabel(dialog,24,24,360,"图案画板 · YMGUI 图层编辑器");
    paintButton(dialog,628,24,168,"取消绘制",cancelPaint);paintButton(dialog,812,24,188,"完成绘制",finishPaint);
    g_picker=YMGUI_Creat_ColorPicker_Creat(dialog,24,112,212,164);YMGUI_ColorPicker_SetChangedCb(g_picker,onColor);YMGUI_ColorPicker_SetColor(g_picker,GY_ARGB(255,g_ink_r,g_ink_g,g_ink_b));
    paintLabel(dialog,24,292,60,"工具");toolDropdown=YMGUI_Creat_Dropdown_Creat(dialog,84,292,152,28);
    const char* tools[]={"笔刷","橡皮","柔和笔刷","喷枪","直线","矩形","三角形","圆形","折线","多边形","填充桶","吸管"};
    for(int i=0;i<TOOL_COUNT;i++)YMGUI_Dropdown_AddOption(toolDropdown,tools[i]);YMGUI_Dropdown_SetSelectedCb(toolDropdown,onTool);
    paintLabel(dialog,24,334,60,"混合");g_blend_dd=YMGUI_Creat_Dropdown_Creat(dialog,84,334,152,28);
    const char* blends[]={"正常","正片叠底","滤色","线性减淡"};for(int i=0;i<BLEND_COUNT;i++)YMGUI_Dropdown_AddOption(g_blend_dd,blends[i]);YMGUI_Dropdown_SetSelectedCb(g_blend_dd,onBlend);
    paintLabel(dialog,24,382,212,"笔刷半径");GYOBJ size=YMGUI_Creat_Slider_Creat(dialog,24,408,212,20);YMGUI_Slider_SetRange(size,1,30);YMGUI_Slider_SetValue(size,g_size);YMGUI_Slider_SetChanged(size,onSize);
    paintLabel(dialog,24,446,212,"喷枪粒度");GYOBJ grain=YMGUI_Creat_Slider_Creat(dialog,24,472,212,20);YMGUI_Slider_SetRange(grain,1,10);YMGUI_Slider_SetValue(grain,g_grain);YMGUI_Slider_SetChanged(grain,onGrain);
    paintLabel(dialog,24,510,212,"图层不透明度");g_opa_slider=YMGUI_Creat_Slider_Creat(dialog,24,536,212,20);YMGUI_Slider_SetRange(g_opa_slider,0,255);YMGUI_Slider_SetValue(g_opa_slider,255);YMGUI_Slider_SetChanged(g_opa_slider,onOpacity);
    paintButton(dialog,24,578,100,"图层显隐",onToggleVis);paintButton(dialog,136,578,100,"清空图层",onClearLayer);
    g_layer_lbl=paintLabel(dialog,24,626,216,"");paintButton(dialog,24,674,100,"撤销",undoPaint);paintButton(dialog,136,674,100,"重做",redoPaint);
    g_canvas=YMGUI_Creat_Canvas_Creat(dialog,264,112,528,560,CW,CH);YMGUI_Canvas_SetZoom(g_canvas,1);YMGUI_Canvas_SetPan(g_canvas,(528-CW)/2,(560-CH)/2);YMGUI_Canvas_SetPaintCb(g_canvas,onPaint);
    canvasEvent=g_canvas->event_cb;g_canvas->event_cb=paintEvent;g_canvas->state|=GY_STATE_Focusable;YMGUI_SetFocus(ctx,g_canvas);
    g_panel=createLayerPanel(dialog);paintLabel(dialog,816,80,184,"图层（最多 4 层）");paintButton(dialog,816,350,84,"新建图层",onAddLayer);paintButton(dialog,912,350,88,"删除图层",onDeleteLayer);
    char text[96];snprintf(text,sizeof(text),"画布 %d × %d · 完成后合成为贴图",CW,CH);paintLabel(dialog,264,688,528,text);
    paintLabel(dialog,264,732,736,"完成绘制后，可在模型上预览，再点击应用贴图。");
    g_status=paintLabel(dialog,264,768,736,extent>512?"原图已等比缩小到 512 像素以内供绘制":"折线与多边形：点击加点，点回起点结束。");
    updateLayerLabel();composite();
}
void SceneTexturePaint_Shutdown(void){SceneTexturePaint_Close();if(dialog)YMGUI_Free_ObjFree(dialog);dialog=NULL;}
