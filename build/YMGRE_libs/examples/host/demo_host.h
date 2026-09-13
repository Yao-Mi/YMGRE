#ifndef YMGRE_DEMO_HOST_H
#define YMGRE_DEMO_HOST_H
#include <stddef.h>

#include "YMGRE_RenderContext.h"
#include "YMGUI_PubDefine.h"
#include "YMGUI_Hal.h"
#include "YMGUI_Obj.h"
#include "YMGUI_Image.h"

#define YMGRE_DEMO_HOST_IMAGE_MAX 16

typedef struct
{
	GYdisp display;
	GYCTX context;
	GYimg image_sources[YMGRE_DEMO_HOST_IMAGE_MAX];
	uint16 image_source_count;
	int frame_limit;
	int frame_count;
	void *input_backend; /* private window/input state owned by GRE */
} YMGRE_DemoHost;

typedef struct
{
	GRE_RenderTarget target;
	int16 x;
	int16 y;
	uint16 width;
	uint16 height;
} YMGRE_DemoView;

int YMGRE_DemoHost_Init(YMGRE_DemoHost *host, uint16 width, uint16 height, uint32 bufferHeight);
GYOBJ YMGRE_DemoHost_AddTarget(YMGRE_DemoHost *host, GRE_RenderTarget target, int16 x, int16 y,
							   uint16 width, uint16 height);
int YMGRE_DemoHost_Step(YMGRE_DemoHost *host, int fps);
int YMGRE_DemoHost_Run(YMGRE_DemoHost *host, int fps);
void YMGRE_DemoHost_Destroy(YMGRE_DemoHost *host);
int YMGRE_DemoHost_Show(uint16 screenWidth, uint16 screenHeight, const YMGRE_DemoView *views,
						uint16 viewCount, int fps);

/* Portable key names: printable keys use lowercase ASCII; no backend headers needed. */
enum
{
	YMGRE_KEY_UP = 256,
	YMGRE_KEY_DOWN,
	YMGRE_KEY_LEFT,
	YMGRE_KEY_RIGHT,
	YMGRE_KEY_HOME,
	YMGRE_KEY_LSHIFT,
	YMGRE_KEY_RSHIFT,
	YMGRE_KEY_ESCAPE = 27,
	YMGRE_KEY_SPACE = 32,
	YMGRE_KEY_TAB = 9
};
typedef struct
{
	int key;
	uint32 held;   /* OR of currently held bindings; aliases release independently. */
	uint32 action; /* One-shot bits on non-repeated keydown. */
	int value;	   /* Optional nonzero selection value; latest keydown wins. */
} YMGRE_DemoKeyBinding;
/* Bind copies up to 64 unique keys. UI-thread call; resets previous input state.
 * Input collection is synchronized inside GRE; clients never need event watches/atomics. */
int YMGRE_DemoHost_BindKeys(YMGRE_DemoHost *host, const YMGRE_DemoKeyBinding *bindings,
							size_t count);
uint32 YMGRE_DemoHost_HeldKeys(YMGRE_DemoHost *host);
uint32 YMGRE_DemoHost_TakeActions(YMGRE_DemoHost *host);
int YMGRE_DemoHost_TakeValue(YMGRE_DemoHost *host);
void YMGRE_DemoHost_ClearInput(YMGRE_DemoHost *host);
/* Regression input goes through the same backend event path as real input. */
int YMGRE_DemoHost_InjectKey(YMGRE_DemoHost *host, int key, int down);
int YMGRE_DemoHost_InjectFocusLost(YMGRE_DemoHost *host);
/* Valid after successful Init; works even when the window lacks keyboard focus. */
int YMGRE_DemoHost_SetTitle(YMGRE_DemoHost *host, const char *title);
const char *YMGRE_DemoHost_LastError(void);
/* Monotonic seconds, also usable for render/physics timing. */
double YMGRE_DemoHost_Time(void);
/* Set the default pixel scale before Init; an explicit environment override wins. */
void YMGRE_DemoHost_DefaultScale(int scale);

#endif
