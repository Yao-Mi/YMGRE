#include "YMGRE_YMGUI_LCD.h"

#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Image.h"
#include "YMGUI_Mem.h"
#include "SDL_LCD.h"
#include <stdlib.h>

#if YMGUI_COLOR_DEPTH != 16
#error "The YMGUI YMGRE port currently requires RGB565 display pixels"
#endif

static GYdisp s_disp;
static GYCTX s_ctx;
static GYOBJ s_main_image;
static GYOBJ s_aux_image;
static GYimg s_main_src;
static GYimg s_aux_src;
static GYpx* s_main_pixels;
static GYpx* s_aux_pixels;
static uint32 s_main_pixels_count;
static uint32 s_aux_pixels_count;
static int s_pointer_x;
static int s_pointer_y;
static uint8 s_pointer_active;
static char s_key_pending;
static uint8 s_key_ready;

static void ymguiCameraEvent(GYOBJ obj, GYEvent event)
{
	if (event == GY_EVENT_Pressed || event == GY_EVENT_Pressing)
	{
		s_pointer_x = obj->ctx->point_x;
		s_pointer_y = obj->ctx->point_y;
		s_pointer_active = 1;
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		s_pointer_active = 0;
	else if (event == GY_EVENT_Key && obj->ctx->last_key <= 0xFF)
	{
		s_key_pending = (char)obj->ctx->last_key;
		s_key_ready = 1;
	}
}
static uint8 s_ready;
static int s_frame_count;
static int s_frame_limit;

void LCD_Init(uint32 width, uint32 height)
{
	s_disp.hor_res = (GYcoord)width;
	s_disp.ver_res = (GYcoord)height;
	s_disp.buf_px_cnt = width * height;
	s_disp.buf1 = (GYpx*)GY_malloc1((size_t)s_disp.buf_px_cnt * sizeof(GYpx));
	s_disp.buf2 = NULL;
	s_disp.flush_busy = 0;
	s_disp.wait_cb = NULL;
	s_disp.user_data = NULL;
	s_frame_count = 0;
	s_pointer_x = 0;
	s_pointer_y = 0;
	s_pointer_active = 0;
	s_key_ready = 0;
	{
		const char* limit = getenv("YMGRE_MAX_FRAMES");
		s_frame_limit = (limit != NULL) ? atoi(limit) : 0;
	}
	if (s_disp.buf1 == NULL || SDL_LCD_Init(&s_disp, 1) != 0)
		return;

	s_ctx = YMGUI_Creat_Ctx_Creat(&s_disp, (GYcoord)width, (GYcoord)height);
	if (s_ctx == NULL)
		return;
	YMGUI_Obj_SetBgColor(s_ctx->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));
	s_main_image = YMGUI_Creat_Image_Creat(s_ctx->root, 0, 0, 500, 500);
	s_aux_image = YMGUI_Creat_Image_Creat(s_ctx->root, 500, 0, 300, 300);
	s_main_image->event_cb = ymguiCameraEvent;
	s_aux_image->event_cb = ymguiCameraEvent;
	s_main_image->state |= GY_STATE_Focusable;
	s_aux_image->state |= GY_STATE_Focusable;
	YMGUI_Image_SetScaleMode(s_main_image, GY_IMG_NONE);
	YMGUI_Image_SetScaleMode(s_aux_image, GY_IMG_NONE);
	YMGUI_Inject_SetCtx(s_ctx);
	s_ready = 1;
}

void LCD_Destory(void)
{
	if (!s_ready)
		return;
	YMGUI_Free_CtxFree(s_ctx);
	s_ctx = NULL;
	SDL_LCD_Destroy();
	GY_free1(s_disp.buf1);
	s_disp.buf1 = NULL;
	GY_free1(s_main_pixels);
	GY_free1(s_aux_pixels);
	s_main_pixels = NULL;
	s_aux_pixels = NULL;
	s_main_pixels_count = 0;
	s_aux_pixels_count = 0;
	s_ready = 0;
}

int LCD_Update(int fps)
{
	if (!s_ready || !SDL_LCD_PumpEvents())
		return 0;
	YMGUI_Refresh(s_ctx);
	SDL_LCD_Delay((fps > 0) ? (1000 / fps) : 0);
	s_frame_count++;
	if (s_frame_limit > 0 && s_frame_count >= s_frame_limit)
		return 0;
	return 1;
}

int LCD_GetXY(int startx, int starty, int rew, int reh, int* px, int* py)
{
	if (!s_pointer_active || s_pointer_x < startx || s_pointer_y < starty ||
		s_pointer_x >= startx + rew || s_pointer_y >= starty + reh)
		return 0;
	*px = s_pointer_x - startx;
	*py = s_pointer_y - starty;
	return 1;
}

int LCD_GetChar(char* c)
{
	if (!s_key_ready)
		return 0;
	*c = s_key_pending;
	s_key_ready = 0;
	return 1;
}

void PAUSE(void) {}

void Delay(int ms)
{
	SDL_LCD_Delay(ms);
}

void LCD_Clear(int color)
{
	(void)color;
	if (s_ctx != NULL)
		YMGUI_Obj_Invalidate(s_ctx->root);
}

void LCD_Fill_RgbRect(int startx, int starty, int width, int height,
                     GRE_FrameBuffer data)
{
	(void)starty;
	GYimg* src = (startx == 500) ? &s_aux_src : &s_main_src;
	GYOBJ image = (startx == 500) ? s_aux_image : s_main_image;
	if (src == NULL || image == NULL || data == NULL)
		return;
	uint32 count = (uint32)width * (uint32)height;
#if YMGRE_CAMERA_COLOR_DEPTH == 16
	src->data = (const GYpx*)data;
#else
	GYpx** pixels = (startx == 500) ? &s_aux_pixels : &s_main_pixels;
	uint32* pixels_count = (startx == 500) ? &s_aux_pixels_count : &s_main_pixels_count;
	if (*pixels == NULL || *pixels_count != count)
	{
		GY_free1(*pixels);
		*pixels = (GYpx*)GY_malloc1((size_t)count * sizeof(GYpx));
		*pixels_count = (*pixels != NULL) ? count : 0;
	}
	if (*pixels == NULL)
		return;
	for (uint32 i = 0; i < count; i++)
		(*pixels)[i] = GRE_RGB24_To_RGB565(data[i]);
	src->data = *pixels;
#endif
	src->w = (GYcoord)width;
	src->h = (GYcoord)height;
	src->use_key = 0;
	src->key = 0;
	YMGUI_Image_SetSrc(image, src);
}
