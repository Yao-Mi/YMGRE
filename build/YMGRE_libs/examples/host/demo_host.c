#include "demo_host.h"

#include "SDL_LCD.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Mem.h"

#include <stdlib.h>
#include <string.h>
#include <SDL.h>

static void destroyInput(YMGRE_DemoHost *host);

int YMGRE_DemoHost_Init(YMGRE_DemoHost *host, uint16 width, uint16 height, uint32 bufferHeight)
{
	if (host == NULL || width == 0 || height == 0 || bufferHeight == 0)
		return 0;
	GY_memset(host, 0, sizeof(*host));
	host->display.hor_res = width;
	host->display.ver_res = height;
	host->display.buf_px_cnt = (uint32)width * bufferHeight;
	host->display.buf1 = (GYpx *)GY_malloc1((size_t)host->display.buf_px_cnt * sizeof(GYpx));
	host->display.buf2 = NULL;
	host->display.user_data = NULL;
	if (host->display.buf1 == NULL)
		return 0;
	int scale = 2;
	const char *scaleText = getenv("YMGRE_WINDOW_SCALE");
	if (scaleText != NULL && atoi(scaleText) > 0)
		scale = atoi(scaleText);
	const char* headless = getenv("YMGRE_HEADLESS");
	if (headless && !strcmp(headless, "1"))
	{
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
		SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
	}
	/* CPU rendering: prevent implicit GLX contexts in the display backend. */
	SDL_setenv("SDL_RENDER_DRIVER", "software", 0);
	SDL_setenv("SDL_FRAMEBUFFER_ACCELERATION", "0", 0);
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
	YMGUI_Obj_SetBgColor(host->context->root, GY_ARGB(0xFF, 0x18, 0x18, 0x20));
	YMGUI_Inject_SetCtx(host->context);
	{
		const char *limit = getenv("YMGRE_MAX_FRAMES");
		host->frame_limit = (limit != NULL) ? atoi(limit) : 0;
	}
	host->frame_count = 0;
	return 1;
}

GYOBJ YMGRE_DemoHost_AddTarget(YMGRE_DemoHost *host, GRE_RenderTarget target, int16 x, int16 y,
							   uint16 width, uint16 height)
{
	if (host == NULL || host->context == NULL || target == NULL)
		return NULL;
	if (host->image_source_count >= YMGRE_DEMO_HOST_IMAGE_MAX)
		return NULL;
	GYOBJ image = YMGUI_Creat_Image_Creat(host->context->root, x, y, width, height);
	if (image == NULL)
		return NULL;
	GYimg *source = &host->image_sources[host->image_source_count++];
	*source = (GYimg){(const GYpx *)target->data, target->width, target->height, 0, 0};
	YMGUI_Image_SetSrc(image, source);
	return image;
}

int YMGRE_DemoHost_Step(YMGRE_DemoHost *host, int fps)
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

int YMGRE_DemoHost_Run(YMGRE_DemoHost *host, int fps)
{
	if (host == NULL || host->context == NULL)
		return 0;
	while (YMGRE_DemoHost_Step(host, fps))
	{
	}
	return 1;
}

void YMGRE_DemoHost_Destroy(YMGRE_DemoHost *host)
{
	if (host == NULL)
		return;
	destroyInput(host);
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

int YMGRE_DemoHost_Show(uint16 screenWidth, uint16 screenHeight, const YMGRE_DemoView *views,
						uint16 viewCount, int fps)
{
	if (views == NULL || viewCount == 0 || viewCount > YMGRE_DEMO_HOST_IMAGE_MAX)
		return 0;
	YMGRE_DemoHost host;
	if (!YMGRE_DemoHost_Init(&host, screenWidth, screenHeight, 40))
		return 0;
	for (uint16 i = 0; i < viewCount; i++)
	{
		if (YMGRE_DemoHost_AddTarget(&host, views[i].target, views[i].x, views[i].y, views[i].width,
									 views[i].height) == NULL)
		{
			YMGRE_DemoHost_Destroy(&host);
			return 0;
		}
	}
	YMGRE_DemoHost_Run(&host, fps);
	YMGRE_DemoHost_Destroy(&host);
	return 1;
}

/* The SDL event watch can run on another thread. All state stays in this backend;
 * application code consumes snapshots and never touches platform event structures. */
typedef struct
{
	SDL_mutex *lock;
	unsigned window;
	size_t count;
	YMGRE_DemoKeyBinding binding[64];
	unsigned char down[64];
	uint32 held, actions;
	int value;
} host_input;
static SDL_Keycode platformKey(int key)
{
	switch (key)
	{
	case YMGRE_KEY_UP:
		return SDLK_UP;
	case YMGRE_KEY_DOWN:
		return SDLK_DOWN;
	case YMGRE_KEY_LEFT:
		return SDLK_LEFT;
	case YMGRE_KEY_RIGHT:
		return SDLK_RIGHT;
	case YMGRE_KEY_HOME:
		return SDLK_HOME;
	case YMGRE_KEY_LSHIFT:
		return SDLK_LSHIFT;
	case YMGRE_KEY_RSHIFT:
		return SDLK_RSHIFT;
	default:
		return key > 0 && key < 128 ? (SDL_Keycode)key : SDLK_UNKNOWN;
	}
}
static int SDLCALL inputWatch(void *data, SDL_Event *e)
{
	host_input *in = data;
	if (e->type == SDL_WINDOWEVENT && e->window.windowID == in->window &&
		e->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
	{
		SDL_LockMutex(in->lock);
		memset(in->down, 0, sizeof in->down);
		in->held = 0;
		SDL_UnlockMutex(in->lock);
		return 1;
	}
	if ((e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) || e->key.windowID != in->window)
		return 1;
	SDL_LockMutex(in->lock);
	for (size_t i = 0; i < in->count; i++)
		if (e->key.keysym.sym == platformKey(in->binding[i].key))
		{
			in->down[i] = e->type == SDL_KEYDOWN;
			if (e->type == SDL_KEYDOWN && !e->key.repeat)
			{
				in->actions |= in->binding[i].action;
				if (in->binding[i].value)
					in->value = in->binding[i].value;
			}
		}
	in->held = 0;
	for (size_t i = 0; i < in->count; i++)
		if (in->down[i])
			in->held |= in->binding[i].held;
	SDL_UnlockMutex(in->lock);
	return 1;
}
int YMGRE_DemoHost_BindKeys(YMGRE_DemoHost *host, const YMGRE_DemoKeyBinding *b, size_t count)
{
	if (!host || !host->context || count > 64 || (count && !b))
		return 0;
	for (size_t i = 0; i < count; i++)
	{
		if (platformKey(b[i].key) == SDLK_UNKNOWN)
			return 0;
		for (size_t j = 0; j < i; j++)
			if (b[j].key == b[i].key)
				return 0;
	}
	host_input *in = host->input_backend;
	if (!in)
	{
		in = calloc(1, sizeof *in);
		if (!in)
			return 0;
		in->window = SDL_LCD_WindowId();
		in->lock = SDL_CreateMutex();
		if (!in->lock || !in->window)
		{
			if (in->lock)
				SDL_DestroyMutex(in->lock);
			free(in);
			return 0;
		}
		host->input_backend = in;
		SDL_AddEventWatch(inputWatch, in);
	}
	SDL_LockMutex(in->lock);
	if (count)
		memcpy(in->binding, b, count * sizeof *b);
	in->count = count;
	in->held = in->actions = 0;
	in->value = 0;
	memset(in->down, 0, sizeof in->down);
	SDL_UnlockMutex(in->lock);
	return 1;
}
uint32 YMGRE_DemoHost_HeldKeys(YMGRE_DemoHost *host)
{
	host_input *in = host ? host->input_backend : NULL;
	if (!in)
		return 0;
	SDL_LockMutex(in->lock);
	uint32 value = in->held;
	SDL_UnlockMutex(in->lock);
	return value;
}
uint32 YMGRE_DemoHost_TakeActions(YMGRE_DemoHost *host)
{
	host_input *in = host ? host->input_backend : NULL;
	if (!in)
		return 0;
	SDL_LockMutex(in->lock);
	uint32 value = in->actions;
	in->actions = 0;
	SDL_UnlockMutex(in->lock);
	return value;
}
int YMGRE_DemoHost_TakeValue(YMGRE_DemoHost *host)
{
	host_input *in = host ? host->input_backend : NULL;
	if (!in)
		return 0;
	SDL_LockMutex(in->lock);
	int value = in->value;
	in->value = 0;
	SDL_UnlockMutex(in->lock);
	return value;
}
void YMGRE_DemoHost_ClearInput(YMGRE_DemoHost *host)
{
	host_input *in = host ? host->input_backend : NULL;
	if (!in)
		return;
	SDL_LockMutex(in->lock);
	in->held = in->actions = 0;
	in->value = 0;
	memset(in->down, 0, sizeof in->down);
	SDL_UnlockMutex(in->lock);
}
static void destroyInput(YMGRE_DemoHost *host)
{
	host_input *in = host->input_backend;
	if (!in)
		return;
	SDL_DelEventWatch(inputWatch, in);
	SDL_DestroyMutex(in->lock);
	free(in);
	host->input_backend = NULL;
}
int YMGRE_DemoHost_InjectKey(YMGRE_DemoHost *host, int key, int down)
{
	if (!host || !host->context || platformKey(key) == SDLK_UNKNOWN)
		return 0;
	SDL_Event e;
	memset(&e, 0, sizeof e);
	e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	e.key.windowID = SDL_LCD_WindowId();
	e.key.keysym.sym = platformKey(key);
	e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	return SDL_PushEvent(&e) == 1;
}
int YMGRE_DemoHost_InjectFocusLost(YMGRE_DemoHost *host)
{
	if (!host || !host->context)
		return 0;
	SDL_Event e;
	memset(&e, 0, sizeof e);
	e.type = SDL_WINDOWEVENT;
	e.window.windowID = SDL_LCD_WindowId();
	e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
	return SDL_PushEvent(&e) == 1;
}
int YMGRE_DemoHost_SetTitle(YMGRE_DemoHost *host, const char *title)
{
	return host && host->context ? SDL_LCD_SetTitle(title) : 0;
}
const char *YMGRE_DemoHost_LastError(void) { return SDL_GetError(); }
double YMGRE_DemoHost_Time(void)
{
	return (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
}
void YMGRE_DemoHost_DefaultScale(int scale)
{
	if (scale > 0)
	{
		char value[24];
		SDL_snprintf(value, sizeof value, "%d", scale);
		SDL_setenv("YMGRE_WINDOW_SCALE", value, 0);
	}
}
