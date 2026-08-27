#include "demo_host.h"
#include "scene_editor_color.h"
#include "scene_editor_font.h"
#include "scene_editor_hierarchy.h"
#include "scene_editor_import.h"
#include "scene_editor_inspector.h"
#include "scene_editor_place.h"

#include "SDL_LCD.h"
#include "YMGUI_Button.h"
#include "YMGUI_Event.h"
#include "YMGUI_Image.h"
#include "YMGUI_Invalidate.h"
#include "YMGUI_Label.h"
#include "YMGUI_Obj.h"
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
#include <time.h>
#include <unistd.h>

typedef enum { VIEW_DRAG_NONE, VIEW_DRAG_PAN, VIEW_DRAG_OBJECT } ViewDragMode;

typedef struct {
	YMGRE_DemoHost host;
	GYOBJ viewport, viewportBrand, viewportCamera, status, tree;
	int pointerX, pointerY, frames, frameLimit;
	ViewDragMode dragMode;
} EditorUi;

static EditorUi* g_ui;
static gre_list g_objectsList, g_lightsList;
static gre_scence g_importContext;
static GRE_RenderWorkspace g_workspace;
static GRE_FramePixel g_colorBuffer[1024 * 540];
static float32 g_depthBuffer[1024 * 540];
static gre_render_target g_target;
static GYOBJ g_image, g_editMenu, g_leftPanel, g_rightPanel, g_centerPanel;
static GYimg g_imageSource;
static gre_line3d g_lines[96];
static uint32 g_lineCount, g_baseLineCount;
static SceneEditorObject g_scene[32];
static SceneEditorObject* g_selected;
static SceneEditorObject* g_mainCamera;
static SceneEditorObject* g_activeCamera;
static SceneEditorObject* g_globalLight;
static GYTREENODE g_objectsNode, g_lightsNode, g_camerasNode;

static void setStatus(const char* text)
{
	if (g_ui != NULL && g_ui->status != NULL) YMGUI_Label_SetText(g_ui->status, text);
}

static void addLine(gre_fvector4d start, gre_fvector4d end, GRErgb24 color)
{
	if (g_lineCount < sizeof(g_lines) / sizeof(g_lines[0]))
		g_lines[g_lineCount++] = (gre_line3d){ start, end, color };
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
	g_lineCount = g_baseLineCount;
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

static void initScene(void)
{
	memset(g_scene, 0, sizeof(g_scene));
	buildReferenceLines();
	YMGRE_RenderTarget_Init(&g_target, 560, 540, g_colorBuffer, g_depthBuffer);
	g_workspace = YMGRE_Creat_RenderWorkspace();
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
	g_globalLight->strength = 0.2f; g_globalLight->color = GY_ARGB(0xFF,255,255,255);
	g_globalLight->light = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24){255,255,255}, 0.2f);
	YMGRE_List_Append(&g_lightsList, sizeof(gre_light4d), g_globalLight->light);
}

static void renderScene(void)
{
	if (g_activeCamera == NULL) return;
	syncCamera(g_activeCamera);
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(g_activeCamera->camera,
		&g_lightsList, &g_objectsList, &g_importContext.MaterialList, g_workspace);
	addSelectionLines();
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
}

static void zoomCamera(float32 factor)
{
	SceneEditorObject* camera = g_activeCamera;
	camera->x=camera->targetX+(camera->x-camera->targetX)*factor;
	camera->y=camera->targetY+(camera->y-camera->targetY)*factor;
	camera->z=camera->targetZ+(camera->z-camera->targetZ)*factor;
	syncCamera(camera);
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
		g_ui->pointerX=x; g_ui->pointerY=y;
		SceneEditorObject* picked=pickObject(object,x,y);
		if (picked != NULL) { selectObject(picked,"已在视口中选中对象，拖动可移动"); g_ui->dragMode=VIEW_DRAG_OBJECT; }
		else { clearSelection("已取消选择；拖动空白区域可平移视图"); g_ui->dragMode=VIEW_DRAG_PAN; }
	} else if (event == GY_EVENT_Pressing) {
		float32 dx=x-g_ui->pointerX, dy=y-g_ui->pointerY;
		if (g_ui->dragMode==VIEW_DRAG_OBJECT) { dragSelected(dx,dy); setStatus("正在移动选中对象"); }
		else if (g_ui->dragMode==VIEW_DRAG_PAN) { panCamera(dx,dy); refreshCameraInspector(); setStatus("正在平移视图"); }
		g_ui->pointerX=x; g_ui->pointerY=y; YMGUI_Obj_Invalidate(object);
	} else if (event == GY_EVENT_ContextRequested) {
		if (g_selected != NULL && g_selected != g_activeCamera &&
			!(g_selected->kind == SCENE_OBJECT_LIGHT && g_selected->lightType == GRE_GlobalLight)) {
			float32 dx = g_activeCamera->x - g_selected->x;
			float32 dy = g_activeCamera->y - g_selected->y;
			float32 dz = g_activeCamera->z - g_selected->z;
			if (dx*dx + dy*dy + dz*dz > 0.0001f) {
				g_activeCamera->targetX = g_selected->x;
				g_activeCamera->targetY = g_selected->y;
				g_activeCamera->targetZ = g_selected->z;
				syncCamera(g_activeCamera);
			}
		}
		g_ui->pointerX=x; g_ui->pointerY=y; setStatus("右键拖动旋转视图");
	} else if (event == GY_EVENT_ContextDragging) {
		orbitCamera(x-g_ui->pointerX,y-g_ui->pointerY); refreshCameraInspector();
		g_ui->pointerX=x; g_ui->pointerY=y; YMGUI_Obj_Invalidate(object);
	} else if (event == GY_EVENT_Key && object->ctx->last_key=='+') {
		zoomCamera(expf(-0.16f)); refreshCameraInspector(); YMGUI_Obj_Invalidate(object); setStatus("视图拉近");
	} else if (event == GY_EVENT_Key && object->ctx->last_key=='-') {
		zoomCamera(expf(0.16f)); refreshCameraInspector(); YMGUI_Obj_Invalidate(object); setStatus("视图拉远");
	} else if (event==GY_EVENT_Released || event==GY_EVENT_ReleasedOff ||
		event==GY_EVENT_ContextReleased || event==GY_EVENT_ContextCancelled) g_ui->dragMode=VIEW_DRAG_NONE;
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
		g_ui->host.context->point_y,object!=g_mainCamera);
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
}

static void switchCamera(SceneEditorObject* object, void* userData)
{
	(void)userData;
	if (object==NULL || object->kind!=SCENE_OBJECT_CAMERA) return;
	g_activeCamera=object; syncCamera(object);
	char text[96]; snprintf(text,sizeof(text),"VIEWPORT  /  %.20s",object->name);
	YMGUI_Label_SetText(g_ui->viewportCamera,text); selectObject(object,"已切换相机视角");
}

static void deleteObject(SceneEditorObject* object, void* userData)
{
	(void)userData;
	if (object==NULL || object==g_mainCamera) return;
	if (object==g_activeCamera) switchCamera(g_mainCamera,NULL);
	if (object->kind==SCENE_OBJECT_MESH) removeListItem(&g_objectsList,object->mesh,YMGRE_Free_Object);
	else if (object->kind==SCENE_OBJECT_LIGHT) removeListItem(&g_lightsList,object->light,YMGRE_Free_Light);
	else if (object->camera!=NULL) YMGRE_Free_Camera(object->camera);
	YMGUI_TreeView_RemoveNode(g_ui->tree,object->treeNode);
	memset(object,0,sizeof(*object));
	clearSelection("对象已删除");
}

static void inspectorChanged(const char* status, void* userData)
{
	(void)userData;
	if (g_selected!=NULL) { normalizeSpotDirection(g_selected); if(g_selected->kind==SCENE_OBJECT_CAMERA)syncCamera(g_selected); }
	setStatus(status); if(g_image!=NULL)YMGUI_Obj_Invalidate(g_image);
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
	snprintf(object->name, sizeof(object->name), "%s", mesh->objName != NULL ? mesh->objName : "Imported Mesh");
	snprintf(object->type, sizeof(object->type), "外部网格");
	YMGRE_List_Append(&g_objectsList, sizeof(gre_object4d), mesh);
	object->treeNode = YMGUI_TreeView_AddNode(g_ui->tree, g_objectsNode, object->name, 0);
	YMGUI_TreeView_SetNodeUserPtr(object->treeNode, object);
	YMGUI_TreeView_SetExpanded(g_ui->tree, g_objectsNode, 1);
	selectObject(object, "外部网格已导入");
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
	return 1;
}

static void saveClicked(GYOBJ button){(void)button;setStatus("场景保存入口待接序列化");}
static void importClicked(GYOBJ button){(void)button;SceneEditorImport_Open();}
static void addClicked(GYOBJ button){(void)button;setStatus("请使用放置菜单创建对象、灯光或相机");}
static void frameClicked(GYOBJ button)
{
	(void)button;
	if(g_selected==NULL)return;
	float32 distance=cameraDistance(g_activeCamera);
	float32 dx=g_activeCamera->x-g_activeCamera->targetX,dy=g_activeCamera->y-g_activeCamera->targetY,dz=g_activeCamera->z-g_activeCamera->targetZ;
	g_activeCamera->targetX=g_selected->x;g_activeCamera->targetY=g_selected->y;g_activeCamera->targetZ=g_selected->z;
	float32 old=sqrtf(dx*dx+dy*dy+dz*dz);if(old<0.001f)old=1;
	g_activeCamera->x=g_activeCamera->targetX+dx/old*distance;g_activeCamera->y=g_activeCamera->targetY+dy/old*distance;g_activeCamera->z=g_activeCamera->targetZ+dz/old*distance;
	syncCamera(g_activeCamera);setStatus("视图已定位到选中对象");
}
static void editClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_editMenu,!(g_editMenu->state&GY_STATE_Hidden));}
static void openClicked(GYOBJ button){(void)button;setStatus("打开场景入口待接文件选择");}
static void resetClicked(GYOBJ button)
{
	(void)button;g_activeCamera->x=0;g_activeCamera->y=149;g_activeCamera->z=-213;
	g_activeCamera->targetX=g_activeCamera->targetY=g_activeCamera->targetZ=0;syncCamera(g_activeCamera);refreshCameraInspector();setStatus("视图已复位");
}
static void leftClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_leftPanel,!(g_leftPanel->state&GY_STATE_Hidden));layoutPanels();}
static void rightClicked(GYOBJ button){(void)button;YMGUI_Obj_SetHidden(g_rightPanel,!(g_rightPanel->state&GY_STATE_Hidden));layoutPanels();}

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
static void menuButton(GYOBJ parent,GYcoord y,const char* text,GYbtn_clicked_cb clicked)
{
	GYOBJ button=YMGUI_Creat_Button_Creat(parent,8,y,150,28);YMGUI_Button_SetText(button,text);YMGUI_Button_SetClicked(button,clicked);
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
	label(bar,650,16,350,20,"PERSPECTIVE   |   60 FPS   |   RGB565",muted);
	g_editMenu=YMGUI_Creat_Obj_Creat(root,12,48,170,172);YMGUI_Obj_SetBgColor(g_editMenu,GY_ARGB(0xFF,0x25,0x2D,0x3C));YMGUI_Obj_SetHidden(g_editMenu,1);
	menuButton(g_editMenu,8,"打开场景",openClicked);menuButton(g_editMenu,40,"保存场景",saveClicked);menuButton(g_editMenu,72,"切换层级",leftClicked);menuButton(g_editMenu,104,"切换检查器",rightClicked);menuButton(g_editMenu,136,"重置视图",resetClicked);
	label(g_leftPanel,16,14,190,22,"场景层级",fg);
	ui->tree=YMGUI_Creat_TreeView_Creat(g_leftPanel,12,48,212,430);YMGUI_TreeView_SetRowHeight(ui->tree,28);YMGUI_TreeView_SetIndent(ui->tree,18);
	YMGUI_TreeView_SetSelectCb(ui->tree,treeSelect);YMGUI_TreeView_SetContextCb(ui->tree,treeContext);
	GYTREENODE scene=YMGUI_TreeView_AddNode(ui->tree,NULL,"Scene",1);
	g_camerasNode=YMGUI_TreeView_AddNode(ui->tree,scene,"Cameras",1);g_lightsNode=YMGUI_TreeView_AddNode(ui->tree,scene,"Lights",1);g_objectsNode=YMGUI_TreeView_AddNode(ui->tree,scene,"Objects",1);
	g_mainCamera->treeNode=YMGUI_TreeView_AddNode(ui->tree,g_camerasNode,g_mainCamera->name,0);YMGUI_TreeView_SetNodeUserPtr(g_mainCamera->treeNode,g_mainCamera);
	g_globalLight->treeNode=YMGUI_TreeView_AddNode(ui->tree,g_lightsNode,g_globalLight->name,0);YMGUI_TreeView_SetNodeUserPtr(g_globalLight->treeNode,g_globalLight);
	YMGUI_TreeView_SetExpanded(ui->tree,scene,1);YMGUI_TreeView_SetExpanded(ui->tree,g_camerasNode,1);YMGUI_TreeView_SetExpanded(ui->tree,g_lightsNode,1);YMGUI_TreeView_SetExpanded(ui->tree,g_objectsNode,1);
	label(g_leftPanel,16,500,204,22,"右键管理场景对象",muted);label(g_leftPanel,16,526,204,22,"点击项目查看属性",muted);
	g_image=YMGUI_Creat_Image_Creat(g_centerPanel,0,0,560,540);g_imageSource=(GYimg){(const GYpx*)g_colorBuffer,560,540,0,0};YMGUI_Image_SetSrc(g_image,&g_imageSource);
	ui->viewport=YMGUI_Creat_Obj_Creat(g_centerPanel,0,0,560,540);ui->viewport->draw_cb=NULL;ui->viewport->event_cb=viewportEvent;ui->viewport->state|=GY_STATE_Focusable;
	ui->viewportBrand=label(g_centerPanel,18,16,240,22,"YMGRE  /  SCENE EDITOR",cyan);ui->viewportCamera=label(g_centerPanel,300,16,242,22,"VIEWPORT  /  Main Camera",fg);
	label(g_centerPanel,18,482,440,24,"LMB select / move    blank drag pans    RMB orbits",muted);
	SceneEditorInspector_Build(g_rightPanel,inspectorChanged,inspectorRemesh,ui);
	SceneEditorHierarchyOps ops={renameObject,deleteObject,switchCamera};SceneEditorHierarchy_Build(ctx,&ops,ui);
	SceneEditorImport_Build(ctx,importMesh,ui);
	ui->status=label(root,14,650,996,24,"就绪",muted);
	clearSelection("就绪");YMGUI_Inject_SetCtx(ctx);YMGUI_SetFocus(ctx,ui->viewport);
}

static uint32 monotonicMilliseconds(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint32)(now.tv_sec * 1000u + now.tv_nsec / 1000000u);
}

static int runSelfTest(void)
{
	int failures=0;
	#define SELF_CHECK(condition) do { if(!(condition)) { \
		fprintf(stderr, "scene_editor self-test failed at line %d: %s\n", __LINE__, #condition); \
		failures++; } } while(0)
	SELF_CHECK(g_selected == NULL && YMGUI_TreeView_GetSelectedNode(g_ui->tree) == NULL);
	SceneEditorPlaceResult sphere={.kind=SCENE_PLACE_SPHERE,.x=12,.y=18,.z=4,.scale=1,
		.color=GY_ARGB(0xFF,80,170,110),.wireframe=1,.detailA=6,.detailB=12};
	snprintf(sphere.name,sizeof(sphere.name),"Selftest Sphere");snprintf(sphere.type,sizeof(sphere.type),"球体");
	sphere.mesh=YMGRE_MeshGener_Sphere(18,sphere.detailA,sphere.detailB,(GRErgb24){80,170,110},sphere.name,"");
	sphere.mesh->WorldCoordinate=(gre_fvector4d){sphere.x,sphere.y,sphere.z,1};sphere.mesh->wireFrame=1;YMGRE_Object_LocalToWorld(sphere.mesh);
	objectPlaced(&sphere,NULL);SceneEditorObject* mesh=g_selected;
	SELF_CHECK(mesh!=NULL&&mesh->mesh!=NULL&&mesh->mesh->polygonNum==120&&g_objectsList.len==1);
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
	addSelectionLines();SELF_CHECK(g_lineCount==g_baseLineCount+12);
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
	if(importedObject!=NULL)deleteObject(importedObject,NULL);
	deleteObject(spotObject,NULL);deleteObject(pointObject,NULL);deleteObject(planeObject,NULL);deleteObject(mesh,NULL);
	SELF_CHECK(g_objectsList.len==0&&g_lightsList.len==1&&g_selected==NULL);
#undef SELF_CHECK
	return failures;
}

int main(void)
{
	EditorUi ui={0};g_ui=&ui;setenv("YMGRE_WINDOW_SCALE","1",1);
	if(!YMGRE_DemoHost_Init(&ui.host,1024,680,40))return 1;
	SceneEditorFont_Init();initScene();buildUi(&ui);
	const char* limit=getenv("YMGRE_MAX_FRAMES");ui.frameLimit=limit?atoi(limit):0;
	int selfTestFailures=0;
	if(getenv("YMGRE_SCENE_EDITOR_SELFTEST")!=NULL){selfTestFailures=runSelfTest();ui.frameLimit=1;}
	YMGUI_Obj_SetBgColor(ui.host.context->root,GY_ARGB(0xFF,0x15,0x1A,0x24));
	uint32 previousTick=monotonicMilliseconds();
	while(SDL_LCD_PumpEvents()){
		uint32 currentTick=monotonicMilliseconds();
		uint32 elapsed=currentTick-previousTick;previousTick=currentTick;
		SceneEditorInspector_Tick((uint16)GREMin(elapsed,65535u));
		renderScene();YMGUI_Obj_Invalidate(g_image);YMGUI_Refresh(ui.host.context);
		if(ui.frameLimit>0&&++ui.frames>=ui.frameLimit) break;
		SDL_LCD_Delay(16);
	}
	for(int i=0;i<32;++i)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_CAMERA&&g_scene[i].camera!=g_mainCamera->camera)YMGRE_Free_Camera(g_scene[i].camera);
	YMGRE_Free_RenderWorkspace(g_workspace);YMGRE_Free_Camera(g_mainCamera->camera);
	YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);YMGRE_List_Clear(&g_objectsList,YMGRE_Free_Object);
	YMGRE_List_Clear(&g_importContext.MaterialList,YMGRE_Free_Material);
	SceneEditorInspector_Shutdown();YMGRE_DemoHost_Destroy(&ui.host);SceneEditorFont_Shutdown();g_ui=NULL;return selfTestFailures?1:0;
}
