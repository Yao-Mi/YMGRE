#ifndef YMGRE_DEMO_HOST_H
#define YMGRE_DEMO_HOST_H

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
} YMGRE_DemoHost;

typedef struct
{
	GRE_RenderTarget target;
	int16 x;
	int16 y;
	uint16 width;
	uint16 height;
} YMGRE_DemoView;

int YMGRE_DemoHost_Init(YMGRE_DemoHost* host, uint16 width, uint16 height,
	uint32 bufferHeight);
GYOBJ YMGRE_DemoHost_AddTarget(YMGRE_DemoHost* host, GRE_RenderTarget target,
	int16 x, int16 y, uint16 width, uint16 height);
int YMGRE_DemoHost_Step(YMGRE_DemoHost* host, int fps);
int YMGRE_DemoHost_Run(YMGRE_DemoHost* host, int fps);
void YMGRE_DemoHost_Destroy(YMGRE_DemoHost* host);
int YMGRE_DemoHost_Show(uint16 screenWidth, uint16 screenHeight,
	const YMGRE_DemoView* views, uint16 viewCount, int fps);

#endif
