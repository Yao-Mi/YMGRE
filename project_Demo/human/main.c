#define _POSIX_C_SOURCE 200809L
#include "YMGUI_Button.h"
#include "YMGUI_Event.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Slider.h"
#include "demo_host.h"
#include "scene.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static demo_scene scene;
static struct
{
	int running, bones, wire, motion, dragging, x, y, refresh;
	float angle, time;
	GYOBJ buttons[8], slider;
} ui;
static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static uint64_t frameHash(void)
{
	GRE_RenderTarget t = YMGRE_Camera_GetRenderTarget(scene.camera);
	uint64_t h = 14695981039346656037ULL;
	const unsigned char *p = (void *)t->data;
	for (size_t i = 0; i < (size_t)t->width * t->height * sizeof(*t->data); ++i)
		h = (h ^ p[i]) * 1099511628211ULL;
	return h;
}
static void command(int k)
{
	switch (k)
	{
	case 27:
		ui.running = 0;
		break;
	case ' ':
		ui.motion = !ui.motion;
		break;
	case 'b':
	case 'B':
		ui.bones = !ui.bones;
		break;
	case 'w':
	case 'W':
		ui.wire = !ui.wire;
		break;
	case 'v':
	case 'V':
		Demo_ViewReset(&scene);
		break;
	case 'r':
	case 'R':
		ui.angle = ui.time = 0;
		ui.motion = 0;
		break;
	case '[':
		scene.selected = scene.selected <= 11 ? (int)scene.asset.info.boneCount - 1 : scene.selected - 1;
		ui.angle = 0;
		ui.motion = 0;
		break;
	case ']':
		scene.selected = scene.selected + 1 >= (int)scene.asset.info.boneCount ? 11 : scene.selected + 1;
		ui.angle = 0;
		ui.motion = 0;
		break;
	case 'x':
	case 'X':
		scene.axis = (scene.axis + 1) % 3;
		break;
	case GY_KEY_LEFT:
		ui.angle = fmaxf(-90, ui.angle - 3);
		ui.motion = 0;
		break;
	case GY_KEY_RIGHT:
		ui.angle = fminf(90, ui.angle + 3);
		ui.motion = 0;
		break;
	}
}
static void onView(GYOBJ o, GYEvent e)
{
	GYCTX c = o->ctx;
	if (e == GY_EVENT_Key)
	{
		command(c->last_key);
		c->key_handled = 1;
	}
	else if (e == GY_EVENT_Pressed)
	{
		ui.dragging = 1;
		ui.x = c->point_x;
		ui.y = c->point_y;
	}
	else if (e == GY_EVENT_Pressing && ui.dragging)
	{
		Demo_ViewMove(&scene, -(c->point_x - ui.x) * .008f, (c->point_y - ui.y) * .008f, 0);
		ui.x = c->point_x;
		ui.y = c->point_y;
	}
	else if (e == GY_EVENT_Released || e == GY_EVENT_ReleasedOff)
		ui.dragging = 0;
	else if (e == GY_EVENT_Wheel)
		Demo_ViewMove(&scene, 0, 0, -c->wheel_y * .09f);
	else if (e == GY_EVENT_DoubleClicked)
		Demo_ViewReset(&scene);
}
static void onButton(GYOBJ o)
{
	const int keys[] = {' ', '[', ']', 'x', 'r', 'v', 'b', 'w'};
	for (int i = 0; i < 8; ++i)
		if (o == ui.buttons[i])
			command(keys[i]);
}
static void onSlider(GYOBJ o, int32 value)
{
	(void)o;
	if (!ui.refresh)
	{
		ui.angle = value * .1f;
		ui.motion = 0;
	}
}
static GYOBJ label(GYOBJ root, int y, const char *t)
{
	GYOBJ o = YMGUI_Creat_Label_Creat(root, 14, y, 932, 24);
	if (o)
	{
		YMGUI_Label_SetText(o, t);
		YMGUI_Label_SetTextColor(o, GY_ARGB(255, 226, 234, 244));
	}
	return o;
}
/* 仅 --capture-ui 截图时拦截宿主分块输出，普通显示没有额外整屏缓存。 */
static FILE *captureFile;
static long captureOffset;
static int captureFailed;
static void (*originalFlush)(GYDISP, const GYrect *, const GYpx *);
static void captureFlush(GYDISP display, const GYrect *area, const GYpx *pixels)
{
	unsigned char row[DEMO_WIDTH * 3];
	for (int y = 0; y < area->h; ++y)
	{
		for (int x = 0; x < area->w; ++x)
		{
			GYcolor color = GY_PxToColor(pixels[y * area->w + x]);
			row[x * 3] = (unsigned char)(color >> 16);
			row[x * 3 + 1] = (unsigned char)(color >> 8);
			row[x * 3 + 2] = (unsigned char)color;
		}
		if (fseek(captureFile, captureOffset + ((long)(area->y + y) * DEMO_WIDTH + area->x) * 3, SEEK_SET) !=
				0 ||
			fwrite(row, 3, (size_t)area->w, captureFile) != (size_t)area->w)
			captureFailed = 1;
	}
	originalFlush(display, area, pixels);
}
static int window(int smoke, int capture)
{
	YMGRE_DemoHost host;
	int result = 1;
	ui.running = 1;
	ui.bones = capture;
	setenv("SDL_RENDER_DRIVER", "software", 1);
	setenv("SDL_FRAMEBUFFER_ACCELERATION", "0", 1);
	setenv("YMGRE_WINDOW_SCALE", "1", 0);
	if (smoke || capture)
		setenv("YMGRE_MAX_FRAMES", "0", 1);
	if (!YMGRE_DemoHost_Init(&host, DEMO_WIDTH, DEMO_HEIGHT + 114, 40))
		return 1;
	GYOBJ view = YMGRE_DemoHost_AddTarget(&host, YMGRE_Camera_GetRenderTarget(scene.camera), 0, 0, DEMO_WIDTH,
										  DEMO_HEIGHT);
	GYOBJ status = label(host.context->root, DEMO_HEIGHT + 48, "HUMAN | Loading");
	if (!view || !status ||
		!label(host.context->root, DEMO_HEIGHT + 80,
			   "[ ] joint | X axis | Arrows angle | Drag orbit | Wheel zoom"))
		goto done;
	view->state |= GY_STATE_Focusable;
	view->event_cb = onView;
	const char *names[] = {"MOTION", "PREV", "NEXT", "AXIS X", "BIND", "VIEW", "BONES", "WIRE"};
	for (int i = 0; i < 8; ++i)
	{
		ui.buttons[i] = YMGUI_Creat_Button_Creat(host.context->root, 14 + i * 76, DEMO_HEIGHT + 8, 70, 30);
		if (!ui.buttons[i])
			goto done;
		YMGUI_Button_SetText(ui.buttons[i], names[i]);
		YMGUI_Button_SetClicked(ui.buttons[i], onButton);
		YMGUI_Button_SetColors(ui.buttons[i], GY_ARGB(255, 41, 55, 73), GY_ARGB(255, 76, 107, 143));
	}
	ui.slider = YMGUI_Creat_Slider_Creat(host.context->root, 640, DEMO_HEIGHT + 10, 302, 26);
	if (!ui.slider)
		goto done;
	YMGUI_Slider_SetRange(ui.slider, -900, 900);
	YMGUI_Slider_SetChanged(ui.slider, onSlider);
	if (capture)
	{
		captureFile = fopen("human_controls.ppm", "wb");
		if (!captureFile)
			goto done;
		fprintf(captureFile, "P6\n%d %d\n255\n", DEMO_WIDTH, DEMO_HEIGHT + 114);
		captureOffset = ftell(captureFile);
		originalFlush = host.display.flush_cb;
		host.display.flush_cb = captureFlush;
	}
	double previous = now();
	uint64_t first = 0;
	int changed = 0;
	for (int frame = 0; ui.running; ++frame)
	{
		YMGUI_SetFocus(host.context, view);
		double start = now();
		float dt = smoke ? 1.f / 60 : (float)(start - previous);
		previous = start;
		if (smoke)
		{
			if (frame == 1)
				YMGUI_Inject_Key(' ', 1);
			if (frame == 3)
			{
				YMGUI_Inject_Key('b', 1);
				if (!ui.bones)
					goto done;
			}
			if (frame == 4)
			{
				YMGUI_Inject_Key('w', 1);
				if (!ui.wire)
					goto done;
			}
			if (frame == 5)
			{
				YMGUI_Inject_Key('r', 1);
				if (ui.angle || ui.motion)
					goto done;
			}
			if (frame == 6)
			{
				float before = scene.orbit;
				YMGUI_Inject_Pointer(100, 100, 1);
				YMGUI_Inject_Pointer(150, 125, 1);
				YMGUI_Inject_Pointer(150, 125, 0);
				if (before == scene.orbit || ui.dragging)
					goto done;
			}
			if (frame == 7)
			{
				float before = scene.distance;
				YMGUI_Inject_Wheel(100, 100, 0, 1);
				if (scene.distance >= before)
					goto done;
			}
			if (frame == 8)
			{
				YMGUI_Inject_Key(']', 1);
				if (scene.selected != 24)
					goto done;
				YMGUI_Inject_Key('x', 1);
				if (scene.axis != 1)
					goto done;
			}
			if (frame == 9)
			{
				YMGUI_Inject_Pointer(900, DEMO_HEIGHT + 22, 1);
				YMGUI_Inject_Pointer(920, DEMO_HEIGHT + 22, 1);
				YMGUI_Inject_Pointer(920, DEMO_HEIGHT + 22, 0);
				if (ui.angle < 20)
					goto done;
			}
			if (frame == 12)
			{
				YMGUI_Inject_Key(27, 1);
				if (!changed)
					goto done;
			}
		}
		if (!ui.running)
			break;
		if (ui.motion)
		{
			ui.time += fminf(dt, .1f);
			ui.angle = 40 * sinf(ui.time * 1.5f);
		}
		if (!Demo_Update(&scene, ui.angle) || !Demo_Validate(&scene))
			goto done;
		Demo_Render(&scene, ui.wire, ui.bones);
		if (smoke)
		{
			uint64_t h = frameHash();
			if (!frame)
				first = h;
			else if (h != first)
				changed = 1;
		}
		char text[220];
		snprintf(text, sizeof(text), "%uV %dT %uB | %.20s | %c %+.0f | %s", scene.asset.info.vertexCount,
				 scene.object->polygonNum, scene.asset.info.boneCount, scene.asset.names[scene.selected].text,
				 "XYZ"[scene.axis], ui.angle, ui.motion ? "TEST" : "MANUAL");
		YMGUI_Label_SetText(status, text);
		YMGUI_Button_SetText(ui.buttons[0], ui.motion ? "PAUSE" : "MOTION");
		char axis[7] = "AXIS X";
		axis[5] = "XYZ"[scene.axis];
		YMGUI_Button_SetText(ui.buttons[3], axis);
		ui.refresh = 1;
		YMGUI_Slider_SetValue(ui.slider, (int32)(ui.angle * 10));
		ui.refresh = 0;
		YMGUI_Obj_Invalidate(view);
		if (capture)
			YMGUI_Obj_Invalidate(host.context->root);
		if (!YMGRE_DemoHost_Step(&host, smoke || capture ? 0 : 60))
		{
			if (smoke)
				goto done;
			break;
		}
		if (capture)
			break;
	}
	result = captureFailed ? 1 : 0;
	if (smoke && !result)
		puts("PASS human window: motion, bind, bones, wire, orbit, zoom, joint, axis, slider, exit");
done:
	if (captureFile)
	{
		host.display.flush_cb = originalFlush;
		if (fclose(captureFile))
			result = 1;
		captureFile = NULL;
	}
	YMGRE_DemoHost_Destroy(&host);
	return result;
}
int main(int argc, char **argv)
{
	const char *root = HUMAN_ASSET_ROOT, *mode = "";
	int text = 0, result = 1;
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "--assets") && i + 1 < argc)
			root = argv[++i];
		else if (!strcmp(argv[i], "--text"))
			text = 1;
		else if (!strcmp(argv[i], "--headless") || !strcmp(argv[i], "--capture-ui") ||
				 !strcmp(argv[i], "--smoke-test"))
			mode = argv[i];
		else
		{
			fprintf(stderr,
					"Usage: %s [--assets directory] [--text] [--headless|--capture-ui|--smoke-test]\n",
					argv[0]);
			return 2;
		}
	}
	if (!Demo_InitFiles(&scene, root, text))
	{
		fprintf(stderr, "Human asset initialization failed\n");
		goto done;
	}
	if (!strcmp(mode, "--headless"))
	{
		if (!Demo_Update(&scene, 0) || !Demo_Validate(&scene))
			goto done;
		Demo_Render(&scene, 0, 0);
		uint64_t first = frameHash();
		if (!Demo_Save(&scene, "human_bind.ppm"))
			goto done;
		if (!Demo_Update(&scene, 45) || !Demo_Validate(&scene))
			goto done;
		Demo_Render(&scene, 0, 0);
		if (first == frameHash() || !Demo_Save(&scene, "human_pose.ppm"))
			goto done;
		Demo_Render(&scene, 0, 1);
		if (!Demo_Save(&scene, "human_bones.ppm"))
			goto done;
		puts("PASS human files, normals, deformation and GRE snapshots");
		result = 0;
	}
	else
		result = window(!strcmp(mode, "--smoke-test"), !strcmp(mode, "--capture-ui"));
done:
	Demo_Destroy(&scene);
	return result;
}
