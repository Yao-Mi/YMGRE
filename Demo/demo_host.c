#include "demo_host.h"

#include "SDL_LCD.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"

#include <stdlib.h>

int YMGRE_DemoHost_Init(YMGRE_DemoHost* host, uint16 width, uint16 height,
	uint32 bufferHeight)
{
	if (host == NULL || width == 0 || height == 0 || bufferHeight == 0)
		return 0;
	GY_memset(host, 0, sizeof(*host));
	host->display.hor_res = width;
	host->display.ver_res = height;
	host->display.buf_px_cnt = (uint32)width * bufferHeight;
	host->display.buf1 = (GYpx*)GY_malloc1(
		(size_t)host->display.buf_px_cnt * sizeof(GYpx));
	host->display.buf2 = NULL;
	host->display.user_data = NULL;
	if (host->display.buf1 == NULL)
		return 0;
	int scale = 2;
	const char* scaleText = getenv("YMGRE_WINDOW_SCALE");
	if (scaleText != NULL && atoi(scaleText) > 0)
		scale = atoi(scaleText);
	if (SDL_LCD_Init(&host->display, scale) != 0)
	{
		GY_free1(host->display.buf1);
		host->display.buf1 = NULL;
		return 0;
	}
	host->context = YMGUI_Creat_Ctx_Creat(&host->display, width, height);
	if (host->context == NULL)
	{
		SDL_LCD_Destroy();
		GY_free1(host->display.buf1);
		host->display.buf1 = NULL;
		return 0;
	}
	YMGUI_Obj_SetBgColor(host->context->root,
		GY_ARGB(0xFF, 0x18, 0x18, 0x20));
	YMGUI_Inject_SetCtx(host->context);
	{
		const char* limit = getenv("YMGRE_MAX_FRAMES");
		host->frame_limit = (limit != NULL) ? atoi(limit) : 0;
	}
	host->frame_count = 0;
	return 1;
}

GYOBJ YMGRE_DemoHost_AddTarget(YMGRE_DemoHost* host, GRE_RenderTarget target,
	int16 x, int16 y, uint16 width, uint16 height)
{
	if (host == NULL || host->context == NULL || target == NULL)
		return NULL;
	if (host->image_source_count >= YMGRE_DEMO_HOST_IMAGE_MAX)
		return NULL;
	GYOBJ image = YMGUI_Creat_Image_Creat(host->context->root, x, y, width, height);
	if (image == NULL)
		return NULL;
	GYimg* source = &host->image_sources[host->image_source_count++];
	*source = (GYimg){
		(const GYpx*)target->data, target->width, target->height, 0, 0
	};
	YMGUI_Image_SetSrc(image, source);
	return image;
}

int YMGRE_DemoHost_Step(YMGRE_DemoHost* host, int fps)
{
	if (host == NULL || host->context == NULL)
		return 0;
	if (!SDL_LCD_PumpEvents())
		return 0;
	YMGUI_Refresh(host->context);
	SDL_LCD_Delay((fps > 0) ? (1000 / fps) : 0);
	host->frame_count++;
	return host->frame_limit <= 0 || host->frame_count < host->frame_limit;
}

int YMGRE_DemoHost_Run(YMGRE_DemoHost* host, int fps)
{
	if (host == NULL || host->context == NULL)
		return 0;
	while (YMGRE_DemoHost_Step(host, fps)) { }
	return 1;
}

void YMGRE_DemoHost_Destroy(YMGRE_DemoHost* host)
{
	if (host == NULL)
		return;
	if (host->context != NULL)
	{
		YMGUI_Free_CtxFree(host->context);
		host->context = NULL;
	}
	SDL_LCD_Destroy();
	GY_free1(host->display.buf1);
	host->display.buf1 = NULL;
	host->image_source_count = 0;
	host->frame_limit = 0;
	host->frame_count = 0;
}

int YMGRE_DemoHost_Show(uint16 screenWidth, uint16 screenHeight,
	const YMGRE_DemoView* views, uint16 viewCount, int fps)
{
	if (views == NULL || viewCount == 0 ||
		viewCount > YMGRE_DEMO_HOST_IMAGE_MAX)
		return 0;
	YMGRE_DemoHost host;
	if (!YMGRE_DemoHost_Init(&host, screenWidth, screenHeight, 40))
		return 0;
	for (uint16 i = 0; i < viewCount; i++)
	{
		if (YMGRE_DemoHost_AddTarget(&host, views[i].target,
			views[i].x, views[i].y, views[i].width, views[i].height) == NULL)
		{
			YMGRE_DemoHost_Destroy(&host);
			return 0;
		}
	}
	YMGRE_DemoHost_Run(&host, fps);
	YMGRE_DemoHost_Destroy(&host);
	return 1;
}
