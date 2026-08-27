#include "demo_host.h"
#include "scene_editor_color.h"
#include "scene_editor_font.h"
#include "scene_editor_hierarchy.h"
#include "scene_editor_import.h"
#include "scene_editor_inspector.h"
#include "scene_editor_io.h"
#include "scene_editor_place.h"

#include "SDL_LCD.h"
#include "YMGUI_Button.h"
#include "YMGUI_Event.h"
#include "YMGUI_Image.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Layout.h"
#include "YMGUI_MsgBox.h"
#include "YMGUI_Obj.h"
#include "YMGUI_TextInput.h"
#include "YMGUI_TreeView.h"
#include "YMGRE_Camera.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"
#include "YMGRE_MathBase.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMCS_File_IO.h"
#include "YMGRE_ScenceManager.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
	VIEW_DRAG_NONE, VIEW_DRAG_PAN, VIEW_DRAG_OBJECT,
	VIEW_DRAG_MOVE_X, VIEW_DRAG_MOVE_Y, VIEW_DRAG_MOVE_Z,
	VIEW_DRAG_ROTATE_X, VIEW_DRAG_ROTATE_Y, VIEW_DRAG_ROTATE_Z, VIEW_DRAG_SCALE
} ViewDragMode;
typedef enum { TRANSFORM_NONE=-1, TRANSFORM_MOVE, TRANSFORM_ROTATE, TRANSFORM_SCALE } TransformMode;
typedef enum { PENDING_NONE, PENDING_NEW, PENDING_OPEN, PENDING_EXIT } PendingAction;

typedef struct {
	YMGRE_DemoHost host;
	GYOBJ viewport, viewportBrand, viewportCamera, status, tree;
	int pointerX, pointerY, frames, frameLimit;
	ViewDragMode dragMode;
	uint8 dragChanged;
} EditorUi;

#define SCENE_HISTORY_CAPACITY 24

static EditorUi* g_ui;
static gre_list g_objectsList, g_lightsList;
static gre_scence g_importContext;
static GRE_RenderWorkspace g_workspace;
static GRE_FramePixel g_colorBuffer[1024 * 540];
static float32 g_depthBuffer[1024 * 540];
static gre_render_target g_target;
static GYOBJ g_image, g_editMenu, g_leftPanel, g_rightPanel, g_centerPanel;
static GYimg g_imageSource;
static gre_line3d g_lines[256];
static uint32 g_lineCount, g_baseLineCount;
static uint8 g_referenceVisible=1;
static int8 g_rotateAxis=-1;
static float32 g_rotateStartAngle;
static int g_rotateStartX;
static GYOBJ g_referenceToggle;
static GYOBJ g_transformButtons[3];
static TransformMode g_transformMode=TRANSFORM_NONE;
static SceneEditorObject g_scene[32];
static SceneEditorObject* g_selected;
static SceneEditorObject* g_mainCamera;
static SceneEditorObject* g_activeCamera;
static SceneEditorObject* g_globalLight;
static GYTREENODE g_objectsNode, g_lightsNode, g_camerasNode;
static GYOBJ g_unsavedPrompt;
static GYOBJ g_sceneOpenError;
static uint8 g_sceneDirty;
static uint8 g_shouldExit;
static PendingAction g_pendingAction;
static char g_currentScenePath[SCENE_EDITOR_PATH_CAPACITY];
static char g_historyPaths[SCENE_HISTORY_CAPACITY][192];
static uint32 g_historyCount,g_historyIndex,g_historySerial;
static int32 g_historySavedIndex=-1;
static uint8 g_historyReady,g_historyRestoring,g_historyBatch;

static void createNewScene(void);
static void completePendingAction(void);
static void historyCommit(void);
static void historyReset(void);
static void undoClicked(GYOBJ button);
static void redoClicked(GYOBJ button);

static void setStatus(const char* text)
{
	if (g_ui != NULL && g_ui->status != NULL) YMGUI_Label_SetText(g_ui->status, text);
}

static void addLine(gre_fvector4d start, gre_fvector4d end, GRErgb24 color)
{
	if (g_lineCount < sizeof(g_lines) / sizeof(g_lines[0]))
		g_lines[g_lineCount++] = (gre_line3d){ start, end, color, 1 };
}

static void addGizmoLine(gre_fvector4d start, gre_fvector4d end, GRErgb24 color)
{
	if (g_lineCount < sizeof(g_lines) / sizeof(g_lines[0]))
		g_lines[g_lineCount++] = (gre_line3d){ start, end, color, 4 };
}

static void addSpotConeLines(const SceneEditorObject* object, GRErgb24 color)
{
	if (object == NULL || object->light == NULL || object->lightType != GRE_SpotLight) return;
	gre_fvector4d direction = {
		object->targetX - object->x,
		object->targetY - object->y,
		object->targetZ - object->z, 0
	};
	float32 coneLength = YMGRE_Fvector4d_Len1(&direction);
	if (!isfinite(coneLength) || coneLength < 0.001f) return;
	YMGRE_Fvector4d_Normalize(&direction);
	gre_fvector4d reference = { 0, 1, 0, 0 };
	if (fabsf(direction.y) > 0.99f) reference = (gre_fvector4d){ 1, 0, 0, 0 };
	gre_fvector4d right, up;
	YMGRE_Fvector4d_CrossToResult(&direction, &reference, &right);
	YMGRE_Fvector4d_Normalize(&right);
	YMGRE_Fvector4d_CrossToResult(&right, &direction, &up);
	YMGRE_Fvector4d_Normalize(&up);
	float32 cosine = object->light->proper.spot.cs_inner_angle;
	if (cosine < 0.001f) cosine = 0.001f;
	if (cosine > 1.0f) cosine = 1.0f;
	float32 radius = coneLength * sqrtf(GREMax(0.0f, 1.0f - cosine * cosine)) / cosine;
	gre_fvector4d apex = { object->x, object->y, object->z, 1 };
	gre_fvector4d center = { object->targetX, object->targetY, object->targetZ, 1 };
	gre_fvector4d ring[8];
	for (uint8 i = 0; i < 8; ++i) {
		float32 angle = (float32)i * (2.0f * YMGRE_Pai / 8.0f);
		float32 c = YMGRE_Cos(angle), s = YMGRE_Sin(angle);
		ring[i] = center;
		ring[i].x += radius * (right.x * c + up.x * s);
		ring[i].y += radius * (right.y * c + up.y * s);
		ring[i].z += radius * (right.z * c + up.z * s);
	}
	for (uint8 i = 0; i < 8; ++i) {
		addLine(ring[i], ring[(i + 1) % 8], color);
		addLine(apex, ring[i], color);
	}
	addLine(apex, center, color);
	float32 marker = GREMax(2.0f, radius * 0.08f);
	addLine((gre_fvector4d){center.x-marker,center.y,center.z,1},
		(gre_fvector4d){center.x+marker,center.y,center.z,1}, color);
	addLine((gre_fvector4d){center.x,center.y-marker,center.z,1},
		(gre_fvector4d){center.x,center.y+marker,center.z,1}, color);
	addLine((gre_fvector4d){center.x,center.y,center.z-marker,1},
		(gre_fvector4d){center.x,center.y,center.z+marker,1}, color);
}

static void addCameraGizmoLines(const SceneEditorObject* object, GRErgb24 color)
{
	if (object == NULL || object->camera == NULL || object->kind != SCENE_OBJECT_CAMERA) return;
	GRE_Camera4d camera = object->camera;
	gre_fvector4d position = { object->x, object->y, object->z, 1 };
	float32 dx = object->targetX - object->x;
	float32 dy = object->targetY - object->y;
	float32 dz = object->targetZ - object->z;
	float32 farDepth = sqrtf(dx * dx + dy * dy + dz * dz);
	if (!isfinite(farDepth) || farDepth < 8.0f) farDepth = 40.0f;
	float32 nearDepth = GREMax(4.0f, farDepth * 0.12f);
	gre_fvector4d right = camera->move.cu;
	gre_fvector4d up = camera->move.cv;
	gre_fvector4d forward = camera->move.cn;
	float32 distance = camera->perspectPlane.Dis;
	if (!isfinite(distance) || distance <= 0.0f) return;
	float32 slopes[4] = {
		camera->perspectPlane.pL / distance, camera->perspectPlane.pR / distance,
		camera->perspectPlane.pD / distance, camera->perspectPlane.pU / distance
	};
	for (uint8 i = 0; i < 4; ++i) if (!isfinite(slopes[i])) return;
	gre_fvector4d corners[8];
	for (uint8 plane = 0; plane < 2; ++plane) {
		float32 depth = plane == 0 ? nearDepth : farDepth;
		for (uint8 corner = 0; corner < 4; ++corner) {
			float32 horizontal = slopes[(corner == 0 || corner == 3) ? 0 : 1] * depth;
			float32 vertical = slopes[(corner < 2) ? 3 : 2] * depth;
			corners[plane * 4 + corner] = (gre_fvector4d){
				position.x + forward.x * depth + right.x * horizontal + up.x * vertical,
				position.y + forward.y * depth + right.y * horizontal + up.y * vertical,
				position.z + forward.z * depth + right.z * horizontal + up.z * vertical, 1
			};
		}
	}
	for (uint8 i = 0; i < 4; ++i) {
		addLine(corners[i], corners[(i + 1) % 4], color);
		addLine(corners[4 + i], corners[4 + (i + 1) % 4], color);
		addLine(corners[i], corners[4 + i], color);
	}
	gre_fvector4d target = { object->targetX, object->targetY, object->targetZ, 1 };
	addLine(position, target, color);
	float32 half = 4.0f;
	gre_fvector4d box[8] = {
		{position.x-half,position.y-half,position.z-half,1}, {position.x+half,position.y-half,position.z-half,1},
		{position.x+half,position.y+half,position.z-half,1}, {position.x-half,position.y+half,position.z-half,1},
		{position.x-half,position.y-half,position.z+half,1}, {position.x+half,position.y-half,position.z+half,1},
		{position.x+half,position.y+half,position.z+half,1}, {position.x-half,position.y+half,position.z+half,1}
	};
	const uint8 edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
	for (uint8 i = 0; i < 12; ++i) addLine(box[edges[i][0]], box[edges[i][1]], color);
}

static void buildReferenceLines(void)
{
	GRErgb24 grid = { 70, 80, 95 };
	for (int coordinate = -120; coordinate <= 120; coordinate += 10) {
		if (coordinate == 0) continue;
		addLine((gre_fvector4d){ -120, 0, coordinate, 1 }, (gre_fvector4d){ 120, 0, coordinate, 1 }, grid);
		addLine((gre_fvector4d){ coordinate, 0, -120, 1 }, (gre_fvector4d){ coordinate, 0, 120, 1 }, grid);
	}
	addLine((gre_fvector4d){ -120, 0, 0, 1 }, (gre_fvector4d){ 120, 0, 0, 1 }, (GRErgb24){ 220, 72, 72 });
	addLine((gre_fvector4d){ 0, 0, 0, 1 }, (gre_fvector4d){ 0, 90, 0, 1 }, (GRErgb24){ 82, 205, 112 });
	addLine((gre_fvector4d){ 0, 0, -120, 1 }, (gre_fvector4d){ 0, 0, 120, 1 }, (GRErgb24){ 70, 135, 235 });
	g_baseLineCount = g_lineCount;
}

static void addSelectionLines(void)
{
	g_lineCount = g_referenceVisible ? g_baseLineCount : 0;
	if (g_selected == NULL || !g_selected->active) return;
	GRErgb24 orange = { 255, 145, 35 };
	if (g_selected->kind == SCENE_OBJECT_MESH && g_selected->mesh != NULL) {
		gre_fvector4d minimum = { FLT_MAX, FLT_MAX, FLT_MAX, 1 };
		gre_fvector4d maximum = { -FLT_MAX, -FLT_MAX, -FLT_MAX, 1 };
		uint32 visiblePoints = 0;
		for (GRE_Object4d part = g_selected->mesh; part != NULL; part = part->nextObject) {
			if (!part->isVisible) continue;
			for (int i = 0; i < part->pointNum; ++i) {
				visiblePoints++;
				gre_fvector4d point = part->pointList[i].pos;
				if (point.x < minimum.x) minimum.x = point.x;
				if (point.x > maximum.x) maximum.x = point.x;
				if (point.y < minimum.y) minimum.y = point.y;
				if (point.y > maximum.y) maximum.y = point.y;
				if (point.z < minimum.z) minimum.z = point.z;
				if (point.z > maximum.z) maximum.z = point.z;
			}
		}
		if (visiblePoints == 0) return;
		gre_fvector4d c[8] = {
			{minimum.x,minimum.y,minimum.z,1}, {maximum.x,minimum.y,minimum.z,1},
			{maximum.x,maximum.y,minimum.z,1}, {minimum.x,maximum.y,minimum.z,1},
			{minimum.x,minimum.y,maximum.z,1}, {maximum.x,minimum.y,maximum.z,1},
			{maximum.x,maximum.y,maximum.z,1}, {minimum.x,maximum.y,maximum.z,1}
		};
		const uint8 edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
		for (int i = 0; i < 12; ++i) addLine(c[edges[i][0]], c[edges[i][1]], orange);
	} else if (g_selected->kind != SCENE_OBJECT_LIGHT || g_selected->lightType != GRE_GlobalLight) {
		float32 size = 8.0f;
		gre_fvector4d center = { g_selected->x, g_selected->y, g_selected->z, 1 };
		addLine((gre_fvector4d){center.x-size,center.y,center.z,1}, (gre_fvector4d){center.x+size,center.y,center.z,1}, orange);
		addLine((gre_fvector4d){center.x,center.y-size,center.z,1}, (gre_fvector4d){center.x,center.y+size,center.z,1}, orange);
		addLine((gre_fvector4d){center.x,center.y,center.z-size,1}, (gre_fvector4d){center.x,center.y,center.z+size,1}, orange);
		addSpotConeLines(g_selected, orange);
		addCameraGizmoLines(g_selected, orange);
	}
}

static float32 transformGizmoSize(void)
{
	if(g_activeCamera==NULL)return 30.0f;
	float32 dx=g_activeCamera->x-g_activeCamera->targetX;
	float32 dy=g_activeCamera->y-g_activeCamera->targetY;
	float32 dz=g_activeCamera->z-g_activeCamera->targetZ;
	float32 size=sqrtf(dx*dx+dy*dy+dz*dz)*0.12f;
	float32 extent=0.0f;
	if(g_selected!=NULL&&g_selected->mesh!=NULL){
		float32 minX=FLT_MAX,minY=FLT_MAX,minZ=FLT_MAX,maxX=-FLT_MAX,maxY=-FLT_MAX,maxZ=-FLT_MAX;
		uint8 found=0;
		for(GRE_Object4d part=g_selected->mesh;part!=NULL;part=part->nextObject) {
			if(!part->isVisible)continue;
			for(int i=0;i<part->pointNum;++i){gre_fvector4d p=part->pointList[i].pos;
				minX=GREMin(minX,p.x);maxX=GREMax(maxX,p.x);minY=GREMin(minY,p.y);maxY=GREMax(maxY,p.y);minZ=GREMin(minZ,p.z);maxZ=GREMax(maxZ,p.z);found=1;}
		}
		if(found)extent=GREMax(maxX-minX,GREMax(maxY-minY,maxZ-minZ));
	}
	if(extent>0.0f)size=GREMax(size,extent*1.15f);
	if(size<24.0f)size=24.0f;if(size>240.0f)size=240.0f;return size;
}

static void addTransformGizmo(void)
{
	if(g_transformMode==TRANSFORM_NONE||g_selected==NULL||!g_selected->active||g_selected->fixed||g_selected==g_activeCamera)return;
	gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1};float32 size=transformGizmoSize();
	if(g_transformMode==TRANSFORM_ROTATE){
		if(g_selected->kind!=SCENE_OBJECT_MESH)return;
		/* 灰色为物体原始坐标轴，彩色环为当前局部坐标轴，旋转过程中保持对照。 */
		addGizmoLine(center,(gre_fvector4d){center.x+size,center.y,center.z,1},(GRErgb24){150,150,150});
		addGizmoLine(center,(gre_fvector4d){center.x,center.y+size,center.z,1},(GRErgb24){150,150,150});
		addGizmoLine(center,(gre_fvector4d){center.x,center.y,center.z+size,1},(GRErgb24){150,150,150});
		for(uint8 axis=0;axis<3;++axis){gre_fvector4d ring[24];
			for(uint8 i=0;i<24;++i){float32 angle=(float32)i*2.0f*YMGRE_Pai/24.0f;ring[i]=center;
				if(axis==0){ring[i].y+=cosf(angle)*size;ring[i].z+=sinf(angle)*size;}
				else if(axis==1){ring[i].x+=cosf(angle)*size;ring[i].z+=sinf(angle)*size;}
				else {ring[i].x+=cosf(angle)*size;ring[i].y+=sinf(angle)*size;}}
			GRErgb24 color=axis==0?(GRErgb24){224,72,72}:(axis==1?(GRErgb24){82,205,112}:(GRErgb24){70,135,235});
			for(uint8 i=0;i<24;++i)addGizmoLine(ring[i],ring[(i+1)%24],color);
			if(g_rotateAxis==(int8)axis){
				float32 degrees=axis==0?g_selected->rotX:(axis==1?g_selected->rotY:g_selected->rotZ);
				float32 radians=degrees*YMGRE_Deg2Rad; int steps=(int)(fabsf(radians)*12.0f/YMGRE_Pai)+1; if(steps>48)steps=48;
				gre_fvector4d previous=center;
				for(int i=0;i<=steps;++i){float32 a=radians*(float32)i/(float32)steps;gre_fvector4d point=center;
					if(axis==0){point.y+=cosf(a)*size*0.82f;point.z+=sinf(a)*size*0.82f;}
					else if(axis==1){point.x+=cosf(a)*size*0.82f;point.z+=sinf(a)*size*0.82f;}
					else {point.x+=cosf(a)*size*0.82f;point.y+=sinf(a)*size*0.82f;}
					if(i>0)addGizmoLine(previous,point,(GRErgb24){245,210,70}); previous=point;
				}
			}
		}
		return;
	}
	if(g_transformMode==TRANSFORM_SCALE&&g_selected->kind!=SCENE_OBJECT_MESH)return;
	gre_fvector4d ends[3]={{center.x+size,center.y,center.z,1},{center.x,center.y+size,center.z,1},{center.x,center.y,center.z+size,1}};
	GRErgb24 colors[3]={{224,72,72},{82,205,112},{70,135,235}};
	for(uint8 axis=0;axis<3;++axis){
		addGizmoLine(center,ends[axis],colors[axis]);
		if(g_transformMode==TRANSFORM_SCALE){float32 marker=size*0.08f;
			addGizmoLine((gre_fvector4d){ends[axis].x-marker,ends[axis].y,ends[axis].z,1},
				(gre_fvector4d){ends[axis].x+marker,ends[axis].y,ends[axis].z,1},colors[axis]);
			addGizmoLine((gre_fvector4d){ends[axis].x,ends[axis].y-marker,ends[axis].z,1},
				(gre_fvector4d){ends[axis].x,ends[axis].y+marker,ends[axis].z,1},colors[axis]);
		}
	}
}

static void updateProjection(GRE_Camera4d camera, uint16 width)
{
	if (camera == NULL) return;
	float32 aspect = (float32)width / g_target.height;
	float32 distance = camera->perspectPlane.Dis;
	camera->perspectPlane.pL = -distance * aspect;
	camera->perspectPlane.pR = distance * aspect;
	camera->perspectPlane.kl = -aspect;
	camera->perspectPlane.kr = aspect;
	camera->img.width = width; camera->img.height = g_target.height;
}

static void updateAllCameraProjections(uint16 width)
{
	for (int i = 0; i < 32; ++i)
		if (g_scene[i].active && g_scene[i].kind == SCENE_OBJECT_CAMERA)
			updateProjection(g_scene[i].camera, width);
}

static float32 cameraDistance(const SceneEditorObject* camera)
{
	float32 dx = camera->x - camera->targetX, dy = camera->y - camera->targetY, dz = camera->z - camera->targetZ;
	return sqrtf(dx*dx + dy*dy + dz*dz);
}

static void syncCamera(SceneEditorObject* camera)
{
	if (camera == NULL || camera->camera == NULL) return;
	float32 distance = cameraDistance(camera);
	if (distance < 0.01f) { camera->z = camera->targetZ - 0.01f; distance = 0.01f; }
	SceneEditorObject_SyncHandle(camera);
	float32 nearPlane = distance * 0.00001f;
	if (nearPlane < 0.001f) nearPlane = 0.001f;
	if (nearPlane > 0.1f) nearPlane = 0.1f;
	YMGRE_Camera_Frustum_Init(camera->camera, nearPlane, distance + 1000.0f);
}

static SceneEditorObject* allocateSceneObject(void)
{
	for (int i = 0; i < 32; ++i)
		if (!g_scene[i].active) { memset(&g_scene[i], 0, sizeof(g_scene[i])); g_scene[i].active = 1; return &g_scene[i]; }
	return NULL;
}

static void createDefaultScene(void)
{
	g_mainCamera = allocateSceneObject();
	snprintf(g_mainCamera->name, sizeof(g_mainCamera->name), "Main Camera");
	snprintf(g_mainCamera->type, sizeof(g_mainCamera->type), "相机");
	g_mainCamera->kind = SCENE_OBJECT_CAMERA;
	g_mainCamera->x = 0; g_mainCamera->y = 149.0f; g_mainCamera->z = -213.0f;
	g_mainCamera->targetX = g_mainCamera->targetY = g_mainCamera->targetZ = 0;
	g_mainCamera->camera = YMGRE_Creat_CameraFromTarget(0, &g_target, 45, 45, 45, 45);
	g_activeCamera = g_mainCamera; updateProjection(g_mainCamera->camera, 560); syncCamera(g_mainCamera);
	g_globalLight = allocateSceneObject();
	snprintf(g_globalLight->name, sizeof(g_globalLight->name), "Global Light");
	snprintf(g_globalLight->type, sizeof(g_globalLight->type), "全局光照");
	g_globalLight->kind = SCENE_OBJECT_LIGHT; g_globalLight->lightType = GRE_GlobalLight;
	g_globalLight->strength = 0.7f; g_globalLight->color = GY_ARGB(0xFF,255,255,255);
	g_globalLight->light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){255,255,255}, 0.7f);
	YMGRE_List_Append(&g_lightsList, sizeof(gre_light4d), g_globalLight->light);
}

static void initScene(void)
{
	memset(g_scene, 0, sizeof(g_scene));
	buildReferenceLines();
	YMGRE_RenderTarget_Init(&g_target, 560, 540, g_colorBuffer, g_depthBuffer);
	g_workspace = YMGRE_Creat_RenderWorkspace();
	createDefaultScene();
}

static void renderScene(void)
{
	if (g_activeCamera == NULL) return;
	syncCamera(g_activeCamera);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(g_activeCamera->camera,
		&g_lightsList, &g_objectsList, &g_importContext.MaterialList, g_workspace);
	addSelectionLines();
	addTransformGizmo();
	YMGRE_Camera_LineList_Rendering(g_activeCamera->camera, g_lines, g_lineCount, 1);
}

static void layoutPanels(void)
{
	int left = (g_leftPanel != NULL && !(g_leftPanel->state & GY_STATE_Hidden)) ? 244 : 0;
	int right = (g_rightPanel != NULL && !(g_rightPanel->state & GY_STATE_Hidden)) ? 212 : 0;
	if (g_centerPanel != NULL) g_centerPanel->area = (GYrect){ left, 64, 1024-left-right, 540 };
	if (g_centerPanel != NULL && g_image != NULL) g_image->area = (GYrect){0,0,g_centerPanel->area.w,540};
	if (g_ui != NULL && g_ui->viewport != NULL) g_ui->viewport->area = (GYrect){0,0,g_centerPanel->area.w,540};
	if (g_ui != NULL && g_ui->viewportCamera != NULL) g_ui->viewportCamera->area.x = g_centerPanel->area.w - 260;
	if (g_centerPanel != NULL) {
		YMGRE_RenderTarget_Init(&g_target, g_centerPanel->area.w, 540, g_colorBuffer, g_depthBuffer);
		g_imageSource.w = g_centerPanel->area.w;
		updateAllCameraProjections(g_centerPanel->area.w);
	}
	if (g_image != NULL) YMGUI_Obj_Invalidate(g_image);
}

static int projectPoint(const gre_fvector4d* world, float32* screenX, float32* screenY, float32* depth)
{
	if (world == NULL || screenX == NULL || screenY == NULL || depth == NULL ||
		g_activeCamera == NULL || g_activeCamera->camera == NULL ||
		g_target.width == 0 || g_target.height == 0) return 0;
	GRE_Camera4d camera = g_activeCamera->camera;
	gre_fvector4d point;
	YMGRE_Fvector4d_MatMultTo(&camera->move.TMat, (GRE_Fvector4d)world, &point);
	if (!isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z) ||
		point.z <= camera->frustum.Znear) return 0;
	float32 viewW = camera->perspectPlane.pR - camera->perspectPlane.pL;
	float32 viewH = camera->perspectPlane.pU - camera->perspectPlane.pD;
	if (!isfinite(viewW) || !isfinite(viewH) || !isfinite(camera->perspectPlane.Dis) ||
		viewW <= 0.0f || viewH <= 0.0f || camera->perspectPlane.Dis <= 0.0f) return 0;
	*screenX = point.x * camera->perspectPlane.Dis / point.z * g_target.width / viewW + g_target.width * 0.5f;
	*screenY = -point.y * camera->perspectPlane.Dis / point.z * g_target.height / viewH + g_target.height * 0.5f;
	*depth = point.z;
	return isfinite(*screenX) && isfinite(*screenY);
}

static float32 pointSegmentDistance(float32 px,float32 py,float32 ax,float32 ay,float32 bx,float32 by)
{
	float32 dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
	float32 t=length>0.0001f?((px-ax)*dx+(py-ay)*dy)/length:0;
	if(t<0)t=0;if(t>1)t=1;dx=px-(ax+t*(bx-ax));dy=py-(ay+t*(by-ay));return sqrtf(dx*dx+dy*dy);
}

static ViewDragMode gizmoDragMode(GYOBJ viewport,GYcoord screenX,GYcoord screenY)
{
	if(g_transformMode==TRANSFORM_NONE||g_selected==NULL||g_selected->fixed||g_selected==g_activeCamera)return VIEW_DRAG_NONE;
	GYrect area;YMGUI_Obj_GetAbsArea(viewport,&area);float32 px=screenX-area.x,py=screenY-area.y;
	gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1};float32 cx,cy,depth;
	if(!projectPoint(&center,&cx,&cy,&depth))return VIEW_DRAG_NONE;
	if(g_transformMode==TRANSFORM_ROTATE){
		if(g_selected->kind!=SCENE_OBJECT_MESH)return VIEW_DRAG_NONE;
		float32 size=transformGizmoSize(),best=FLT_MAX;ViewDragMode result=VIEW_DRAG_NONE;
		for(uint8 axis=0;axis<3;++axis){float32 previousX=0,previousY=0,unused,ringBest=FLT_MAX;uint8 havePrevious=0;
			for(uint8 i=0;i<=24;++i){float32 angle=(float32)(i%24)*2.0f*YMGRE_Pai/24.0f;gre_fvector4d point=center;
				if(axis==0){point.y+=cosf(angle)*size;point.z+=sinf(angle)*size;}else if(axis==1){point.x+=cosf(angle)*size;point.z+=sinf(angle)*size;}else{point.x+=cosf(angle)*size;point.y+=sinf(angle)*size;}
				float32 x,y;if(projectPoint(&point,&x,&y,&unused)){if(havePrevious)ringBest=GREMin(ringBest,pointSegmentDistance(px,py,previousX,previousY,x,y));previousX=x;previousY=y;havePrevious=1;}}
			if(ringBest<best){best=ringBest;result=(ViewDragMode)(VIEW_DRAG_ROTATE_X+axis);}
		}
		return best<=16.0f?result:VIEW_DRAG_NONE;
	}
	if(g_transformMode==TRANSFORM_SCALE&&g_selected->kind!=SCENE_OBJECT_MESH)return VIEW_DRAG_NONE;
	float32 size=transformGizmoSize(),best=FLT_MAX;ViewDragMode result=VIEW_DRAG_NONE;
	gre_fvector4d ends[3]={{center.x+size,center.y,center.z,1},{center.x,center.y+size,center.z,1},{center.x,center.y,center.z+size,1}};
	for(uint8 axis=0;axis<3;++axis){float32 ex,ey;
		if(projectPoint(&ends[axis],&ex,&ey,&depth)){float32 distance=pointSegmentDistance(px,py,cx,cy,ex,ey);
			if(distance<best){best=distance;result=(ViewDragMode)(VIEW_DRAG_MOVE_X+axis);}}}
	if(best>16.0f)return VIEW_DRAG_NONE;
	return g_transformMode==TRANSFORM_SCALE?VIEW_DRAG_SCALE:result;
}

static SceneEditorObject* pickObject(GYOBJ viewport, GYcoord screenX, GYcoord screenY)
{
	GYrect area; YMGUI_Obj_GetAbsArea(viewport, &area);
	float32 px = screenX - area.x, py = screenY - area.y, nearest = FLT_MAX;
	SceneEditorObject* picked = NULL;
	for (int i = 0; i < 32; ++i) {
		SceneEditorObject* object = &g_scene[i];
		if (!object->active || object->fixed ||
			(object->kind == SCENE_OBJECT_LIGHT && object->lightType == GRE_GlobalLight)) continue;
		if (object->mesh != NULL) {
			float32 minimumX=FLT_MAX,minimumY=FLT_MAX,maximumX=-FLT_MAX,maximumY=-FLT_MAX;
			float32 meshDepth=FLT_MAX;uint32 projected=0,visibleParts=0;
			for(GRE_Object4d part=object->mesh;part!=NULL;part=part->nextObject) {
				if (!part->isVisible) continue;
				visibleParts++;
				for(int point=0;point<part->pointNum;++point){
					float32 vx,vy,vz;
					if(!projectPoint(&part->pointList[point].pos,&vx,&vy,&vz))continue;
					minimumX=GREMin(minimumX,vx);maximumX=GREMax(maximumX,vx);
					minimumY=GREMin(minimumY,vy);maximumY=GREMax(maximumY,vy);
					meshDepth=GREMin(meshDepth,vz);projected++;
				}
			}
			if(visibleParts==0)continue;
			if(projected>0){
				float32 padding=6.0f;
				if(px>=minimumX-padding&&px<=maximumX+padding&&py>=minimumY-padding&&py<=maximumY+padding&&meshDepth<nearest){
					nearest=meshDepth;picked=object;
				}
				continue;
			}
		}
		gre_fvector4d center = { object->x, object->y, object->z, 1 };
		float32 sx, sy, depth;
		if (!projectPoint(&center, &sx, &sy, &depth)) continue;
		float32 radius = 11.0f;
		float32 dx = px-sx, dy = py-sy;
		if (dx*dx + dy*dy <= radius*radius && depth < nearest) { nearest = depth; picked = object; }
	}
	return picked;
}

static void selectObject(SceneEditorObject* object, const char* status)
{
	if (object == NULL || !object->active) return;
	g_selected = object;
	SceneEditorInspector_SetObject(object);
	if (object->treeNode != NULL) YMGUI_TreeView_SetSelectedNode(g_ui->tree, object->treeNode);
	setStatus(status);
}

static void clearSelection(const char* status)
{
	g_selected = NULL;
	SceneEditorInspector_SetObject(NULL);
	if (g_ui != NULL && g_ui->tree != NULL)
		YMGUI_TreeView_SetSelectedNode(g_ui->tree, NULL);
	if (g_ui != NULL && g_ui->viewport != NULL)
		YMGUI_Obj_Invalidate(g_ui->viewport);
	setStatus(status);
}

static void rebuildHierarchyTree(void)
{
	YMGUI_TreeView_Clear(g_ui->tree);
	GYTREENODE scene=YMGUI_TreeView_AddNode(g_ui->tree,NULL,"Scene",1);
	g_camerasNode=YMGUI_TreeView_AddNode(g_ui->tree,scene,"Cameras",1);
	g_lightsNode=YMGUI_TreeView_AddNode(g_ui->tree,scene,"Lights",1);
	g_objectsNode=YMGUI_TreeView_AddNode(g_ui->tree,scene,"Objects",1);
	for(int i=0;i<32;++i){
		SceneEditorObject* object=&g_scene[i]; if(!object->active)continue;
		GYTREENODE parent=object->kind==SCENE_OBJECT_CAMERA?g_camerasNode:
			(object->kind==SCENE_OBJECT_LIGHT?g_lightsNode:g_objectsNode);
		object->treeNode=YMGUI_TreeView_AddNode(g_ui->tree,parent,object->name,0);
		YMGUI_TreeView_SetNodeUserPtr(object->treeNode,object);
	}
	YMGUI_TreeView_SetExpanded(g_ui->tree,scene,1);
	YMGUI_TreeView_SetExpanded(g_ui->tree,g_camerasNode,1);
	YMGUI_TreeView_SetExpanded(g_ui->tree,g_lightsNode,1);
	YMGUI_TreeView_SetExpanded(g_ui->tree,g_objectsNode,1);
}

static void orbitCamera(float32 deltaX, float32 deltaY)
{
	SceneEditorObject* camera = g_activeCamera;
	float32 dx=camera->x-camera->targetX, dy=camera->y-camera->targetY, dz=camera->z-camera->targetZ;
	float32 distance=sqrtf(dx*dx+dy*dy+dz*dz);
	float32 yaw=atan2f(dx,-dz)+deltaX*0.35f*YMGRE_Deg2Rad;
	float32 pitch=asinf(dy/distance)+deltaY*0.25f*YMGRE_Deg2Rad;
	float32 limit=85.0f*YMGRE_Deg2Rad;
	if (pitch>limit) pitch=limit;
	if (pitch < -limit) pitch=-limit;
	camera->x=camera->targetX+distance*cosf(pitch)*sinf(yaw);
	camera->y=camera->targetY+distance*sinf(pitch);
	camera->z=camera->targetZ-distance*cosf(pitch)*cosf(yaw);
	syncCamera(camera);
	g_sceneDirty = 1;
}

static void panCamera(float32 deltaX, float32 deltaY)
{
	SceneEditorObject* camera = g_activeCamera;
	float32 scale = cameraDistance(camera) * 0.0015f;
	gre_fvector4d right = camera->camera->move.cu, up = camera->camera->move.cv;
	float32 dx=(-right.x*deltaX+up.x*deltaY)*scale;
	float32 dy=(-right.y*deltaX+up.y*deltaY)*scale;
	float32 dz=(-right.z*deltaX+up.z*deltaY)*scale;
	camera->x+=dx; camera->y+=dy; camera->z+=dz;
	camera->targetX+=dx; camera->targetY+=dy; camera->targetZ+=dz;
	syncCamera(camera);
	g_sceneDirty = 1;
}

static void zoomCamera(float32 factor)
{
	SceneEditorObject* camera = g_activeCamera;
	camera->x=camera->targetX+(camera->x-camera->targetX)*factor;
	camera->y=camera->targetY+(camera->y-camera->targetY)*factor;
	camera->z=camera->targetZ+(camera->z-camera->targetZ)*factor;
	syncCamera(camera);
	g_sceneDirty = 1;
}

static void dragSelected(float32 deltaX, float32 deltaY)
{
	if (g_selected == NULL || g_selected->fixed ||
		(g_selected->kind == SCENE_OBJECT_LIGHT && g_selected->lightType == GRE_GlobalLight)) return;
	if (!isfinite(deltaX) || !isfinite(deltaY)) return;
	gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1};
	float32 sx,sy,depth;
	if (!projectPoint(&center,&sx,&sy,&depth)) return;
	GRE_Camera4d camera=g_activeCamera->camera;
	float32 viewW=camera->perspectPlane.pR-camera->perspectPlane.pL;
	float32 viewH=camera->perspectPlane.pU-camera->perspectPlane.pD;
	float32 unitsX=depth*viewW/(camera->perspectPlane.Dis*g_target.width);
	float32 unitsY=depth*viewH/(camera->perspectPlane.Dis*g_target.height);
	gre_fvector4d right=camera->move.cu, up=camera->move.cv;
	if (!isfinite(unitsX) || !isfinite(unitsY) ||
		!isfinite(right.x) || !isfinite(right.y) || !isfinite(right.z) ||
		!isfinite(up.x) || !isfinite(up.y) || !isfinite(up.z)) return;
	SceneEditorObject_Translate(g_selected,
		right.x*deltaX*unitsX-up.x*deltaY*unitsY,
		right.y*deltaX*unitsX-up.y*deltaY*unitsY,
		right.z*deltaX*unitsX-up.z*deltaY*unitsY);
	SceneEditorInspector_SetObject(g_selected);
	g_sceneDirty = 1;
}

static void dragSelectedAxis(ViewDragMode mode,float32 deltaX,float32 deltaY)
{
	if(g_selected==NULL||g_selected->fixed)return;
	if(mode>=VIEW_DRAG_ROTATE_X&&mode<=VIEW_DRAG_ROTATE_Z&&g_selected->kind==SCENE_OBJECT_MESH){
		uint8 axis=(uint8)(mode-VIEW_DRAG_ROTATE_X);
		SceneEditorObject_SetRotationAxis(g_selected,axis,g_rotateStartAngle+deltaX*0.6f);
	}else if(mode==VIEW_DRAG_SCALE&&g_selected->kind==SCENE_OBJECT_MESH){
		float32 scale=g_selected->scale*expf((deltaX-deltaY)*0.01f);
		if(scale<0.01f)scale=0.01f;if(scale>1000.0f)scale=1000.0f;SceneEditorObject_SetScale(g_selected,scale);
	}else if(mode>=VIEW_DRAG_MOVE_X&&mode<=VIEW_DRAG_MOVE_Z){
		uint8 axis=(uint8)(mode-VIEW_DRAG_MOVE_X);float32 size=transformGizmoSize();
		gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1},end=center;
		if(axis==0)end.x+=size;else if(axis==1)end.y+=size;else end.z+=size;
		float32 cx,cy,ex,ey,depth;
		if(!projectPoint(&center,&cx,&cy,&depth)||!projectPoint(&end,&ex,&ey,&depth))return;
		float32 sx=ex-cx,sy=ey-cy,length=sx*sx+sy*sy;if(length<0.001f)return;
		float32 amount=(deltaX*sx+deltaY*sy)/length*size;
		SceneEditorObject_Translate(g_selected,axis==0?amount:0,axis==1?amount:0,axis==2?amount:0);
	}else return;
	SceneEditorInspector_SetObject(g_selected);g_sceneDirty=1;
}

static void refreshCameraInspector(void)
{
	if (g_selected == g_activeCamera) SceneEditorInspector_SetObject(g_selected);
}

static void viewportEvent(GYOBJ object, GYEvent event)
{
	if (g_ui == NULL) return;
	GYcoord x=object->ctx->point_x, y=object->ctx->point_y;
	if (event == GY_EVENT_Pressed) {
		YMGUI_SetFocus(object->ctx, object);
		g_ui->pointerX=x; g_ui->pointerY=y;g_ui->dragChanged=0;
		ViewDragMode gizmoMode=gizmoDragMode(object,x,y);
		if(gizmoMode!=VIEW_DRAG_NONE){g_ui->dragMode=gizmoMode;
			if(gizmoMode>=VIEW_DRAG_ROTATE_X&&gizmoMode<=VIEW_DRAG_ROTATE_Z){g_rotateAxis=(int8)(gizmoMode-VIEW_DRAG_ROTATE_X);g_rotateStartX=x;g_rotateStartAngle=g_rotateAxis==0?g_selected->rotX:(g_rotateAxis==1?g_selected->rotY:g_selected->rotZ);}
			setStatus(gizmoMode>=VIEW_DRAG_ROTATE_X&&gizmoMode<=VIEW_DRAG_ROTATE_Z?"拖动旋转参考轴":
			(gizmoMode==VIEW_DRAG_SCALE?"拖动操作轴统一缩放":"拖动操作轴约束移动"));}
		else{SceneEditorObject* picked=pickObject(object,x,y);
			if(picked!=NULL){selectObject(picked,"已在视口中选中对象");g_rotateAxis=-1;g_ui->dragMode=(g_transformMode==TRANSFORM_NONE||g_transformMode==TRANSFORM_MOVE)?VIEW_DRAG_OBJECT:VIEW_DRAG_NONE;}
			else{clearSelection("已取消选择；拖动空白区域可平移视图");g_ui->dragMode=VIEW_DRAG_PAN;}}
	} else if (event == GY_EVENT_Pressing) {
		float32 dx=x-g_ui->pointerX, dy=y-g_ui->pointerY;
		if (g_ui->dragMode==VIEW_DRAG_OBJECT) { dragSelected(dx,dy);g_ui->dragChanged|=dx!=0||dy!=0;setStatus("正在移动选中对象"); }
		else if (g_ui->dragMode==VIEW_DRAG_PAN) { panCamera(dx,dy);g_ui->dragChanged|=dx!=0||dy!=0;refreshCameraInspector();setStatus("正在平移视图"); }
		else if(g_ui->dragMode>=VIEW_DRAG_MOVE_X){
			if(g_ui->dragMode>=VIEW_DRAG_ROTATE_X&&g_ui->dragMode<=VIEW_DRAG_ROTATE_Z)
				dragSelectedAxis(g_ui->dragMode,(float32)(x-g_rotateStartX),dy);
			else dragSelectedAxis(g_ui->dragMode,dx,dy);
			g_ui->dragChanged|=dx!=0||dy!=0;setStatus("正在使用变换操作轴");}
		g_ui->pointerX=x; g_ui->pointerY=y; YMGUI_Obj_Invalidate(object);
	} else if (event == GY_EVENT_ContextRequested) {
		uint8 orbitSelection=g_selected!=NULL&&g_selected!=g_activeCamera&&
			!(g_selected->kind==SCENE_OBJECT_LIGHT&&g_selected->lightType==GRE_GlobalLight);
		if(orbitSelection){
			float32 dx=g_activeCamera->x-g_selected->x,dy=g_activeCamera->y-g_selected->y;
			float32 dz=g_activeCamera->z-g_selected->z;
			if(dx*dx+dy*dy+dz*dz<=0.0001f)orbitSelection=0;
		}
		if(orbitSelection){
			g_activeCamera->targetX=g_selected->x;
			g_activeCamera->targetY=g_selected->y;
			g_activeCamera->targetZ=g_selected->z;
		}else{
			g_activeCamera->targetX=0;g_activeCamera->targetY=0;g_activeCamera->targetZ=0;
		}
			syncCamera(g_activeCamera);
			g_sceneDirty = 1;
			g_ui->dragChanged=1;
		g_ui->pointerX=x;g_ui->pointerY=y;
		setStatus(orbitSelection?"右键拖动绕选中对象旋转":"右键拖动绕世界原点旋转");
	} else if (event == GY_EVENT_ContextDragging) {
		orbitCamera(x-g_ui->pointerX,y-g_ui->pointerY); refreshCameraInspector();
		g_ui->dragChanged=1;
		g_ui->pointerX=x; g_ui->pointerY=y; YMGUI_Obj_Invalidate(object);
	} else if (event == GY_EVENT_Wheel) {
		float32 steps=(float32)object->ctx->wheel_y;
		if(steps>8)steps=8;else if(steps<-8)steps=-8;
		if(steps!=0){
			zoomCamera(expf(-0.16f*steps));refreshCameraInspector();YMGUI_Obj_Invalidate(object);
			historyCommit();setStatus(steps>0?"视图拉近":"视图拉远");
		}
	} else if (event == GY_EVENT_Key && object->ctx->last_key=='+') {
		zoomCamera(expf(-0.16f));historyCommit();refreshCameraInspector();YMGUI_Obj_Invalidate(object);setStatus("视图拉近");
	} else if (event == GY_EVENT_Key && object->ctx->last_key=='-') {
		zoomCamera(expf(0.16f));historyCommit();refreshCameraInspector();YMGUI_Obj_Invalidate(object);setStatus("视图拉远");
	} else if (event == GY_EVENT_Key && object->ctx->last_key==GY_KEY_UNDO) {
		undoClicked(NULL);
	} else if (event == GY_EVENT_Key && object->ctx->last_key==GY_KEY_REDO) {
		redoClicked(NULL);
	} else if (event==GY_EVENT_Released || event==GY_EVENT_ReleasedOff ||
		event==GY_EVENT_ContextReleased || event==GY_EVENT_ContextCancelled){
		if(g_ui->dragChanged)historyCommit();g_ui->dragChanged=0;g_ui->dragMode=VIEW_DRAG_NONE;
	}
}

static void treeSelect(GYOBJ tree, GYTREENODE node)
{
	(void)tree;
	SceneEditorObject* object=YMGUI_TreeView_NodeUserPtr(node);
	if (object != NULL) selectObject(object,"已切换场景选中项");
}

static void treeContext(GYOBJ tree, GYTREENODE node)
{
	(void)tree;
	SceneEditorObject* object=YMGUI_TreeView_NodeUserPtr(node);
	if (object != NULL) SceneEditorHierarchy_Open(object,g_ui->host.context->point_x,
		g_ui->host.context->point_y,object!=g_mainCamera&&object!=g_globalLight);
}

static void normalizeSpotDirection(SceneEditorObject* object)
{
	if (object == NULL || object->light == NULL || object->lightType != GRE_SpotLight) return;
	float32 x=object->targetX-object->x, y=object->targetY-object->y, z=object->targetZ-object->z;
	float32 length=sqrtf(x*x+y*y+z*z); if (length < 0.001f) length=1.0f;
	object->light->proper.spot.direct=(gre_fvector4d){x/length,y/length,z/length,0};
}

static void objectPlaced(const SceneEditorPlaceResult* result, void* userData)
{
	(void)userData;
	SceneEditorObject* object=allocateSceneObject();
	if (object==NULL) { if(result->mesh!=NULL)YMGRE_Free_Object(result->mesh); setStatus("场景对象数量已满"); return; }
	snprintf(object->name,sizeof(object->name),"%s",result->name);
	snprintf(object->type,sizeof(object->type),"%s",result->type);
	object->x=result->x; object->y=result->y; object->z=result->z;
	object->targetX=result->targetX; object->targetY=result->targetY; object->targetZ=result->targetZ;
	object->scale=result->scale; object->rotY=result->rotationY; object->strength=result->strength;
	object->color=result->color; object->visible=1; object->wireframe=result->wireframe;
	object->shadowsEnabled=result->shadowsEnabled;
	if (result->kind <= SCENE_PLACE_CAPSULE) {
		object->kind=SCENE_OBJECT_MESH; object->mesh=result->mesh;
		object->primitiveKind=(uint8)result->kind; object->detailA=result->detailA; object->detailB=result->detailB;
		YMGRE_List_Append(&g_objectsList,sizeof(gre_object4d),object->mesh);
		object->treeNode=YMGUI_TreeView_AddNode(g_ui->tree,g_objectsNode,object->name,0);
		YMGUI_TreeView_SetExpanded(g_ui->tree,g_objectsNode,1);
	} else if (result->kind == SCENE_PLACE_POINT_LIGHT || result->kind == SCENE_PLACE_SPOT_LIGHT) {
		object->kind=SCENE_OBJECT_LIGHT;
		object->lightType=result->kind==SCENE_PLACE_POINT_LIGHT?GRE_PointLight:GRE_SpotLight;
		GRErgb24 rgb={(uint8)(result->color>>16),(uint8)(result->color>>8),(uint8)result->color};
		object->light=YMGRE_Creat_Light((int16)(object-g_scene),object->lightType,rgb,result->strength);
		SceneEditorObject_SyncHandle(object); normalizeSpotDirection(object);
		YMGRE_List_Append(&g_lightsList,sizeof(gre_light4d),object->light);
		object->treeNode=YMGUI_TreeView_AddNode(g_ui->tree,g_lightsNode,object->name,0);
		YMGUI_TreeView_SetExpanded(g_ui->tree,g_lightsNode,1);
	} else {
		object->kind=SCENE_OBJECT_CAMERA;
		object->camera=YMGRE_Creat_CameraFromTarget((int16)(object-g_scene),&g_target,45,45,45,45);
		updateProjection(object->camera,g_target.width); syncCamera(object);
		object->treeNode=YMGUI_TreeView_AddNode(g_ui->tree,g_camerasNode,object->name,0);
		YMGUI_TreeView_SetExpanded(g_ui->tree,g_camerasNode,1);
	}
	YMGUI_TreeView_SetNodeUserPtr(object->treeNode,object);
	selectObject(object,"对象已放入场景");
	g_sceneDirty = 1;historyCommit();
}

static int removeListItem(GRE_List list, void* data, listNodeDataFree freeData)
{
	int index=0;
	for (GRE_ListNode node=list->listhead; node!=NULL; node=node->next,index++)
		if (node->data==data) { YMGRE_List_Remove(list,index,freeData); return 1; }
	return 0;
}

static void renameObject(SceneEditorObject* object, const char* name, void* userData)
{
	(void)userData;
	snprintf(object->name,sizeof(object->name),"%s",name);
	YMGUI_TreeView_SetNodeName(g_ui->tree,object->treeNode,object->name);
	SceneEditorInspector_SetObject(object); setStatus("对象已重命名");
	g_sceneDirty = 1;historyCommit();
}

static void switchCamera(SceneEditorObject* object, void* userData)
{
	(void)userData;
	if (object==NULL || object->kind!=SCENE_OBJECT_CAMERA) return;
	g_activeCamera=object; syncCamera(object);
	char text[96]; snprintf(text,sizeof(text),"VIEWPORT  /  %.20s",object->name);
	YMGUI_Label_SetText(g_ui->viewportCamera,text); selectObject(object,"已切换相机视角");
	g_sceneDirty = 1;historyCommit();
}

static void deleteObject(SceneEditorObject* object, void* userData)
{
	(void)userData;
	if (object==NULL || object==g_mainCamera || object==g_globalLight) return;
	if (object==g_activeCamera){g_historyBatch++;switchCamera(g_mainCamera,NULL);g_historyBatch--;}
	if (object->kind==SCENE_OBJECT_MESH) removeListItem(&g_objectsList,object->mesh,YMGRE_Free_Object);
	else if (object->kind==SCENE_OBJECT_LIGHT) removeListItem(&g_lightsList,object->light,YMGRE_Free_Light);
	else if (object->camera!=NULL) YMGRE_Free_Camera(object->camera);
	YMGUI_TreeView_RemoveNode(g_ui->tree,object->treeNode);
	memset(object,0,sizeof(*object));
	clearSelection("对象已删除");
	g_sceneDirty = 1;historyCommit();
}

static void inspectorChanged(const char* status, void* userData)
{
	(void)userData;
	if (g_selected!=NULL) { normalizeSpotDirection(g_selected); if(g_selected->kind==SCENE_OBJECT_CAMERA)syncCamera(g_selected); }
	setStatus(status); if(g_image!=NULL)YMGUI_Obj_Invalidate(g_image);
	g_sceneDirty = 1;historyCommit();
}

static uint8 importMesh(const char* path, uint8 wireframe, void* userData)
{
	(void)userData;
	if (path == NULL) return 0;
	size_t length = strlen(path);
	if (length < 6 || strcmp(path + length - 5, ".mesh") != 0 || length + 4 >= 4096) return 0;
	char materialPath[4096];
	snprintf(materialPath, sizeof(materialPath), "%s", path);
	snprintf(materialPath + length - 5, sizeof(materialPath) - length + 5, ".material");
	if (access(path, R_OK) != 0 || access(materialPath, R_OK) != 0) return 0;
	SceneEditorObject* object = allocateSceneObject();
	if (object == NULL) { setStatus("场景对象数量已满"); return 0; }
	GRE_Object4d mesh = YMGRE_LoadOgreMeshAndMaterial(&g_importContext, path);
	if (mesh == NULL) { memset(object, 0, sizeof(*object)); return 0; }
	float32 minimumY = mesh->pointList[0].pos.y;
	for (GRE_Object4d part = mesh; part != NULL; part = part->nextObject)
		for (int i = 0; i < part->pointNum; ++i)
			minimumY = GREMin(minimumY, part->pointList[i].pos.y);
	SceneEditorPlace_TransformMesh(mesh, 0, -minimumY, 0, 0, 1, wireframe);
	object->kind = SCENE_OBJECT_MESH; object->primitiveKind = 0xFF;
	object->mesh = mesh; object->scale = 1; object->visible = 1; object->wireframe = wireframe;
	object->x = 0; object->y = -minimumY; object->z = 0;
	object->color = GY_ARGB(0xFF, 128, 128, 128);
	if(realpath(path,object->sourcePath)==NULL)
		snprintf(object->sourcePath, sizeof(object->sourcePath), "%s", path);
	snprintf(object->name, sizeof(object->name), "%s", mesh->objName != NULL ? mesh->objName : "Imported Mesh");
	snprintf(object->type, sizeof(object->type), "外部网格");
	YMGRE_List_Append(&g_objectsList, sizeof(gre_object4d), mesh);
	object->treeNode = YMGUI_TreeView_AddNode(g_ui->tree, g_objectsNode, object->name, 0);
	YMGUI_TreeView_SetNodeUserPtr(object->treeNode, object);
	YMGUI_TreeView_SetExpanded(g_ui->tree, g_objectsNode, 1);
	selectObject(object, "外部网格已导入");
	g_sceneDirty = 1;historyCommit();
	return 1;
}

static uint8 inspectorRemesh(SceneEditorObject* object, uint16 detailA, uint16 detailB,
	void* userData)
{
	(void)userData;
	if (object == NULL || object->kind != SCENE_OBJECT_MESH ||
		(object->primitiveKind != SCENE_PLACE_PLANE &&
		(object->primitiveKind < SCENE_PLACE_SPHERE || object->primitiveKind > SCENE_PLACE_CAPSULE)))
		return 0;
	SceneEditorPlaceResult result = { .kind = (SceneEditorPlaceKind)object->primitiveKind,
		.scale = object->scale, .rotationY = object->rotY, .detailA = detailA, .detailB = detailB,
		.color = object->color, .wireframe = object->wireframe };
	snprintf(result.name, sizeof(result.name), "%s", object->name);
	GRE_Object4d replacement = SceneEditorPlace_CreateMesh(&result);
	if (replacement == NULL) return 0;
	SceneEditorPlace_TransformMesh(replacement, object->x, object->y, object->z,
		object->rotY, object->scale, object->wireframe);
	GRE_Object4d previous = object->mesh;
	uint8 replaced = 0;
	for (GRE_ListNode node = g_objectsList.listhead; node != NULL; node = node->next)
		if (node->data == previous) { node->data = replacement; replaced = 1; break; }
	if (!replaced) { YMGRE_Free_Object(replacement); return 0; }
	object->mesh = replacement;
	object->detailA = detailA;
	object->detailB = detailB;
	YMGRE_Free_Object(previous);
	if (g_image != NULL) YMGUI_Obj_Invalidate(g_image);
	g_sceneDirty = 1;
	return 1;
}

static void duplicateObject(SceneEditorObject* source, void* userData)
{
	(void)userData;
	if(source==NULL){setStatus("请先选择要复制的对象");return;}
	if(source==g_globalLight){setStatus("全局光照不能复制");return;}
	char copyName[64];snprintf(copyName,sizeof(copyName),"%.55s Copy",source->name);
	g_historyBatch++;
	if(source->kind==SCENE_OBJECT_MESH&&source->primitiveKind==0xFF){
		if(!importMesh(source->sourcePath,source->wireframe,NULL)){g_historyBatch--;setStatus("复制失败：外部网格无法重新加载");return;}
		SceneEditorObject* copy=g_selected;
		SceneEditorObject_Translate(copy,source->x+10-copy->x,source->y-copy->y,source->z+10-copy->z);
		SceneEditorObject_SetScale(copy,source->scale);SceneEditorObject_SetRotationY(copy,source->rotY);
		copy->color=source->color;copy->visible=source->visible;copy->fixed=source->fixed;
		for(GRE_Object4d part=copy->mesh;part!=NULL;part=part->nextObject)part->isVisible=copy->visible;
		snprintf(copy->name,sizeof(copy->name),"%s",copyName);YMGUI_TreeView_SetNodeName(g_ui->tree,copy->treeNode,copy->name);
		SceneEditorInspector_SetObject(copy);
	}else{
		SceneEditorPlaceResult result={0};
		result.kind=source->kind==SCENE_OBJECT_CAMERA?SCENE_PLACE_CAMERA:
			(source->kind==SCENE_OBJECT_LIGHT?(source->lightType==GRE_SpotLight?SCENE_PLACE_SPOT_LIGHT:SCENE_PLACE_POINT_LIGHT):
			(SceneEditorPlaceKind)source->primitiveKind);
		result.x=source->x+10;result.y=source->y;result.z=source->z+10;
		result.targetX=source->targetX;result.targetY=source->targetY;result.targetZ=source->targetZ;
		result.scale=source->scale;result.rotationY=source->rotY;result.strength=source->strength;
		result.color=source->color;result.wireframe=source->wireframe;result.shadowsEnabled=source->shadowsEnabled;
		result.detailA=source->detailA;result.detailB=source->detailB;
		snprintf(result.name,sizeof(result.name),"%s",copyName);snprintf(result.type,sizeof(result.type),"%s",source->type);
		if(source->kind==SCENE_OBJECT_MESH){
			result.mesh=SceneEditorPlace_CreateMesh(&result);
			if(result.mesh==NULL){g_historyBatch--;setStatus("复制失败：无法创建网格");return;}
			SceneEditorPlace_TransformMesh(result.mesh,result.x,result.y,result.z,result.rotationY,result.scale,result.wireframe);
		}
		objectPlaced(&result,NULL);
		if(g_selected!=NULL)g_selected->fixed=source->fixed;
	}
	g_historyBatch--;g_sceneDirty=1;historyCommit();setStatus("对象已复制");
}

static int writeCurrentScene(const char* path,char* error,size_t errorCapacity)
{
	SceneEditorObject compact[32]; uint32 count=0;
	int32 mainIndex=-1,activeIndex=-1;
	for(int i=0;i<32;++i)if(g_scene[i].active){
		if(&g_scene[i]==g_mainCamera)mainIndex=(int32)count;
		if(&g_scene[i]==g_activeCamera)activeIndex=(int32)count;
		compact[count++]=g_scene[i];
	}
	return SceneEditorIo_Write(path,compact,count,mainIndex,activeIndex,error,errorCapacity);
}

static void saveSceneToPath(const char* path, void* userData)
{
	(void)userData;char error[192];
	if(!writeCurrentScene(path,error,sizeof(error))){
		g_pendingAction=PENDING_NONE;SceneEditorIo_SetError(error);setStatus("场景保存失败，当前场景已保留");return;
	}
	snprintf(g_currentScenePath,sizeof(g_currentScenePath),"%s",path);g_sceneDirty=0;
	if(g_historyReady)g_historySavedIndex=(int32)g_historyIndex;
	SceneEditorIo_Close();char status[192];snprintf(status,sizeof(status),"场景已保存：%.150s",path);setStatus(status);
	completePendingAction();
}

static void freeStagedScene(SceneEditorObject* objects, uint32 count, GRE_List objectList,
	GRE_List lightList, gre_scence* importContext)
{
	for(uint32 i=0;i<count;++i)if(objects[i].kind==SCENE_OBJECT_CAMERA&&objects[i].camera!=NULL)
		YMGRE_Free_Camera(objects[i].camera);
	YMGRE_List_Clear(lightList,YMGRE_Free_Light);YMGRE_List_Clear(objectList,YMGRE_Free_Object);
	YMGRE_List_Clear(&importContext->MaterialList,YMGRE_Free_Material);
}

static void reopenSceneDialog(GYOBJ messageBox, int index)
{
	(void)messageBox;(void)index;SceneEditorIo_OpenLoad();
}

static void showSceneOpenError(const char* message)
{
	if(g_sceneOpenError==NULL){fprintf(stderr,"scene editor open: %s\n",message);return;}
	YMGUI_MsgBox_ClearButtons(g_sceneOpenError);
	YMGUI_MsgBox_SetTitle(g_sceneOpenError,"无法打开场景");
	YMGUI_MsgBox_SetText(g_sceneOpenError,message);
	YMGUI_MsgBox_AddButton(g_sceneOpenError,"返回",reopenSceneDialog);
	YMGUI_MsgBox_Show(g_sceneOpenError);
}

static void loadSceneFromPath(const char* path, void* userData)
{
	(void)userData;SceneEditorObject staged[32];uint32 count=0;int32 mainIndex=-1,activeIndex=-1;
	char error[256];
	if(!SceneEditorIo_Read(path,staged,32,&count,&mainIndex,&activeIndex,error,sizeof(error))){
		SceneEditorIo_SetError(error);showSceneOpenError(error);setStatus("场景打开失败，已保留当前场景");return;
	}
	uint8 ignoreResourceErrors=SceneEditorIo_GetIgnoreLoadErrors();
	char skippedDetails[224]="";uint32 skippedCount=0;
	gre_list stagedObjects={0},stagedLights={0};gre_scence stagedImport={0};uint32 built=0;
	for(;built<count;++built){
		SceneEditorObject* object=&staged[built];object->treeNode=NULL;
		if(object->kind==SCENE_OBJECT_MESH){
			uint8 meshFailed=0;char resourceError[224]="";
			if(object->primitiveKind<=SCENE_PLACE_CAPSULE){
				SceneEditorPlaceResult result={.kind=(SceneEditorPlaceKind)object->primitiveKind,
					.scale=object->scale,.rotationY=object->rotY,.detailA=object->detailA,
					.detailB=object->detailB,.color=object->color,.wireframe=object->wireframe};
				snprintf(result.name,sizeof(result.name),"%s",object->name);
				object->mesh=SceneEditorPlace_CreateMesh(&result);
			}else if(object->primitiveKind==0xFF&&object->sourcePath[0]!='\0'){
				if(!SceneEditorImport_Validate(object->sourcePath,resourceError,sizeof(resourceError))){
					meshFailed=1;
				}else{
					object->mesh=YMGRE_LoadOgreMeshAndMaterial(&stagedImport,object->sourcePath);
					if(object->mesh!=NULL&&!SceneEditorImport_ValidateLoadedMaterials(object->sourcePath,
						object->mesh,resourceError,sizeof(resourceError))){
						YMGRE_Free_Object(object->mesh);object->mesh=NULL;meshFailed=1;
					}
				}
			}
			if(object->mesh==NULL&&!meshFailed){snprintf(resourceError,sizeof(resourceError),
				"网格解析失败：%.150s",object->sourcePath[0]!='\0'?object->sourcePath:"内置网格");meshFailed=1;}
			if(meshFailed){
				snprintf(error,sizeof(error),"物体「%.48s」：%.180s",object->name,resourceError);
				if(!ignoreResourceErrors)break;
				size_t used=strlen(skippedDetails);
				if(used+1<sizeof(skippedDetails))snprintf(skippedDetails+used,sizeof(skippedDetails)-used,
					"「%.36s」：%.120s\n",object->name,resourceError);
				object->active=0;skippedCount++;continue;
			}
			SceneEditorPlace_TransformMesh(object->mesh,object->x,object->y,object->z,
				object->rotY,object->scale,object->wireframe);
			for(GRE_Object4d part=object->mesh;part!=NULL;part=part->nextObject)part->isVisible=object->visible;
			YMGRE_List_Append(&stagedObjects,sizeof(gre_object4d),object->mesh);
		}else if(object->kind==SCENE_OBJECT_LIGHT){
			GRErgb24 rgb={(uint8)(object->color>>16),(uint8)(object->color>>8),(uint8)object->color};
			object->light=YMGRE_Creat_Light((int16)built,object->lightType,rgb,object->strength);
			if(object->light==NULL){snprintf(error,sizeof(error),"无法创建灯光：%.120s",object->name);break;}
			SceneEditorObject_SyncHandle(object);normalizeSpotDirection(object);
			YMGRE_List_Append(&stagedLights,sizeof(gre_light4d),object->light);
		}else{
			object->camera=YMGRE_Creat_CameraFromTarget((int16)built,&g_target,45,45,45,45);
			if(object->camera==NULL){snprintf(error,sizeof(error),"无法创建相机：%.120s",object->name);break;}
			updateProjection(object->camera,g_target.width);syncCamera(object);
		}
	}
	if(built!=count){freeStagedScene(staged,built,&stagedObjects,&stagedLights,&stagedImport);
		SceneEditorIo_SetError(error);showSceneOpenError(error);setStatus("场景打开失败，已保留当前场景");return;}
	if(skippedCount>0){
		uint32 compactCount=0;
		for(uint32 source=0;source<count;++source)if(staged[source].active){
			if((int32)source==mainIndex)mainIndex=(int32)compactCount;
			if((int32)source==activeIndex)activeIndex=(int32)compactCount;
			if(source!=compactCount)staged[compactCount]=staged[source];
			compactCount++;
		}
		count=compactCount;
	}
	clearSelection("正在打开场景");
	for(int i=0;i<32;++i)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_CAMERA&&g_scene[i].camera!=NULL)
		YMGRE_Free_Camera(g_scene[i].camera);
	YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);YMGRE_List_Clear(&g_objectsList,YMGRE_Free_Object);
	YMGRE_List_Clear(&g_importContext.MaterialList,YMGRE_Free_Material);
	memset(g_scene,0,sizeof(g_scene));memcpy(g_scene,staged,sizeof(SceneEditorObject)*count);
	g_objectsList=stagedObjects;g_lightsList=stagedLights;g_importContext=stagedImport;
	g_mainCamera=&g_scene[mainIndex];g_activeCamera=&g_scene[activeIndex];g_globalLight=NULL;
	for(uint32 i=0;i<count;++i)if(g_scene[i].kind==SCENE_OBJECT_LIGHT&&
		g_scene[i].lightType==GRE_GlobalLight){g_globalLight=&g_scene[i];break;}
	rebuildHierarchyTree();syncCamera(g_activeCamera);
	char cameraText[96];snprintf(cameraText,sizeof(cameraText),"VIEWPORT  /  %.20s",g_activeCamera->name);
	YMGUI_Label_SetText(g_ui->viewportCamera,cameraText);SceneEditorIo_Close();
	snprintf(g_currentScenePath,sizeof(g_currentScenePath),"%s",path);g_sceneDirty=0;g_pendingAction=PENDING_NONE;
	if(!g_historyRestoring)historyReset();
	char status[192];
	if(skippedCount>0)snprintf(status,sizeof(status),"场景已打开，已过滤 %u 个错误物体",skippedCount);
	else snprintf(status,sizeof(status),"场景已打开：%.150s",path);
	setStatus(status);
	if(skippedCount>0){
		char warning[256];snprintf(warning,sizeof(warning),"已过滤 %u 个无法加载的物体：\n%.205s",skippedCount,skippedDetails);
		YMGUI_MsgBox_ClearButtons(g_sceneOpenError);YMGUI_MsgBox_SetTitle(g_sceneOpenError,"场景已打开，但有资源被忽略");
		YMGUI_MsgBox_SetText(g_sceneOpenError,warning);YMGUI_MsgBox_AddButton(g_sceneOpenError,"确定",NULL);
		YMGUI_MsgBox_Show(g_sceneOpenError);
	}
}

static void historyRemoveFrom(uint32 first)
{
	for(uint32 i=first;i<g_historyCount;++i)if(g_historyPaths[i][0]!='\0')unlink(g_historyPaths[i]);
	g_historyCount=first;
}

static void historyCommit(void)
{
	if(!g_historyReady||g_historyRestoring||g_historyBatch)return;
	if(g_historyIndex+1<g_historyCount){
		if(g_historySavedIndex>(int32)g_historyIndex)g_historySavedIndex=-1;
		historyRemoveFrom(g_historyIndex+1);
	}
	if(g_historyCount==SCENE_HISTORY_CAPACITY){
		unlink(g_historyPaths[0]);
		memmove(g_historyPaths,g_historyPaths[1],sizeof(g_historyPaths[0])*(SCENE_HISTORY_CAPACITY-1));
		g_historyCount--;if(g_historyIndex>0)g_historyIndex--;
		if(g_historySavedIndex>=0)g_historySavedIndex--;
	}
	char path[192],error[192];
	snprintf(path,sizeof(path),"/tmp/ymgre_scene_history_%ld_%u.scene",(long)getpid(),g_historySerial++);
	if(!writeCurrentScene(path,error,sizeof(error))){fprintf(stderr,"scene history: %s\n",error);return;}
	snprintf(g_historyPaths[g_historyCount],sizeof(g_historyPaths[g_historyCount]),"%s",path);
	g_historyIndex=g_historyCount++;
	g_sceneDirty=g_historySavedIndex<0||(int32)g_historyIndex!=g_historySavedIndex;
}

static void historyReset(void)
{
	historyRemoveFrom(0);g_historyIndex=0;g_historySavedIndex=-1;g_historyReady=1;
	historyCommit();g_historySavedIndex=(int32)g_historyIndex;g_sceneDirty=0;
}

static uint8 historyRestore(uint32 target)
{
	if(!g_historyReady||target>=g_historyCount||target==g_historyIndex)return 0;
	char documentPath[SCENE_EDITOR_PATH_CAPACITY];snprintf(documentPath,sizeof(documentPath),"%s",g_currentScenePath);
	uint8 ignore=SceneEditorIo_GetIgnoreLoadErrors();g_historyRestoring=1;SceneEditorIo_SetIgnoreLoadErrors(0);
	loadSceneFromPath(g_historyPaths[target],NULL);
	uint8 restored=strcmp(g_currentScenePath,g_historyPaths[target])==0;
	g_historyRestoring=0;SceneEditorIo_SetIgnoreLoadErrors(ignore);
	if(!restored)return 0;
	snprintf(g_currentScenePath,sizeof(g_currentScenePath),"%s",documentPath);g_historyIndex=target;
	g_sceneDirty=g_historySavedIndex<0||(int32)g_historyIndex!=g_historySavedIndex;
	return 1;
}

static void undoClicked(GYOBJ button){(void)button;if(g_historyIndex>0){if(historyRestore(g_historyIndex-1))setStatus("已撤销场景修改");}else setStatus("没有可撤销的修改");}
static void redoClicked(GYOBJ button){(void)button;if(g_historyIndex+1<g_historyCount){if(historyRestore(g_historyIndex+1))setStatus("已重做场景修改");}else setStatus("没有可重做的修改");}

static void createNewScene(void)
{
	clearSelection("正在新建场景");
	for(int i=0;i<32;++i)
		if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_CAMERA&&g_scene[i].camera!=NULL)
			YMGRE_Free_Camera(g_scene[i].camera);
	YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);
	YMGRE_List_Clear(&g_objectsList,YMGRE_Free_Object);
	YMGRE_List_Clear(&g_importContext.MaterialList,YMGRE_Free_Material);
	memset(g_scene,0,sizeof(g_scene));
	createDefaultScene();
	rebuildHierarchyTree();
	YMGUI_Label_SetText(g_ui->viewportCamera,"VIEWPORT  /  Main Camera");
	SceneEditorIo_ResetPath();
	g_currentScenePath[0]='\0';g_sceneDirty=0;g_pendingAction=PENDING_NONE;
	if(g_image!=NULL)YMGUI_Obj_Invalidate(g_image);
	setStatus("已新建场景");
	if(g_historyReady)historyReset();
}

static void completePendingAction(void)
{
	PendingAction action=g_pendingAction;g_pendingAction=PENDING_NONE;
	if(action==PENDING_NEW)createNewScene();
	else if(action==PENDING_OPEN)SceneEditorIo_OpenLoad();
	else if(action==PENDING_EXIT)g_shouldExit=1;
}

static void unsavedPromptResult(GYOBJ messageBox, int index)
{
	(void)messageBox;
	if(index==0){
		if(g_currentScenePath[0]!='\0')saveSceneToPath(g_currentScenePath,g_ui);
		else SceneEditorIo_OpenSave();
	}else if(index==1)completePendingAction();
	else g_pendingAction=PENDING_NONE;
}

static void showUnsavedPrompt(PendingAction action)
{
	if(g_unsavedPrompt==NULL)return;
	g_pendingAction=action;YMGUI_MsgBox_ClearButtons(g_unsavedPrompt);
	YMGUI_MsgBox_SetTitle(g_unsavedPrompt,action==PENDING_NEW?"新建场景":
		(action==PENDING_OPEN?"打开场景":"退出场景编辑器"));
	YMGUI_MsgBox_SetText(g_unsavedPrompt,"当前场景有未保存的修改，是否先保存？");
	YMGUI_MsgBox_AddButton(g_unsavedPrompt,"保存",unsavedPromptResult);
	YMGUI_MsgBox_AddButton(g_unsavedPrompt,"不保存",unsavedPromptResult);
	YMGUI_MsgBox_AddButton(g_unsavedPrompt,"取消",unsavedPromptResult);
	YMGUI_MsgBox_Show(g_unsavedPrompt);
}

static void sceneDialogCancelled(void* userData)
{
	(void)userData;uint8 hadPending=g_pendingAction!=PENDING_NONE;g_pendingAction=PENDING_NONE;
	setStatus(hadPending?"已取消保存，当前场景已保留":"已取消文件操作");
}

static void saveClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_editMenu,1);SceneEditorIo_OpenSave();}
static void importClicked(GYOBJ button){(void)button;SceneEditorImport_Open();}
static void addClicked(GYOBJ button)
{
	(void)button;
	if(!g_sceneDirty){createNewScene();return;}
	showUnsavedPrompt(PENDING_NEW);
}
static void frameClicked(GYOBJ button)
{
	(void)button;
	if(g_selected==NULL)return;
	float32 distance=cameraDistance(g_activeCamera);
	float32 dx=g_activeCamera->x-g_activeCamera->targetX,dy=g_activeCamera->y-g_activeCamera->targetY,dz=g_activeCamera->z-g_activeCamera->targetZ;
	g_activeCamera->targetX=g_selected->x;g_activeCamera->targetY=g_selected->y;g_activeCamera->targetZ=g_selected->z;
	float32 old=sqrtf(dx*dx+dy*dy+dz*dz);if(old<0.001f)old=1;
	g_activeCamera->x=g_activeCamera->targetX+dx/old*distance;g_activeCamera->y=g_activeCamera->targetY+dy/old*distance;g_activeCamera->z=g_activeCamera->targetZ+dz/old*distance;
	syncCamera(g_activeCamera);g_sceneDirty=1;historyCommit();setStatus("视图已定位到选中对象");
}
static void editClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_editMenu,!(g_editMenu->state&GY_STATE_Hidden));}
static void openClicked(GYOBJ button)
{
	(void)button;YMGUI_Obj_SetHidden(g_editMenu,1);
	if(g_sceneDirty)showUnsavedPrompt(PENDING_OPEN);else SceneEditorIo_OpenLoad();
}

static int quitRequested(void* userData)
{
	(void)userData;
	if(g_shouldExit)return 1;
	if(g_sceneDirty){
		if(g_unsavedPrompt==NULL||!YMGUI_MsgBox_IsShown(g_unsavedPrompt))showUnsavedPrompt(PENDING_EXIT);
	}else g_shouldExit=1;
	return 0;
}
static void resetClicked(GYOBJ button)
{
	(void)button;g_activeCamera->x=0;g_activeCamera->y=149;g_activeCamera->z=-213;
	g_activeCamera->targetX=g_activeCamera->targetY=g_activeCamera->targetZ=0;syncCamera(g_activeCamera);g_sceneDirty=1;historyCommit();refreshCameraInspector();setStatus("视图已复位");
}
static void leftClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_leftPanel,!(g_leftPanel->state&GY_STATE_Hidden));layoutPanels();}
static void rightClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_rightPanel,!(g_rightPanel->state&GY_STATE_Hidden));layoutPanels();}
static void referenceClicked(GYOBJ button)
{
	g_referenceVisible=!g_referenceVisible;
	if(g_referenceVisible){g_lineCount=0;buildReferenceLines();}
	YMGUI_Button_SetText(button,g_referenceVisible?"隐藏坐标轴和网格":"显示坐标轴和网格");
	if(g_image!=NULL)YMGUI_Obj_Invalidate(g_image);
	setStatus(g_referenceVisible?"已显示坐标轴和网格":"已隐藏坐标轴和网格");
}

static GYOBJ label(GYOBJ parent,GYcoord x,GYcoord y,GYcoord w,GYcoord h,const char* text,GYcolor color)
{
	GYOBJ object=YMGUI_Creat_Label_Creat(parent,x,y,w,h);
	if(object!=NULL){YMGUI_Label_SetText(object,text);YMGUI_Label_SetTextColor(object,color);}return object;
}
static void toolbarButton(GYOBJ parent,GYcoord x,const char* text,GYbtn_clicked_cb clicked)
{
	GYOBJ button=YMGUI_Creat_Button_Creat(parent,x,12,78,30);YMGUI_Button_SetText(button,text);
	YMGUI_Button_SetColors(button,GY_ARGB(0xFF,0x2C,0x35,0x48),GY_ARGB(0xFF,0x3C,0x86,0xB8));YMGUI_Button_SetClicked(button,clicked);
}
static void updateTransformButtons(void)
{
	for(uint8 i=0;i<3;++i)if(g_transformButtons[i]!=NULL)
		YMGUI_Button_SetColors(g_transformButtons[i],i==(uint8)g_transformMode?GY_ARGB(0xFF,0x2E,0x79,0x96):GY_ARGB(0xFF,0x2C,0x35,0x48),
			GY_ARGB(0xFF,0x3C,0x86,0xB8));
}
static void transformModeClicked(GYOBJ button)
{
	for(uint8 i=0;i<3;++i)if(g_transformButtons[i]==button){g_transformMode=(g_transformMode==(TransformMode)i)?TRANSFORM_NONE:(TransformMode)i;break;}
	updateTransformButtons();if(g_image!=NULL)YMGUI_Obj_Invalidate(g_image);
	setStatus(g_transformMode==TRANSFORM_NONE?"已取消变换约束":(g_transformMode==TRANSFORM_MOVE?"移动模式":(g_transformMode==TRANSFORM_ROTATE?"三轴旋转模式":"统一缩放模式")));
}
static void transformButton(GYOBJ parent,GYcoord x,const char* text,uint8 index)
{
	g_transformButtons[index]=YMGUI_Creat_Button_Creat(parent,x,12,54,30);
	YMGUI_Button_SetText(g_transformButtons[index],text);YMGUI_Button_SetClicked(g_transformButtons[index],transformModeClicked);
}
static GYOBJ menuButton(GYOBJ parent,GYcoord y,const char* text,GYbtn_clicked_cb clicked)
{
	GYOBJ button=YMGUI_Creat_Button_Creat(parent,8,y,150,28);YMGUI_Button_SetText(button,text);YMGUI_Button_SetClicked(button,clicked);
	return button;
}

static void buildUi(EditorUi* ui)
{
	GYCTX ctx=ui->host.context;GYOBJ root=ctx->root;
	GYOBJ bar=YMGUI_Creat_Obj_Creat(root,0,0,1024,56);
	g_leftPanel=YMGUI_Creat_Obj_Creat(root,0,56,236,620);
	g_centerPanel=YMGUI_Creat_Obj_Creat(root,244,64,560,540);
	g_rightPanel=YMGUI_Creat_Obj_Creat(root,812,56,212,620);
	GYcolor fg=GY_ARGB(0xFF,0xE4,0xEA,0xF2),muted=GY_ARGB(0xFF,0x9A,0xA7,0xB8),cyan=GY_ARGB(0xFF,0x6E,0xC8,0xE8);
	YMGUI_Obj_SetBgColor(bar,GY_ARGB(0xFF,0x16,0x1C,0x28));YMGUI_Obj_SetBgColor(g_leftPanel,GY_ARGB(0xFF,0x27,0x31,0x42));
	YMGUI_Obj_SetBgColor(g_centerPanel,GY_ARGB(0xFF,0x0E,0x13,0x1C));YMGUI_Obj_SetBgColor(g_rightPanel,GY_ARGB(0xFF,0x32,0x2B,0x3E));
	toolbarButton(bar,18,"编辑",editClicked);SceneEditorPlace_Build(bar,104,ctx,objectPlaced,ui);
	SceneEditorColor_Build(ctx);toolbarButton(bar,190,"新建",addClicked);toolbarButton(bar,276,"保存",saveClicked);toolbarButton(bar,362,"定位",frameClicked);toolbarButton(bar,448,"导入",importClicked);
	transformButton(bar,540,"移动",0);transformButton(bar,598,"旋转",1);transformButton(bar,656,"缩放",2);updateTransformButtons();
	label(bar,724,16,286,20,"PERSPECTIVE | 60 FPS | RGB565",muted);
	g_editMenu=YMGUI_Creat_Obj_Creat(root,12,48,170,300);YMGUI_Obj_SetBgColor(g_editMenu,GY_ARGB(0xFF,0x25,0x2D,0x3C));YMGUI_Obj_SetHidden(g_editMenu,1);
	menuButton(g_editMenu,8,"打开场景",openClicked);menuButton(g_editMenu,40,"保存场景",saveClicked);
	menuButton(g_editMenu,72,"撤销",undoClicked);menuButton(g_editMenu,104,"重做",redoClicked);
	menuButton(g_editMenu,136,"切换层级",leftClicked);
	menuButton(g_editMenu,200,"切换检查器",rightClicked);menuButton(g_editMenu,232,"重置视图",resetClicked);
	g_referenceToggle=menuButton(g_editMenu,264,"隐藏坐标轴和网格",referenceClicked);
	YMGUI_Layout_Stack(g_editMenu,GY_LAYOUT_VER,4,8,GY_CROSS_START);
	label(g_leftPanel,16,14,190,22,"场景层级",fg);
	ui->tree=YMGUI_Creat_TreeView_Creat(g_leftPanel,12,48,212,430);YMGUI_TreeView_SetRowHeight(ui->tree,28);YMGUI_TreeView_SetIndent(ui->tree,18);
	YMGUI_TreeView_SetSelectCb(ui->tree,treeSelect);YMGUI_TreeView_SetContextCb(ui->tree,treeContext);
	rebuildHierarchyTree();
	label(g_leftPanel,16,500,204,22,"右键管理场景对象",muted);label(g_leftPanel,16,526,204,22,"点击项目查看属性",muted);
	g_image=YMGUI_Creat_Image_Creat(g_centerPanel,0,0,560,540);g_imageSource=(GYimg){(const GYpx*)g_colorBuffer,560,540,0,0};YMGUI_Image_SetSrc(g_image,&g_imageSource);
	ui->viewport=YMGUI_Creat_Obj_Creat(g_centerPanel,0,0,560,540);ui->viewport->draw_cb=NULL;ui->viewport->event_cb=viewportEvent;ui->viewport->state|=GY_STATE_Focusable;
	ui->viewportBrand=label(g_centerPanel,18,16,240,22,"YMGRE  /  SCENE EDITOR",cyan);ui->viewportCamera=label(g_centerPanel,300,16,242,22,"VIEWPORT  /  Main Camera",fg);
	label(g_centerPanel,18,482,440,24,"LMB select / move    blank drag pans    RMB orbits",muted);
	SceneEditorInspector_Build(g_rightPanel,inspectorChanged,inspectorRemesh,ui);
	SceneEditorHierarchyOps ops={renameObject,deleteObject,switchCamera,duplicateObject};SceneEditorHierarchy_Build(ctx,&ops,ui);
	SceneEditorImport_Build(ctx,importMesh,ui);
	SceneEditorIo_Build(ctx,saveSceneToPath,loadSceneFromPath,sceneDialogCancelled,ui);
	g_unsavedPrompt=YMGUI_Creat_MsgBox_Creat(ctx);
	g_sceneOpenError=YMGUI_Creat_MsgBox_Creat(ctx);
	ui->status=label(root,14,650,996,24,"就绪",muted);
	clearSelection("就绪");YMGUI_Inject_SetCtx(ctx);YMGUI_SetFocus(ctx,ui->viewport);
}

static int runSelfTest(void)
{
	int failures=0;
	#define SELF_CHECK(condition) do { if(!(condition)) { \
		fprintf(stderr, "scene_editor self-test failed at line %d: %s\n", __LINE__, #condition); \
		failures++; } } while(0)
	const char* longPath="/tmp/ymgre/scenes/a_directory_name_longer_than_the_old_limit/blender_export.scene";
	GYOBJ pathInput=YMGUI_Creat_TextInput_Creat(g_ui->host.context->root,0,0,320,28,255);
	YMGUI_TextInput_SetText(pathInput,longPath);
	SELF_CHECK(strcmp(YMGUI_TextInput_GetText(pathInput),longPath)==0);
	YMGUI_Free_ObjFree(pathInput);
	char orphanMesh[160],orphanError[160];
	snprintf(orphanMesh,sizeof(orphanMesh),"/tmp/ymgre_orphan_%ld.MESH",(long)getpid());
	FILE* orphan=fopen(orphanMesh,"wb");SELF_CHECK(orphan!=NULL);
	if(orphan!=NULL)fclose(orphan);
	SELF_CHECK(!SceneEditorImport_Validate(orphanMesh,orphanError,sizeof(orphanError))&&
		strstr(orphanError,".material")!=NULL);
	unlink(orphanMesh);
	char textureMesh[160],textureMaterial[160];
	snprintf(textureMesh,sizeof(textureMesh),"/tmp/ymgre_texture_%ld.mesh",(long)getpid());
	snprintf(textureMaterial,sizeof(textureMaterial),"/tmp/ymgre_texture_%ld.material",(long)getpid());
	FILE* textureMeshFile=fopen(textureMesh,"wb");SELF_CHECK(textureMeshFile!=NULL);
	if(textureMeshFile!=NULL)fclose(textureMeshFile);
	FILE* textureMaterialFile=fopen(textureMaterial,"w");SELF_CHECK(textureMaterialFile!=NULL);
	if(textureMaterialFile!=NULL){
		fputs("material SelftestMaterial\n{\n texture_unit\n {\n  texture missing_selftest.bmp\n }\n}\n",textureMaterialFile);
		fclose(textureMaterialFile);
	}
	SELF_CHECK(!SceneEditorImport_Validate(textureMesh,orphanError,sizeof(orphanError))&&
		strstr(orphanError,"SelftestMaterial")!=NULL&&strstr(orphanError,"missing_selftest.bmp")!=NULL);
	GRE_Object4d missingMaterialMesh=YMGRE_Creat_Object(1,0,"Selftest Mesh","MissingMaterial");
	SELF_CHECK(missingMaterialMesh!=NULL&&!SceneEditorImport_ValidateLoadedMaterials(textureMesh,
		missingMaterialMesh,orphanError,sizeof(orphanError))&&strstr(orphanError,"MissingMaterial")!=NULL&&
		strstr(orphanError,".material")!=NULL);
	if(missingMaterialMesh!=NULL)YMGRE_Free_Object(missingMaterialMesh);
	unlink(textureMaterial);unlink(textureMesh);
	char brokenScenePath[160],missingMeshPath[160];
	snprintf(brokenScenePath,sizeof(brokenScenePath),"/tmp/ymgre_broken_%ld.scene",(long)getpid());
	snprintf(missingMeshPath,sizeof(missingMeshPath),"/tmp/missing_scene_mesh_%ld.mesh",(long)getpid());
	SceneEditorObject brokenScene[3]={{0}};
	brokenScene[0].active=1;brokenScene[0].kind=SCENE_OBJECT_LIGHT;
	brokenScene[0].lightType=GRE_GlobalLight;brokenScene[0].strength=1;
	brokenScene[0].color=GY_ARGB(0xFF,255,255,255);snprintf(brokenScene[0].name,sizeof(brokenScene[0].name),"Global Light");
	brokenScene[1].active=1;brokenScene[1].kind=SCENE_OBJECT_CAMERA;brokenScene[1].scale=1;
	brokenScene[1].x=0;brokenScene[1].y=149;brokenScene[1].z=-213;
	snprintf(brokenScene[1].name,sizeof(brokenScene[1].name),"Test Camera");
	brokenScene[2].active=1;brokenScene[2].kind=SCENE_OBJECT_MESH;brokenScene[2].primitiveKind=0xFF;
	brokenScene[2].scale=1;snprintf(brokenScene[2].name,sizeof(brokenScene[2].name),"Missing Statue");
	snprintf(brokenScene[2].sourcePath,sizeof(brokenScene[2].sourcePath),"%s",missingMeshPath);
	SELF_CHECK(SceneEditorIo_Write(brokenScenePath,brokenScene,3,1,1,orphanError,sizeof(orphanError)));
	SceneEditorObject brokenRead[3];uint32 brokenCount=0;int32 brokenMain=-1,brokenActive=-1;
	SELF_CHECK(SceneEditorIo_Read(brokenScenePath,brokenRead,3,&brokenCount,&brokenMain,&brokenActive,
		orphanError,sizeof(orphanError))&&brokenCount==3&&
		strstr(brokenRead[2].sourcePath,"missing_scene_mesh")!=NULL);
	SceneEditorIo_SetIgnoreLoadErrors(0);
	loadSceneFromPath(brokenScenePath,NULL);
	SELF_CHECK(YMGUI_MsgBox_IsShown(g_sceneOpenError)&&g_objectsList.len==0&&g_lightsList.len==1);
	YMGUI_MsgBox_Close(g_sceneOpenError);
	SceneEditorIo_SetIgnoreLoadErrors(1);loadSceneFromPath(brokenScenePath,NULL);
	uint32 filteredActive=0;for(int i=0;i<32;++i)if(g_scene[i].active)filteredActive++;
	SELF_CHECK(YMGUI_MsgBox_IsShown(g_sceneOpenError)&&filteredActive==2&&
		g_objectsList.len==0&&g_lightsList.len==1&&strcmp(g_currentScenePath,brokenScenePath)==0);
	YMGUI_MsgBox_Close(g_sceneOpenError);SceneEditorIo_SetIgnoreLoadErrors(1);
	SceneEditorIo_OpenLoad();SELF_CHECK(!SceneEditorIo_GetIgnoreLoadErrors());SceneEditorIo_Close();
	unlink(brokenScenePath);
	float32 wheelDistance=cameraDistance(g_activeCamera);
	g_ui->host.context->wheel_y=1;viewportEvent(g_ui->viewport,GY_EVENT_Wheel);
	SELF_CHECK(cameraDistance(g_activeCamera)<wheelDistance);
	g_ui->host.context->wheel_y=-1;viewportEvent(g_ui->viewport,GY_EVENT_Wheel);
	SELF_CHECK(fabsf(cameraDistance(g_activeCamera)-wheelDistance)<0.01f);
	SELF_CHECK(g_selected == NULL && YMGUI_TreeView_GetSelectedNode(g_ui->tree) == NULL);
	g_activeCamera->targetX=12;g_activeCamera->targetY=8;g_activeCamera->targetZ=-4;
	viewportEvent(g_ui->viewport,GY_EVENT_ContextRequested);
	SELF_CHECK(g_activeCamera->targetX==0&&g_activeCamera->targetY==0&&g_activeCamera->targetZ==0);
	g_selected=g_activeCamera;g_activeCamera->targetX=5;
	viewportEvent(g_ui->viewport,GY_EVENT_ContextRequested);
	SELF_CHECK(g_activeCamera->targetX==0&&g_activeCamera->targetY==0&&g_activeCamera->targetZ==0);
	SceneEditorObject otherCameraPivot={.active=1,.kind=SCENE_OBJECT_CAMERA,.x=35,.y=20,.z=-15};
	g_selected=&otherCameraPivot;viewportEvent(g_ui->viewport,GY_EVENT_ContextRequested);
	SELF_CHECK(g_activeCamera->targetX==otherCameraPivot.x&&
		g_activeCamera->targetY==otherCameraPivot.y&&g_activeCamera->targetZ==otherCameraPivot.z);
	g_selected=NULL;
	g_sceneDirty=0;
	SceneEditorPlaceResult sphere={.kind=SCENE_PLACE_SPHERE,.x=12,.y=18,.z=4,.scale=1,
		.color=GY_ARGB(0xFF,80,170,110),.wireframe=1,.detailA=6,.detailB=12};
	snprintf(sphere.name,sizeof(sphere.name),"Selftest Sphere");snprintf(sphere.type,sizeof(sphere.type),"球体");
	sphere.mesh=YMGRE_MeshGener_Sphere(18,sphere.detailA,sphere.detailB,(GRErgb24){80,170,110},sphere.name,"");
	sphere.mesh->WorldCoordinate=(gre_fvector4d){sphere.x,sphere.y,sphere.z,1};sphere.mesh->wireFrame=1;YMGRE_Object_LocalToWorld(sphere.mesh);
	objectPlaced(&sphere,NULL);SceneEditorObject* mesh=g_selected;
	SELF_CHECK(g_sceneDirty&&mesh!=NULL&&mesh->mesh!=NULL&&mesh->mesh->polygonNum==120&&g_objectsList.len==1);
	viewportEvent(g_ui->viewport,GY_EVENT_ContextRequested);
	SELF_CHECK(g_activeCamera->targetX==mesh->x&&g_activeCamera->targetY==mesh->y&&g_activeCamera->targetZ==mesh->z);
	GRE_Object4d originalMesh=mesh->mesh;
	SELF_CHECK(inspectorRemesh(mesh,8,16,NULL)&&mesh->mesh!=originalMesh&&
		mesh->mesh->polygonNum==224&&mesh->detailA==8&&mesh->detailB==16&&
		g_objectsList.listhead->data==mesh->mesh);
	float32 beforeX=mesh->mesh->pointList[1].pos.x-mesh->x;
	float32 beforeZ=mesh->mesh->pointList[1].pos.z-mesh->z;
	SceneEditorObject_SetRotationY(mesh,90);
	SELF_CHECK(fabsf(mesh->rotY-90)<0.001f&&
		fabsf((mesh->mesh->pointList[1].pos.x-mesh->x)-beforeZ)<0.001f&&
		fabsf((mesh->mesh->pointList[1].pos.z-mesh->z)+beforeX)<0.001f);
	addSelectionLines();SELF_CHECK(g_referenceVisible&&g_lineCount==g_baseLineCount+12);
	referenceClicked(g_referenceToggle);addSelectionLines();
	SELF_CHECK(!g_referenceVisible&&g_lineCount==12);
	referenceClicked(g_referenceToggle);addSelectionLines();
	SELF_CHECK(g_referenceVisible&&g_lineCount==g_baseLineCount+12);
	SceneEditorPlaceResult grounded={.kind=SCENE_PLACE_SPHERE,.scale=2,.detailA=6,.detailB=12,
		.color=GY_ARGB(0xFF,80,170,110)};
	snprintf(grounded.name,sizeof(grounded.name),"Grounded Sphere");
	grounded.mesh=SceneEditorPlace_CreateMesh(&grounded);
	float32 localMinimum=grounded.mesh->pointList[0].pos.y;
	for(int i=1;i<grounded.mesh->pointNum;++i)localMinimum=GREMin(localMinimum,grounded.mesh->pointList[i].pos.y);
	grounded.y=-localMinimum*grounded.scale;
	SceneEditorPlace_TransformMesh(grounded.mesh,0,grounded.y,0,0,grounded.scale,1);
	float32 worldMinimum=grounded.mesh->pointList[0].pos.y;
	for(int i=1;i<grounded.mesh->pointNum;++i)worldMinimum=GREMin(worldMinimum,grounded.mesh->pointList[i].pos.y);
	SELF_CHECK(fabsf(worldMinimum)<0.001f);YMGRE_Free_Object(grounded.mesh);
	SceneEditorPlaceResult plane={.kind=SCENE_PLACE_PLANE,.scale=1,.detailA=3,.detailB=4,
		.color=GY_ARGB(0xFF,100,120,160),.wireframe=1};
	snprintf(plane.name,sizeof(plane.name),"Selftest Plane");snprintf(plane.type,sizeof(plane.type),"平面");
	plane.mesh=SceneEditorPlace_CreateMesh(&plane);SceneEditorPlace_TransformMesh(plane.mesh,0,0,0,0,1,1);
	objectPlaced(&plane,NULL);SceneEditorObject* planeObject=g_selected;
	SELF_CHECK(planeObject->mesh->polygonNum==24&&inspectorRemesh(planeObject,5,6,NULL)&&
		planeObject->mesh->polygonNum==60);

	SceneEditorObject_SetColor(g_globalLight,GY_ARGB(0xFF,120,140,200));
	SELF_CHECK(g_globalLight->light->proper.lightcolor.R==120&&g_globalLight->light->proper.lightcolor.B==200);
	SceneEditorPlaceResult point={.kind=SCENE_PLACE_POINT_LIGHT,.x=20,.y=40,.z=10,.strength=1.5f,.color=GY_ARGB(0xFF,255,200,150)};
	snprintf(point.name,sizeof(point.name),"Selftest Point");snprintf(point.type,sizeof(point.type),"点光源");objectPlaced(&point,NULL);SceneEditorObject* pointObject=g_selected;
	SELF_CHECK(pointObject->light!=NULL&&pointObject->light->type==GRE_PointLight&&
		pointObject->light->proper.shadowK==0&&g_lightsList.len==2);
	pointObject->shadowsEnabled=1;SceneEditorObject_SyncHandle(pointObject);
	SELF_CHECK(pointObject->light->proper.shadowK>0);pointObject->shadowsEnabled=0;SceneEditorObject_SyncHandle(pointObject);
	selectObject(pointObject,"Selftest drag");dragSelected(2.0f,-1.0f);
	SELF_CHECK(isfinite(pointObject->x)&&isfinite(pointObject->y)&&isfinite(pointObject->z)&&
		fabsf(pointObject->x-20.0f)<10.0f&&fabsf(pointObject->y-40.0f)<10.0f&&
		isfinite(pointObject->light->pos.x)&&isfinite(pointObject->light->pos.y)&&isfinite(pointObject->light->pos.z));
	SceneEditorPlaceResult spot={.kind=SCENE_PLACE_SPOT_LIGHT,.x=-20,.y=50,.z=0,.targetY=0,.strength=2,.color=GY_ARGB(0xFF,180,220,255)};
	snprintf(spot.name,sizeof(spot.name),"Selftest Spot");snprintf(spot.type,sizeof(spot.type),"聚光灯");objectPlaced(&spot,NULL);SceneEditorObject* spotObject=g_selected;
	SELF_CHECK(spotObject->light!=NULL&&spotObject->light->type==GRE_SpotLight&&spotObject->light->proper.spot.direct.y<0&&g_lightsList.len==3);
	addSelectionLines();SELF_CHECK(g_lineCount==g_baseLineCount+23&&
		fabsf(g_lines[g_lineCount-4].end.x-spotObject->targetX)<0.001f&&
		fabsf(g_lines[g_lineCount-4].end.y-spotObject->targetY)<0.001f&&
		fabsf(g_lines[g_lineCount-4].end.z-spotObject->targetZ)<0.001f);
	SceneEditorPlaceResult camera={.kind=SCENE_PLACE_CAMERA,.x=80,.y=70,.z=-100,.targetX=0,.targetY=10,.targetZ=0};
	snprintf(camera.name,sizeof(camera.name),"Selftest Camera");snprintf(camera.type,sizeof(camera.type),"相机");objectPlaced(&camera,NULL);SceneEditorObject* cameraObject=g_selected;
	SELF_CHECK(cameraObject->camera!=NULL);switchCamera(cameraObject,NULL);SELF_CHECK(g_activeCamera==cameraObject);
	addSelectionLines();SELF_CHECK(g_lineCount==g_baseLineCount+28);
	renameObject(cameraObject,"Camera Renamed",NULL);SELF_CHECK(strcmp(cameraObject->name,"Camera Renamed")==0);
	deleteObject(cameraObject,NULL);SELF_CHECK(g_activeCamera==g_mainCamera&&!cameraObject->active);
	SceneEditorObject* importedObject=NULL;
	if(access("Resource/obj/Tank1_Body.mesh",R_OK)==0){
		SELF_CHECK(importMesh("Resource/obj/Tank1_Body.mesh",0,NULL));importedObject=g_selected;
		SELF_CHECK(importedObject!=NULL&&importedObject->mesh!=NULL&&!importedObject->wireframe&&
			!importedObject->mesh->wireFrame&&g_importContext.MaterialList.len>0);
		SceneEditorObject_Translate(importedObject,80,0,0);
		float32 pickX=0,pickY=0,pickDepth=0;uint8 foundPoint=0;
		for(GRE_Object4d part=importedObject->mesh;part!=NULL&&!foundPoint;part=part->nextObject)
			for(int i=0;i<part->pointNum;++i)
				if(projectPoint(&part->pointList[i].pos,&pickX,&pickY,&pickDepth)){foundPoint=1;break;}
		GYrect viewportArea;YMGUI_Obj_GetAbsArea(g_ui->viewport,&viewportArea);
			SELF_CHECK(foundPoint&&pickObject(g_ui->viewport,viewportArea.x+(GYcoord)pickX,
				viewportArea.y+(GYcoord)pickY)==importedObject);
			importedObject->fixed=1;
			SELF_CHECK(pickObject(g_ui->viewport,viewportArea.x+(GYcoord)pickX,
				viewportArea.y+(GYcoord)pickY)!=importedObject);
			float32 fixedX=importedObject->x;selectObject(importedObject,"Selftest fixed");dragSelected(10,0);
			SELF_CHECK(fabsf(importedObject->x-fixedX)<0.001f);importedObject->fixed=0;
			for(GRE_Object4d part=importedObject->mesh;part!=NULL;part=part->nextObject)part->isVisible=0;
			SELF_CHECK(pickObject(g_ui->viewport,viewportArea.x+(GYcoord)pickX,
				viewportArea.y+(GYcoord)pickY)!=importedObject);
			for(GRE_Object4d part=importedObject->mesh;part!=NULL;part=part->nextObject)part->isVisible=1;
		}
	SceneEditorPlaceResult savedCamera={.kind=SCENE_PLACE_CAMERA,.x=65,.y=55,.z=-90,.targetY=8};
	snprintf(savedCamera.name,sizeof(savedCamera.name),"Saved Camera");snprintf(savedCamera.type,sizeof(savedCamera.type),"相机");
	objectPlaced(&savedCamera,NULL);SceneEditorObject* savedCameraObject=g_selected;switchCamera(savedCameraObject,NULL);
	int savedObjectCount=g_objectsList.len,savedLightCount=g_lightsList.len;
	char scenePath[160];snprintf(scenePath,sizeof(scenePath),"/tmp/ymgre_scene_editor_selftest_%ld.scene",(long)getpid());
	unlink(scenePath);saveSceneToPath(scenePath,NULL);SELF_CHECK(access(scenePath,R_OK)==0&&
		!g_sceneDirty&&strcmp(g_currentScenePath,scenePath)==0);
	if(importedObject!=NULL)deleteObject(importedObject,NULL);
	deleteObject(savedCameraObject,NULL);deleteObject(spotObject,NULL);deleteObject(pointObject,NULL);
	deleteObject(planeObject,NULL);deleteObject(mesh,NULL);
	SELF_CHECK(g_objectsList.len==0&&g_lightsList.len==1&&g_selected==NULL);
	loadSceneFromPath(scenePath,NULL);SELF_CHECK(g_objectsList.len==savedObjectCount&&
		g_lightsList.len==savedLightCount&&strcmp(g_activeCamera->name,"Saved Camera")==0&&
		g_globalLight!=NULL&&g_globalLight->light->proper.lightcolor.R==120&&
		!g_sceneDirty&&strcmp(g_currentScenePath,scenePath)==0);
	SceneEditorObject* loadedSphere=NULL;SceneEditorObject* loadedImport=NULL;
	for(int i=0;i<32;++i)if(g_scene[i].active){
		if(strcmp(g_scene[i].name,"Selftest Sphere")==0)loadedSphere=&g_scene[i];
		if(g_scene[i].primitiveKind==0xFF)loadedImport=&g_scene[i];
	}
	SELF_CHECK(loadedSphere!=NULL&&loadedSphere->mesh!=NULL&&loadedSphere->detailA==8&&
		fabsf(loadedSphere->rotY-90)<0.001f);
	if(importedObject!=NULL)SELF_CHECK(loadedImport!=NULL&&loadedImport->sourcePath[0]!='\0');
	for(int i=31;i>=0;--i)if(g_scene[i].active&&&g_scene[i]!=g_mainCamera&&&g_scene[i]!=g_globalLight)
		deleteObject(&g_scene[i],NULL);
	SELF_CHECK(g_sceneDirty&&g_objectsList.len==0&&g_lightsList.len==1&&g_selected==NULL);
	addClicked(NULL);SELF_CHECK(YMGUI_MsgBox_IsShown(g_unsavedPrompt)&&g_pendingAction==PENDING_NEW);
	unsavedPromptResult(g_unsavedPrompt,2);YMGUI_MsgBox_Close(g_unsavedPrompt);
	SELF_CHECK(g_sceneDirty&&g_currentScenePath[0]!='\0');
	createNewScene();uint32 activeObjects=0;
	for(int i=0;i<32;++i)if(g_scene[i].active)activeObjects++;
	SELF_CHECK(activeObjects==2&&g_objectsList.len==0&&g_lightsList.len==1&&
		g_mainCamera!=NULL&&g_mainCamera->camera!=NULL&&g_activeCamera==g_mainCamera&&
		g_globalLight!=NULL&&g_globalLight->light!=NULL&&!g_sceneDirty&&
		g_currentScenePath[0]=='\0'&&g_selected==NULL);
	SceneEditorPlaceResult historySphere={.kind=SCENE_PLACE_SPHERE,.x=0,.y=18,.z=0,.scale=1,
		.color=GY_ARGB(0xFF,90,160,210),.detailA=6,.detailB=12};
	snprintf(historySphere.name,sizeof(historySphere.name),"History Sphere");snprintf(historySphere.type,sizeof(historySphere.type),"球体");
	historySphere.mesh=SceneEditorPlace_CreateMesh(&historySphere);
	SceneEditorPlace_TransformMesh(historySphere.mesh,historySphere.x,historySphere.y,historySphere.z,0,1,0);
	objectPlaced(&historySphere,NULL);SceneEditorObject* historyObject=g_selected;
	g_transformMode=TRANSFORM_MOVE;addSelectionLines();uint32 beforeGizmo=g_lineCount;addTransformGizmo();
	SELF_CHECK(g_lineCount==beforeGizmo+3);
	float32 transformX=historyObject->x;dragSelectedAxis(VIEW_DRAG_MOVE_X,12,0);
	SELF_CHECK(fabsf(historyObject->x-transformX)>0.001f);
	g_transformMode=TRANSFORM_ROTATE;addSelectionLines();beforeGizmo=g_lineCount;addTransformGizmo();
	SELF_CHECK(g_lineCount==beforeGizmo+75);float32 transformRotation=historyObject->rotY;
	dragSelectedAxis(VIEW_DRAG_ROTATE_Y,10,0);SELF_CHECK(historyObject->rotY>transformRotation);
	g_transformMode=TRANSFORM_SCALE;float32 transformScale=historyObject->scale;
	dragSelectedAxis(VIEW_DRAG_SCALE,10,-5);SELF_CHECK(historyObject->scale>transformScale);historyCommit();
	duplicateObject(historyObject,NULL);SELF_CHECK(g_objectsList.len==2&&strstr(g_selected->name,"Copy")!=NULL);
	undoClicked(NULL);SELF_CHECK(g_objectsList.len==1);
	redoClicked(NULL);SELF_CHECK(g_objectsList.len==2);
	g_sceneDirty=1;openClicked(NULL);SELF_CHECK(YMGUI_MsgBox_IsShown(g_unsavedPrompt)&&g_pendingAction==PENDING_OPEN);
	unsavedPromptResult(g_unsavedPrompt,2);YMGUI_MsgBox_Close(g_unsavedPrompt);
	SELF_CHECK(g_sceneDirty&&g_pendingAction==PENDING_NONE);
	SELF_CHECK(!quitRequested(NULL)&&YMGUI_MsgBox_IsShown(g_unsavedPrompt)&&g_pendingAction==PENDING_EXIT&&!g_shouldExit);
	unsavedPromptResult(g_unsavedPrompt,2);YMGUI_MsgBox_Close(g_unsavedPrompt);
	SELF_CHECK(!g_shouldExit&&g_pendingAction==PENDING_NONE);createNewScene();g_transformMode=TRANSFORM_MOVE;updateTransformButtons();
	unlink(scenePath);
#undef SELF_CHECK
	return failures;
}

int main(void)
{
	EditorUi ui={0};g_ui=&ui;setenv("YMGRE_WINDOW_SCALE","1",1);
	if(!YMGRE_DemoHost_Init(&ui.host,1024,680,40))return 1;
	SceneEditorFont_Init();initScene();buildUi(&ui);historyReset();SDL_LCD_SetQuitRequestCb(quitRequested,NULL);
	const char* limit=getenv("YMGRE_MAX_FRAMES");ui.frameLimit=limit?atoi(limit):0;
	int selfTestFailures=0;
	if(getenv("YMGRE_SCENE_EDITOR_SELFTEST")!=NULL){selfTestFailures=runSelfTest();ui.frameLimit=1;}
	YMGUI_Obj_SetBgColor(ui.host.context->root,GY_ARGB(0xFF,0x15,0x1A,0x24));
	while(!g_shouldExit&&SDL_LCD_PumpEvents()){
		renderScene();YMGUI_Obj_Invalidate(g_image);YMGUI_Refresh(ui.host.context);
		if(ui.frameLimit>0&&++ui.frames>=ui.frameLimit) break;
		SDL_LCD_Delay(16);
	}
	for(int i=0;i<32;++i)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_CAMERA&&g_scene[i].camera!=g_mainCamera->camera)YMGRE_Free_Camera(g_scene[i].camera);
	YMGRE_Free_RenderWorkspace(g_workspace);YMGRE_Free_Camera(g_mainCamera->camera);
	YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);YMGRE_List_Clear(&g_objectsList,YMGRE_Free_Object);
	YMGRE_List_Clear(&g_importContext.MaterialList,YMGRE_Free_Material);
	historyRemoveFrom(0);
	SceneEditorInspector_Shutdown();YMGRE_DemoHost_Destroy(&ui.host);SceneEditorFont_Shutdown();g_ui=NULL;return selfTestFailures?1:0;
}
