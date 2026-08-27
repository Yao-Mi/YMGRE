#include "demo_host.h"
#include "YMGRE_ScenceManager.h"
#include "YMGUI_Invalidate.h"

typedef struct
{
	YMGRE_DemoHost host;
	GYOBJ images[2];
	int pointerCamera;
	int pointerX;
	int pointerY;
	uint8 pointerActive;
	char key;
	uint8 keyReady;
} scene_app;

static scene_app* s_app;

static void cameraEvent(GYOBJ image, GYEvent event)
{
	if (s_app == NULL)
		return;
	int cameraId = (s_app->images[1] == image) ? 1 : 0;
	if (event == GY_EVENT_Pressed || event == GY_EVENT_Pressing)
	{
		s_app->pointerCamera = cameraId;
		s_app->pointerX = image->ctx->point_x - image->area.x;
		s_app->pointerY = image->ctx->point_y - image->area.y;
		s_app->pointerActive = 1;
	}
	else if (event == GY_EVENT_Released || event == GY_EVENT_ReleasedOff)
		s_app->pointerActive = 0;
	else if (event == GY_EVENT_Key && image->ctx->last_key <= 0xFF)
	{
		s_app->key = (char)image->ctx->last_key;
		s_app->keyReady = 1;
	}
}

static int appStep(void* userData, int fps)
{
	scene_app* app = userData;
	return YMGRE_DemoHost_Step(&app->host, fps);
}

static int appReadKey(void* userData, char* key)
{
	scene_app* app = userData;
	if (!app->keyReady)
		return 0;
	*key = app->key;
	app->keyReady = 0;
	return 1;
}

static int appReadPointer(void* userData, GRE_Camera4d camera, int* x, int* y)
{
	scene_app* app = userData;
	if (!app->pointerActive || app->pointerCamera != camera->ID)
		return 0;
	*x = app->pointerX;
	*y = app->pointerY;
	return 1;
}

static void appPresent(void* userData, GRE_Camera4d camera)
{
	scene_app* app = userData;
	if (camera->ID < 0 || camera->ID >= 2)
		return;
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(camera);
	GYOBJ* image = &app->images[camera->ID];
	if (*image == NULL)
	{
		int16 x = (camera->ID == 0) ? 0 : 500;
		*image = YMGRE_DemoHost_AddTarget(&app->host, target, x, 0,
			target->width, target->height);
		if (*image == NULL)
			return;
		(*image)->event_cb = cameraEvent;
		(*image)->state |= GY_STATE_Focusable;
	}
	YMGUI_Obj_Invalidate(*image);
}

int main(int argc, char** argv)
{
	gre_scence scene = { 0 };
	scene_app app = { 0 };
	if (!YMGRE_DemoHost_Init(&app.host, 800, 600, 40))
		return 1;
	if (!YMGRE_Scene_Init(&scene, (argc > 1) ? argv[1] : "Resource"))
	{
		YMGRE_DemoHost_Destroy(&app.host);
		return 1;
	}
	s_app = &app;
	gre_scene_host sceneHost = {
		&app, appStep, appReadKey, appReadPointer, appPresent
	};
	YMGRE_Scene_Rendering(&scene, &sceneHost);
	s_app = NULL;
	YMGRE_DemoHost_Destroy(&app.host);
	return 0;
}
