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
#define LCD_IMAGE_SLOTS 16
typedef struct
{
	GYOBJ image;
	GYimg src;
	GYpx* pixels;
	uint32 pixels_count;
	int x, y, width, height;
} lcd_image_slot;
static lcd_image_slot s_image_slots[LCD_IMAGE_SLOTS];
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
	GY_memset(s_image_slots, 0, sizeof(s_image_slots));
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
	for (int i = 0; i < LCD_IMAGE_SLOTS; i++)
		GY_free1(s_image_slots[i].pixels);
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
	if (s_ctx == NULL || data == NULL || width <= 0 || height <= 0)
		return;
	lcd_image_slot* slot = NULL;
	for (int i = 0; i < LCD_IMAGE_SLOTS; i++)
		if (s_image_slots[i].image != NULL && s_image_slots[i].x == startx &&
			s_image_slots[i].y == starty && s_image_slots[i].width == width &&
			s_image_slots[i].height == height) { slot = &s_image_slots[i]; break; }
	if (slot == NULL)
	{
		for (int i = 0; i < LCD_IMAGE_SLOTS; i++)
			if (s_image_slots[i].image == NULL) { slot = &s_image_slots[i]; break; }
		if (slot == NULL) return;
		slot->image = YMGUI_Creat_Image_Creat(s_ctx->root, (GYcoord)startx,
			(GYcoord)starty, (GYcoord)width, (GYcoord)height);
		if (slot->image == NULL) return;
		slot->image->event_cb = ymguiCameraEvent;
		slot->image->state |= GY_STATE_Focusable;
		YMGUI_Image_SetScaleMode(slot->image, GY_IMG_NONE);
		slot->x = startx; slot->y = starty; slot->width = width; slot->height = height;
	}
#if YMGRE_CAMERA_COLOR_DEPTH == 16
	slot->src.data = (const GYpx*)data;
#else
	uint32 count = (uint32)width * (uint32)height;
	if (slot->pixels == NULL || slot->pixels_count != count)
	{
		GY_free1(slot->pixels);
		slot->pixels = (GYpx*)GY_malloc1((size_t)count * sizeof(GYpx));
		slot->pixels_count = (slot->pixels != NULL) ? count : 0;
	}
	if (slot->pixels == NULL)
		return;
	for (uint32 i = 0; i < count; i++)
		slot->pixels[i] = GRE_RGB24_To_RGB565(data[i]);
	slot->src.data = slot->pixels;
#endif
	slot->src.w = (GYcoord)width;
	slot->src.h = (GYcoord)height;
	slot->src.use_key = 0;
	slot->src.key = 0;
	YMGUI_Image_SetSrc(slot->image, &slot->src);
}
