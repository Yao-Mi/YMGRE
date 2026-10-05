#include "demo_host.h"
#include "scene_editor_color.h"
#include "scene_editor_font.h"
#include "scene_editor_hierarchy.h"
#include "scene_editor_import.h"
#include "scene_editor_inspector.h"
#include "scene_editor_io.h"
#include "scene_editor_place.h"
#include "scene_bake.h"
#include "scene_model_export.h"
#include "scene_ray_renderer.h"
#include "scene_uv.h"
#include "scene_uv_editor.h"
#include "scene_uv_preview.h"
#include "scene_editor_texture.h"
#include "YMGUI_Dropdown.h"
#include "scene_editor_file_dialog.h"
#include "YMGUI_FileDialog.h"

#include "SDL_LCD.h"
#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
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
#include "YMGRE_LOD.h"
#include "YMGRE_LOD_Simplify.h"
#include "YMCS_File_IO.h"
#include "YMGRE_ScenceManager.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// New YMGUI keeps Ctrl+Z as the only built-in edit key; the editor retains
// its private redo value for application-level shortcut handling.
#ifndef GY_KEY_REDO
#define GY_KEY_REDO 0x110E
#endif
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
static SceneRayRenderer* g_rayRenderer;
static GYOBJ g_rendererSelector, g_rendererProgress;
static uint16 g_rendererMode;
static int g_rayProgress;
static GRE_FramePixel g_colorBuffer[1024 * 648];
static float32 g_depthBuffer[1024 * 648];
static gre_render_target g_target;
static GYOBJ g_image, g_editMenu, g_leftPanel, g_rightPanel, g_centerPanel;
static GYimg g_imageSource;
static gre_line3d g_lines[256];
static uint32 g_lineCount, g_baseLineCount;
static uint8 g_referenceVisible=1;
static int8 g_rotateAxis=-1;
static float32 g_rotateStartAngle;
static float32 g_rotateStartPointerAngle;
static float32 g_rotateCenterX, g_rotateCenterY;
static float32 g_rotateBasisUX, g_rotateBasisUY, g_rotateBasisVX, g_rotateBasisVY;

static float32 rotatePointerAngle(float32 x, float32 y)
{
	float32 dx=x-g_rotateCenterX, dy=y-g_rotateCenterY;
	return atan2f(g_rotateBasisUX*dy-g_rotateBasisUY*dx,
		g_rotateBasisUX*dx+g_rotateBasisUY*dy);
}

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
static GYOBJ g_bakeExportDialog, g_bakeExportPath;
static SceneEditorObject* g_bakeExportObject;
static uint64_t g_bakeExportFingerprint;
static uint8 g_exportModel;
static GYOBJ g_lodPanel,g_lodNearInput,g_lodMiddleInput,g_lodHysteresisInput;
static GYOBJ g_lodMiddleRatioInput,g_lodFarRatioInput,g_lodFarKind,g_lodPreviewButton;
typedef struct {
    SceneEditorObject* object;
    SceneEditorObject* camera;
    GRE_Object4d source,levels[3],active;
    grass_impostor *image;
    YMGRE_LOD_Object descriptor;
    YMGRE_LOD_Instance selection;
    gre_fvector4d center,direction;
    float radius,savedCamera[6];
    uint8 running,activeLevel;
    unsigned frame;
} SceneLodPreview;
static SceneLodPreview g_lodPreview;
static void lodStopPreview(void);
static GYOBJ g_bakeLighting;
static char g_bakedModelPath[SCENE_EDITOR_PATH_CAPACITY];
static char g_bakeExportDirectory[SCENE_EDITOR_PATH_CAPACITY];
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
static int projectPoint(const gre_fvector4d* world, float32* screenX, float32* screenY, float32* depth);

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

static int projectMouseToRotationPlane(uint8 axis, gre_fvector4d center, gre_fvector4d* hit)
{
	if(g_ui==NULL||g_activeCamera==NULL||g_activeCamera->camera==NULL||hit==NULL)return 0;
	GYrect area; YMGUI_Obj_GetAbsArea(g_ui->viewport,&area);
	GRE_Camera4d camera=g_activeCamera->camera;
	float32 px=(float32)(g_ui->pointerX-area.x), py=(float32)(g_ui->pointerY-area.y);
	float32 viewW=camera->perspectPlane.pR-camera->perspectPlane.pL, viewH=camera->perspectPlane.pU-camera->perspectPlane.pD;
	float32 cx=(px-(float32)g_target.width*0.5f)*viewW/g_target.width;
	float32 cy=((float32)g_target.height*0.5f-py)*viewH/g_target.height;
	gre_fvector4d origin=camera->pos;
	gre_fvector4d direction={camera->move.cu.x*cx+camera->move.cv.x*cy+camera->move.cn.x*camera->perspectPlane.Dis,
		camera->move.cu.y*cx+camera->move.cv.y*cy+camera->move.cn.y*camera->perspectPlane.Dis,
		camera->move.cu.z*cx+camera->move.cv.z*cy+camera->move.cn.z*camera->perspectPlane.Dis,0};
	YMGRE_Fvector4d_Normalize(&direction);
	gre_fvector4d normal={axis==0?1.0f:0.0f,axis==1?1.0f:0.0f,axis==2?1.0f:0.0f,0};
	gre_fvector4d toCenter={center.x-origin.x,center.y-origin.y,center.z-origin.z,0};
	float32 denominator=YMGRE_Fvector4d_Dot(&direction,&normal);
	float32 t;
	if(fabsf(denominator)<0.0001f){
		/* 视线平行于旋转面时没有唯一交点，取中心深度处的射线点并压回该平面。 */
		t=YMGRE_Fvector4d_Dot(&toCenter,&direction); if(t<=0.0f||!isfinite(t))t=1.0f;
	} else { t=YMGRE_Fvector4d_Dot(&toCenter,&normal)/denominator; if(t<=0.0f||!isfinite(t))return 0; }
	hit->x=origin.x+direction.x*t;hit->y=origin.y+direction.y*t;hit->z=origin.z+direction.z*t;hit->w=1;
	if(fabsf(denominator)<0.0001f){if(axis==0)hit->x=center.x;else if(axis==1)hit->y=center.y;else hit->z=center.z;}
	return isfinite(hit->x)&&isfinite(hit->y)&&isfinite(hit->z);
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

static float32 rotationVisualDirection(uint8 axis)
{
	float32 direction=axis==1?-1.0f:1.0f;
	if(g_selected!=NULL&&g_activeCamera!=NULL&&g_activeCamera->camera!=NULL){
		if(axis==1&&g_activeCamera->y<g_selected->y)direction=-direction;
	}
	return direction;
}

static float32 projectedPlaneAngle(uint8 axis, gre_fvector4d center, gre_fvector4d hit)
{
	gre_fvector4d reference=center;
	if(axis==0)reference.y+=1.0f; else reference.x+=1.0f;
	gre_fvector4d r={reference.x-center.x,reference.y-center.y,reference.z-center.z,0};
	gre_fvector4d h={hit.x-center.x,hit.y-center.y,hit.z-center.z,0};
	gre_fvector4d normal={axis==0?1.0f:0.0f,axis==1?1.0f:0.0f,axis==2?1.0f:0.0f,0};
	gre_fvector4d cross;YMGRE_Fvector4d_CrossToResult(&r,&h,&cross);
	return atan2f(YMGRE_Fvector4d_Dot(&cross,&normal),YMGRE_Fvector4d_Dot(&r,&h))/YMGRE_Deg2Rad;
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
	if(g_selected!=NULL&&g_selected->kind==SCENE_OBJECT_MESH){
		if(g_selected->gizmoBaseExtent<=0.0f&&extent>0.0f)
			g_selected->gizmoBaseExtent=extent/(g_selected->scale>0.001f?g_selected->scale:1.0f);
		float32 base=g_selected->gizmoBaseExtent;
		if(base>0.0f){
			float32 gizmoExtent=base*1.3f;
			gizmoExtent*=g_selected->scale>0.001f?g_selected->scale:1.0f;
			size=gizmoExtent;
		}
	}
	if(g_selected==NULL||g_selected->kind!=SCENE_OBJECT_MESH){if(size<24.0f)size=24.0f;}
	if(size>240.0f)size=240.0f;return size;
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
				float32 degrees=axis==0?g_selected->rotX:(axis==1?-g_selected->rotY:g_selected->rotZ);
				float32 radians=degrees*YMGRE_Deg2Rad; int steps=(int)(fabsf(radians)*16.0f/YMGRE_Pai)+1; if(steps>64)steps=64;
				/* 用多条同心弧和两条边界线形成真正的扇形，而不是单独一截圆环。 */
				for(int band=0;band<3;++band){float32 radius=size*(0.68f+0.16f*(float32)band);gre_fvector4d previous=center;
					for(int i=0;i<=steps;++i){float32 a=radians*(float32)i/(float32)steps;gre_fvector4d point=center;
						if(axis==0){point.y+=cosf(a)*radius;point.z+=sinf(a)*radius;}
						else if(axis==1){point.x+=cosf(a)*radius;point.z+=sinf(a)*radius;}
						else {point.x+=cosf(a)*radius;point.y+=sinf(a)*radius;}
						if(i>0)addGizmoLine(previous,point,(GRErgb24){245,210,70}); previous=point;
					}
				}
				for(int edge=0;edge<2;++edge){float32 a=edge?0.0f:radians;gre_fvector4d point=center;
					if(axis==0){point.y+=cosf(a)*size;point.z+=sinf(a)*size;} else if(axis==1){point.x+=cosf(a)*size;point.z+=sinf(a)*size;} else {point.x+=cosf(a)*size;point.y+=sinf(a)*size;}
					addGizmoLine(center,point,(GRErgb24){245,210,70});
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
	YMGRE_RenderTarget_Init(&g_target, 560, 648, g_colorBuffer, g_depthBuffer);
	g_workspace = YMGRE_Creat_RenderWorkspace();
	createDefaultScene();
}

static gre_material bakeSurface(SceneEditorObject* object)
{
	gre_material surface={0};
	GRE_Material original=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
	if(original)surface=*original;
	if(object->primitiveKind!=0xFF || !original) {
		surface.ambient=surface.diffuse=(GRErgb24){(object->color>>16)&255,(object->color>>8)&255,object->color&255};
	}
	return surface;
}

static uint64_t signatureBytes(uint64_t h,const void* data,size_t size)
{const unsigned char* p=data;for(size_t i=0;i<size;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static uint64_t rayBakeSignature(void)
{
    uint64_t h=UINT64_C(14695981039346656037);
    for(int i=0;i<32;i++) {
        SceneEditorObject* o=g_scene+i;if(!o->active||o->kind!=SCENE_OBJECT_MESH||!o->mesh)continue;
        uint64_t values[]={SceneBake_Fingerprint(o->mesh,&g_lightsList),o->visible,o->color,o->rayType,o->analytic,
            (uint64_t)llround(o->reflectivity*100000),(uint64_t)llround(o->ior*100000),o->transmissionColor,
            (uint64_t)llround(o->mirrorKs*100000),(uint64_t)llround(o->specularPower*1000)};
        for(size_t j=0;j<sizeof(values)/sizeof(values[0]);j++){h^=values[j];h*=UINT64_C(1099511628211);}
        for(GRE_Object4d part=o->mesh;part;part=part->nextObject) {
            uint64_t geometry=SceneBake_Fingerprint(part,&g_lightsList);h=signatureBytes(h,&geometry,sizeof(geometry));
            h=signatureBytes(h,&part->isVisible,sizeof(part->isVisible));
            GRE_Material m=YMGRE_Material_Find(&g_importContext.MaterialList,part->materiaName);
            if(m){h=signatureBytes(h,&m->ambient,sizeof(GRErgb24));h=signatureBytes(h,&m->diffuse,sizeof(GRErgb24));
                h=signatureBytes(h,&m->specular,sizeof(GRErgb24));h=signatureBytes(h,&m->unlit,sizeof(m->unlit));
                if(m->pixel)h=signatureBytes(h,m->pixel,(size_t)m->width*m->height*sizeof(GRErgb24));
                if(m->advanced&&m->advanced->normalPixel)h=signatureBytes(h,m->advanced->normalPixel,(size_t)m->advanced->normalWidth*m->advanced->normalHeight*sizeof(GRErgb24));}
        }
    }
    for(GRE_ListNode n=g_lightsList.listhead;n;n=n->next){GRE_Light4d l=n->data;
        h=signatureBytes(h,&l->ishide,sizeof(l->ishide));h=signatureBytes(h,&l->proper.shadowK,sizeof(l->proper.shadowK));}
    return h?h:1;
}

static uint8 bakeColorsChanged(SceneEditorObject* object)
{
	GRE_Lightmap map=object->mesh->lightmap;if(!map->colorsBaked)return 0;
	gre_material surface=bakeSurface(object);
	return memcmp(&surface.ambient,&map->capturedAmbient,sizeof(GRErgb24)) ||
		memcmp(&surface.diffuse,&map->capturedDiffuse,sizeof(GRErgb24));
}

static void bakeSpecularSettings(SceneEditorObject* object,GRErgb24* color,uint8* power)
{
	GRE_Material base=YMGRE_Material_Find(&g_importContext.MaterialList,object->mesh->materiaName);
	*color=object->primitiveKind!=0xFF?(GRErgb24){255,255,255}:(base?base->specular:(GRErgb24){255,255,255});
	*power=object->specularPower>0?(uint8)object->specularPower:
		(base && base->advanced && base->advanced->specularPower?base->advanced->specularPower:30);
}

static uint8 bakeSpecularChanged(SceneEditorObject* object)
{
	GRE_Lightmap map=object->mesh->lightmap;if(!map->specularPixels)return 0;
	GRErgb24 color;uint8 power;bakeSpecularSettings(object,&color,&power);
	return fabsf(map->specularKs-object->mesh->mirrorKs)>1e-6f || power!=map->specularPower ||
		memcmp(&color,&map->specularColor,sizeof(color))!=0;
}

static void invalidateStaleBakes(void)
{
	for (int i=0;i<32;i++) {
		SceneEditorObject* object=&g_scene[i];
		if (!object->active || object->kind!=SCENE_OBJECT_MESH || !object->bakePath[0]) continue;
		if (!object->mesh || !object->mesh->lightmap ||
			object->bakeFingerprint!=SceneBake_ModeFingerprint(object->mesh,&g_lightsList,object->mesh->lightmap->materialOnly) ||
			bakeSpecularChanged(object) || bakeColorsChanged(object) ||
            (object->rayBakeSignature&&object->rayBakeSignature!=rayBakeSignature())) {
			if(object->mesh) {YMGRE_Free_Lightmap(object->mesh->lightmap);object->mesh->lightmap=NULL;}
			object->bakePath[0]='\0';object->bakeFingerprint=0;object->rayBakeSignature=0;
			setStatus("物体或灯光已变化，烘焙预览已停用，请重新烘焙");
		}
	}
}

static void rendererSelected(GYOBJ dropdown, uint16 mode)
{
	(void)dropdown;
	if(mode==1&&g_lodPreview.running)lodStopPreview();
	g_rendererMode=mode==1;
	g_rayProgress=0;
	SceneEditorInspector_SetRayTracing(g_rendererMode);
	SceneRay_Invalidate(g_rayRenderer);
	YMGUI_Label_SetText(g_rendererProgress,g_rendererMode?"光追预览":"");
	setStatus(g_rendererMode?"光线追踪：静止后逐步细化；光源的启用阴影控制遮挡阴影":"已切换为光栅化渲染");
}

static void renderRayWireframes(void)
{
	gre_line3d lines[192];uint32 count=0;
	for(GRE_ListNode node=g_objectsList.listhead;node;node=node->next)
		for(GRE_Object4d o=node->data;o;o=o->nextObject) {
			if(!o->isVisible||!o->wireFrame)continue;
			for(int i=0;i<o->polygonNum;i++) {
				GRE_Polygon4d p=o->polygonList+i;
				for(int j=0;j<p->num;j++) {
					if(p->index[j]>=o->pointNum||p->index[(j+1)%p->num]>=o->pointNum)continue;
					lines[count++]=(gre_line3d){o->pointList[p->index[j]].pos,o->pointList[p->index[(j+1)%p->num]].pos,{225,225,225},1};
					if(count==192){YMGRE_Camera_LineList_Rendering(g_activeCamera->camera,lines,count,1);count=0;}
				}
			}
		}
	if(count)YMGRE_Camera_LineList_Rendering(g_activeCamera->camera,lines,count,1);
}

static void syncRayMaterials(void)
{
    static unsigned serial;
    for(int i=0;i<32;i++) {
        SceneEditorObject* o=g_scene+i;if(!o->active||o->kind!=SCENE_OBJECT_MESH)continue;
        unsigned partIndex=0;
        for(GRE_Object4d part=o->mesh;part;part=part->nextObject,partIndex++) {
            GRE_Material m=YMGRE_Material_Find(&g_importContext.MaterialList,part->materiaName);if(!m)continue;
            if(!o->rayType&&(!m->advanced||!m->advanced->rayType))continue;
            /* Imported objects can share texture materials; editing one must not edit its neighbours. */
            if(o->primitiveKind==0xFF) {
                char name[64],prefix[48];snprintf(prefix,sizeof(prefix),"scene_ray_%d_%u_",i,partIndex);
                if(strncmp(part->materiaName,prefix,strlen(prefix))) {
                    snprintf(name,sizeof(name),"%s%u",prefix,++serial);
                    GRE_Material copy=YMGRE_Material_Find(&g_importContext.MaterialList,name);
                    if(!copy) {
                        copy=YMGRE_Creat_Material(name);if(!copy)continue;
                        copy->ambient=m->ambient;copy->diffuse=m->diffuse;copy->specular=m->specular;copy->unlit=m->unlit;
                        copy->width=m->width;copy->height=m->height;
                        if(m->pixel){size_t bytes=(size_t)m->width*m->height*sizeof(GRErgb24);copy->pixel=GRE_ImageBuff_Malloc(bytes);if(copy->pixel)memcpy(copy->pixel,m->pixel,bytes);}
                        if(m->advanced){copy->advanced=GRE_malloc0(sizeof(*copy->advanced));if(copy->advanced){*copy->advanced=*m->advanced;copy->advanced->normalPixel=NULL;
                            if(m->advanced->normalPixel){size_t bytes=(size_t)m->advanced->normalWidth*m->advanced->normalHeight*sizeof(GRErgb24);
                                copy->advanced->normalPixel=GRE_ImageBuff_Malloc(bytes);if(copy->advanced->normalPixel)memcpy(copy->advanced->normalPixel,m->advanced->normalPixel,bytes);}}}
                        if((m->pixel&&!copy->pixel)||(m->advanced&&!copy->advanced)||(m->advanced&&m->advanced->normalPixel&&!copy->advanced->normalPixel)){YMGRE_Free_Material(copy);continue;}
                        YMGRE_List_Append(&g_importContext.MaterialList,sizeof(GRE_Material),copy);
                    }
                    char* materialName=GRE_malloc1(strlen(name)+1);if(!materialName)continue;strcpy(materialName,name);
                    GRE_free1(part->materiaName);part->materiaName=materialName;m=copy;
                }
            }
            if(!m->advanced){m->advanced=GRE_malloc0(sizeof(*m->advanced));if(m->advanced)memset(m->advanced,0,sizeof(*m->advanced));}
            if(m->advanced){m->advanced->rayType=o->rayType;m->advanced->reflectivity=o->reflectivity;m->advanced->raySpecularStrength=o->mirrorKs;
                m->advanced->ior=o->ior>0?o->ior:1.5f;GYcolor c=o->transmissionColor?o->transmissionColor:0xFFFFFFFF;
                m->advanced->transmissionColor=(GRErgb24){c>>16,c>>8,c};}
        }
    }
}
static void prepareRayRenderer(void)
{
    if(!g_rayRenderer)g_rayRenderer=SceneRay_Create();
    SceneRayPrimitive primitives[32]={0};int count=0;
    for(int i=0;i<32;i++) {
        SceneEditorObject* o=g_scene+i;
        if(!o->active||!o->mesh||!o->analytic||(o->primitiveKind!=SCENE_PLACE_SPHERE&&o->primitiveKind!=SCENE_PLACE_CYLINDER))continue;
        SceneRayPrimitive* p=primitives+count++;p->object=o->mesh;p->type=o->primitiveKind==SCENE_PLACE_SPHERE?1:2;
        p->center=(gre_fvector4d){o->x,o->y,o->z,1};p->radius=(p->type==1?18:15)*o->scale;p->halfHeight=16*o->scale;
        float a=o->rotX*YMGRE_Deg2Rad,b=o->rotY*YMGRE_Deg2Rad,c=o->rotZ*YMGRE_Deg2Rad;
        float cx=cosf(a),sx=sinf(a),cy=cosf(b),sy=sinf(b),cz=cosf(c),sz=sinf(c);
        p->axis[0]=(gre_fvector4d){cz*cy,sz*cy,-sy,0};
        p->axis[1]=(gre_fvector4d){cz*sy*sx-sz*cx,sz*sy*sx+cz*cx,cy*sx,0};
        p->axis[2]=(gre_fvector4d){cz*sy*cx+sz*sx,sz*sy*cx-cz*sx,cy*cx,0};
    }
    SceneRay_SetPrimitives(g_rayRenderer,primitives,count);
}

static void lodStopPreview(void)
{
    if(g_lodPreview.running&&g_lodPreview.camera&&g_lodPreview.camera->active){
        SceneEditorObject* camera=g_lodPreview.camera;
        camera->x=g_lodPreview.savedCamera[0];camera->y=g_lodPreview.savedCamera[1];
        camera->z=g_lodPreview.savedCamera[2];camera->targetX=g_lodPreview.savedCamera[3];
        camera->targetY=g_lodPreview.savedCamera[4];camera->targetZ=g_lodPreview.savedCamera[5];
    }
    YMGRE_Free_Object(g_lodPreview.active);
    YMGRE_Free_Object(g_lodPreview.levels[1]);
    YMGRE_Free_Object(g_lodPreview.levels[2]);
    free(g_lodPreview.image);
    memset(&g_lodPreview,0,sizeof(g_lodPreview));
    if(g_lodPreviewButton)YMGUI_Button_SetText(g_lodPreviewButton,"连续预览");
}

static int lodReadSettings(float *nearPixels,float *middlePixels,float *hysteresis)
{
    char extra;
    if(!g_lodNearInput||!g_lodMiddleInput||!g_lodHysteresisInput||
       sscanf(YMGUI_TextInput_GetText(g_lodNearInput),"%f %c",nearPixels,&extra)!=1||
       sscanf(YMGUI_TextInput_GetText(g_lodMiddleInput),"%f %c",middlePixels,&extra)!=1||
       sscanf(YMGUI_TextInput_GetText(g_lodHysteresisInput),"%f %c",hysteresis,&extra)!=1||
       !isfinite(*nearPixels)||!isfinite(*middlePixels)||!isfinite(*hysteresis)||
       *nearPixels<=*middlePixels||*middlePixels<=0||*hysteresis<0||*hysteresis>=1){
        setStatus("LOD 阈值无效：近 > 中 > 0，死区范围为 0～1");return 0;
    }
    return 1;
}
static int lodReadRatios(float *middleRatio,float *farRatio)
{
    char extra;
    int farImage=YMGUI_Dropdown_GetSelected(g_lodFarKind)==1;
    if(sscanf(YMGUI_TextInput_GetText(g_lodMiddleRatioInput),"%f %c",middleRatio,&extra)!=1||
       !isfinite(*middleRatio)||*middleRatio<=0||*middleRatio>1||
       (!farImage&&(sscanf(YMGUI_TextInput_GetText(g_lodFarRatioInput),"%f %c",farRatio,&extra)!=1||
       !isfinite(*farRatio)||*farRatio<=0||*farRatio>*middleRatio))){
        setStatus("保留比例无效：0 < 远级比例 ≤ 中级比例 ≤ 1");return 0;
    }
    if(farImage)*farRatio=*middleRatio;
    return 1;
}
static int lodPrepareLevels(GRE_Object4d source,GRE_Object4d levels[3],
    float middleRatio,float farRatio,int farImage)
{
    levels[0]=source;
    for(GRE_Object4d part=source;part;part=part->nextObject)
        if(part->lightmap){setStatus("带逐面光照贴图的模型暂不能简化，请先导出普通模型");return 0;}
    levels[1]=YMGRE_LOD_SimplifyMesh(source,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=middleRatio,.prune_components=1},NULL);
    levels[2]=farImage?NULL:YMGRE_LOD_SimplifyMesh(source,
        (YMGRE_LOD_SimplifyOptions){.target_ratio=farRatio,.prune_components=1},NULL);
    if(!levels[1]||(!farImage&&!levels[2])){
        YMGRE_Free_Object(levels[1]);YMGRE_Free_Object(levels[2]);
        levels[1]=levels[2]=NULL;setStatus("LOD 简化失败；检查网格拓扑和贴图");return 0;
    }
    return 1;
}
static int lodObjectBounds(GRE_Object4d mesh,gre_fvector4d *center,float *radius)
{
    gre_fvector4d lo={FLT_MAX,FLT_MAX,FLT_MAX,1},hi={-FLT_MAX,-FLT_MAX,-FLT_MAX,1};
    for(GRE_Object4d part=mesh;part;part=part->nextObject)
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            if(p.x<lo.x)lo.x=p.x;if(p.y<lo.y)lo.y=p.y;if(p.z<lo.z)lo.z=p.z;
            if(p.x>hi.x)hi.x=p.x;if(p.y>hi.y)hi.y=p.y;if(p.z>hi.z)hi.z=p.z;
        }
    *center=(gre_fvector4d){(lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f,(lo.z+hi.z)*.5f,1};
    *radius=.5f*sqrtf((hi.x-lo.x)*(hi.x-lo.x)+(hi.y-lo.y)*(hi.y-lo.y)+(hi.z-lo.z)*(hi.z-lo.z));
    return isfinite(*radius)&&*radius>0;
}
static void lodPositionPreviewCamera(void)
{
    SceneLodPreview* preview=&g_lodPreview;
    GRE_Camera4d camera=preview->camera->camera;
    float planeHeight=camera->perspectPlane.pU-camera->perspectPlane.pD;
    if(!(planeHeight>0))return;
    unsigned phase=preview->frame%64,step=phase<=32?phase:64-phase;
    float fraction=(float)step/32.0f;
    float nearPixels=preview->descriptor.levels[0].min_pixels*1.5f;
    float farPixels=fmaxf(1.0f,preview->descriptor.levels[1].min_pixels*.25f);
    float pixels=nearPixels*powf(farPixels/nearPixels,fraction);
    float depth=2*preview->radius*camera->perspectPlane.Dis*camera->img.height/(planeHeight*pixels);
    preview->camera->x=preview->center.x+preview->direction.x*depth;
    preview->camera->y=preview->center.y+preview->direction.y*depth;
    preview->camera->z=preview->center.z+preview->direction.z*depth;
    preview->camera->targetX=preview->center.x;
    preview->camera->targetY=preview->center.y;
    preview->camera->targetZ=preview->center.z;
}
static GRE_ListNode lodPreviewNode(void)
{
    SceneLodPreview* preview=&g_lodPreview;
    if(!preview->running)return NULL;
    if(!preview->object->active||preview->object->mesh!=preview->source||
       preview->camera!=g_activeCamera){lodStopPreview();return NULL;}
    const YMGRE_LOD_Level* level=YMGRE_LOD_SelectCamera(&preview->descriptor,
        &preview->selection,g_activeCamera->camera,preview->center,preview->radius);
    if(!level)return NULL;
    if(level->kind==YMGRE_LOD_IMAGE){
        YMGRE_Free_Object(preview->active);preview->active=NULL;
        preview->activeLevel=preview->selection.level;
        for(GRE_ListNode node=g_objectsList.listhead;node;node=node->next)
            if(node->data==preview->source)return node;
        lodStopPreview();return NULL;
    }
    if(!preview->active||preview->activeLevel!=preview->selection.level){
        GRE_Object4d replacement=YMGRE_Object_Clone((GRE_Object4d)level->resource);
        if(!replacement){setStatus("LOD 预览网格克隆失败");lodStopPreview();return NULL;}
        int prepared=1;
        for(GRE_Object4d part=replacement;part;part=part->nextObject){
            part->WorldCoordinate=preview->center;
            part->boundType=GRE_Bounding_Sphere_R;part->BoundingSphereR=0;
            for(int i=0;i<part->pointNum;i++){
                gre_fvector4d p=part->pointList[i].pos;
                float dx=p.x-preview->center.x,dy=p.y-preview->center.y,dz=p.z-preview->center.z;
                float distance=sqrtf(dx*dx+dy*dy+dz*dz);
                if(distance>part->BoundingSphereR)part->BoundingSphereR=distance;
            }
            if(!YMGRE_Object_GenerateVertexAttributes(part)){prepared=0;break;}
        }
        if(!prepared){YMGRE_Free_Object(replacement);setStatus("LOD 预览顶点准备失败");lodStopPreview();return NULL;}
        YMGRE_Free_Object(preview->active);preview->active=replacement;
        preview->activeLevel=preview->selection.level;
    }
    for(GRE_ListNode node=g_objectsList.listhead;node;node=node->next)
        if(node->data==preview->source)return node;
    lodStopPreview();return NULL;
}

static void renderScene(void)
{
	if (g_activeCamera == NULL) return;
	if(g_lodPreview.running)lodPositionPreviewCamera();
	syncCamera(g_activeCamera);
	for (GRE_ListNode node = g_objectsList.listhead; node != NULL; node = node->next) {
		GRE_Object4d object = (GRE_Object4d)node->data;
		if (object != NULL && object->pointList_wN == NULL) YMGRE_Object_GenerateVertexAttributes(object);
		for (int si = 0; si < 32; ++si) {
			SceneEditorObject *so = &g_scene[si];
			if (!so->active || so->kind != SCENE_OBJECT_MESH || so->mesh != object || so->primitiveKind==0xFF) continue;
			if (object->materiaName == NULL || object->materiaName[0] == '\0') {
				char name[96]; snprintf(name, sizeof(name), "scene_object_%d", si);
				GRE_free1(object->materiaName);
				object->materiaName = GRE_malloc1(strlen(name) + 1);
				if (object->materiaName != NULL) strcpy(object->materiaName, name);
			}
			GRE_Material material = YMGRE_Material_Find(&g_importContext.MaterialList, object->materiaName);
			if (material == NULL && object->materiaName != NULL) {
				material = YMGRE_Creat_Material(object->materiaName);
				if (material != NULL) {
					GRErgb24 rgb = {(uint8)(so->color >> 16), (uint8)(so->color >> 8), (uint8)so->color};
					material->ambient = rgb; material->diffuse = rgb; material->specular = (GRErgb24){255,255,255};
					material->width = material->height = 1;
					material->pixel = GRE_ImageBuff_Malloc(sizeof(GRErgb24));
					if (material->pixel != NULL&&!so->texturePath[0]) material->pixel[0] = (GRErgb24){255,255,255};
					material->advanced = GRE_malloc0(sizeof(gre_material_advanced));
					if (material->advanced != NULL) {memset(material->advanced,0,sizeof(*material->advanced));material->advanced->specularPower = (uint8)so->specularPower;}
					YMGRE_List_Append(&g_importContext.MaterialList, sizeof(GRE_Material), material);
				}
			}
			if (material != NULL) {
				GRErgb24 rgb = {(uint8)(so->color >> 16), (uint8)(so->color >> 8), (uint8)so->color};
				material->ambient = rgb; material->diffuse = rgb;
				if (material->pixel != NULL&&!so->texturePath[0]) material->pixel[0] = (GRErgb24){255,255,255};
				if(!material->advanced) {material->advanced=GRE_malloc0(sizeof(gre_material_advanced));
					if(material->advanced)memset(material->advanced,0,sizeof(*material->advanced));}
				if (material->advanced != NULL) material->advanced->specularPower = (uint8)so->specularPower;
			}
			break;
		}
		for (int i = 0; i < 32; ++i) {
			if (g_scene[i].active && g_scene[i].kind == SCENE_OBJECT_MESH && g_scene[i].mesh == object) {
				for (GRE_Object4d part = object; part != NULL; part = part->nextObject)
					part->wireFrame = g_scene[i].wireframe;
				break;
			}
		}
	}
	syncRayMaterials();
	invalidateStaleBakes();
	GRE_ListNode previewNode=lodPreviewNode();
	if(previewNode)previewNode->data=g_lodPreview.active;
	if(g_rendererMode==1) {
		prepareRayRenderer();
		int progress=SceneRay_Render(g_rayRenderer,g_activeCamera->camera,&g_objectsList,&g_lightsList,&g_importContext.MaterialList);
        g_rayProgress=progress;
		if(progress<0) {
			g_rendererMode=0;YMGUI_Dropdown_SetSelected(g_rendererSelector,0);
			SceneEditorInspector_SetRayTracing(0);
			YMGUI_Label_SetText(g_rendererProgress,"");setStatus("光追缓冲分配失败，已恢复光栅化");
		} else {
			char text[64];snprintf(text,sizeof(text),progress==100?"光追完成":"光追细化 %d%%",progress);
			YMGUI_Label_SetText(g_rendererProgress,text);renderRayWireframes();
		}
	}
	if(g_rendererMode==0)YMGRE_Camera_TanglePipline_wN(g_activeCamera->camera,
		&g_lightsList, &g_objectsList, &g_importContext.MaterialList, g_workspace);
	if(previewNode){
		if(!g_lodPreview.active&&g_lodPreview.image)
			grass_impostor_draw(g_lodPreview.image,g_activeCamera->camera,0,0,0,1,0);
		previewNode->data=g_lodPreview.source;
		if(g_lodPreview.frame%4==0){
			char status[180];snprintf(status,sizeof(status),"LOD 预览：第 %u 层，%u 面，投影 %.0f 像素",
				(unsigned)g_lodPreview.selection.level,
				g_lodPreview.active?(unsigned)g_lodPreview.active->polygonNum:2u,
				YMGRE_LOD_ProjectedDiameter(g_activeCamera->camera,g_lodPreview.center,g_lodPreview.radius));
			setStatus(status);
		}
		g_lodPreview.frame++;
	}
	addSelectionLines();
	addTransformGizmo();
	YMGRE_Camera_LineList_Rendering(g_activeCamera->camera, g_lines, g_lineCount, 1);
}

static void renderEditorFrame(void)
{
    /* The UV modal covers the viewport. Keep its last image until the modal closes;
       invalidating it every frame also makes the overlay redraw unnecessarily. */
    if(!SceneUvEditor_IsOpen()) {
        renderScene();YMGUI_Obj_Invalidate(g_image);
    }
    SceneUvEditor_Tick();YMGUI_Refresh(g_ui->host.context);
}

static void layoutPanels(void)
{
	int left = (g_leftPanel != NULL && !(g_leftPanel->state & GY_STATE_Hidden)) ? 244 : 0;
	int right = (g_rightPanel != NULL && !(g_rightPanel->state & GY_STATE_Hidden)) ? 212 : 0;
	if (g_centerPanel != NULL) g_centerPanel->area = (GYrect){ left, 64, 1024-left-right, 648 };
	if (g_centerPanel != NULL && g_image != NULL) g_image->area = (GYrect){0,0,g_centerPanel->area.w,648};
	if (g_ui != NULL && g_ui->viewport != NULL) g_ui->viewport->area = (GYrect){0,0,g_centerPanel->area.w,648};
	if (g_ui != NULL && g_ui->viewportCamera != NULL) g_ui->viewportCamera->area.x = g_centerPanel->area.w - 260;
	if (g_centerPanel != NULL) {
		YMGRE_RenderTarget_Init(&g_target, g_centerPanel->area.w, 648, g_colorBuffer, g_depthBuffer);
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
		/* 绝对定位：鼠标当前极角就是轴的目标角度，不再累加拖动变化量。 */
		SceneEditorObject_SetRotationAxis(g_selected, axis, deltaX);
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
			if(gizmoMode>=VIEW_DRAG_ROTATE_X&&gizmoMode<=VIEW_DRAG_ROTATE_Z){
				g_rotateAxis=(int8)(gizmoMode-VIEW_DRAG_ROTATE_X);
				g_rotateStartAngle=g_rotateAxis==0?g_selected->rotX:(g_rotateAxis==1?g_selected->rotY:g_selected->rotZ);
				float32 depth; gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1};
				if(projectPoint(&center,&g_rotateCenterX,&g_rotateCenterY,&depth)) {
					GYrect viewportArea; YMGUI_Obj_GetAbsArea(g_ui->viewport,&viewportArea);
					g_rotateCenterX+=viewportArea.x; g_rotateCenterY+=viewportArea.y;
					gre_fvector4d reference=center;
					/* 与环的 0° 起点保持一致：X 环从 +Y，Y/Z 环从 +X 开始。 */
					if(g_rotateAxis==0)reference.y+=1.0f; else reference.x+=1.0f;
					float32 ux,uy,unused;
					if(projectPoint(&reference,&ux,&uy,&unused)) {
						float32 length=sqrtf((ux-g_rotateCenterX)*(ux-g_rotateCenterX)+(uy-g_rotateCenterY)*(uy-g_rotateCenterY));
						g_rotateBasisUX=length>0.001f?(ux-g_rotateCenterX)/length:1.0f;
						g_rotateBasisUY=length>0.001f?(uy-g_rotateCenterY)/length:0.0f;
					}
					g_rotateStartPointerAngle=rotatePointerAngle((float32)x,(float32)y);
				}
			}
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
			if(g_ui->dragMode>=VIEW_DRAG_ROTATE_X&&g_ui->dragMode<=VIEW_DRAG_ROTATE_Z){
				uint8 axis=(uint8)(g_ui->dragMode-VIEW_DRAG_ROTATE_X); gre_fvector4d center={g_selected->x,g_selected->y,g_selected->z,1},hit;
				float32 targetAngle=0.0f; if(projectMouseToRotationPlane(axis,center,&hit))targetAngle=projectedPlaneAngle(axis,center,hit);
				while(targetAngle>180.0f)targetAngle-=360.0f; while(targetAngle<-180.0f)targetAngle+=360.0f;
				dragSelectedAxis(g_ui->dragMode,targetAngle,dy);
			}
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
	object->mirrorKs=0.2f;
	object->specularPower=30.0f;
	object->reflectivity=.9f;object->ior=1.5f;object->transmissionColor=0xFFFFFFFF;
	if (result->kind <= SCENE_PLACE_CAPSULE) {
		object->kind=SCENE_OBJECT_MESH; object->mesh=result->mesh;
		object->primitiveKind=(uint8)result->kind; object->detailA=result->detailA; object->detailB=result->detailB;
        /* New primitives use the same UV mode as the explicit unwrap command. */
        if(!result->preserveSourceUv) {
            char uvError[192];
            if(!SceneEditorObject_GenerateUv(object,result->kind==SCENE_PLACE_SPHERE?2:5,NULL,uvError,sizeof(uvError))) {
                YMGRE_Free_Object(object->mesh);object->mesh=NULL;object->active=0;setStatus(uvError);return;
            }
        }
		for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) part->mirrorKs = object->mirrorKs;
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
	if(g_lodPreview.running&&(object==g_lodPreview.object||object==g_lodPreview.camera))lodStopPreview();
	if (object==g_activeCamera){g_historyBatch++;switchCamera(g_mainCamera,NULL);g_historyBatch--;}
	if (object->kind==SCENE_OBJECT_MESH) removeListItem(&g_objectsList,object->mesh,YMGRE_Free_Object);
	else if (object->kind==SCENE_OBJECT_LIGHT) removeListItem(&g_lightsList,object->light,YMGRE_Free_Light);
	else if (object->camera!=NULL) YMGRE_Free_Camera(object->camera);
	YMGUI_TreeView_RemoveNode(g_ui->tree,object->treeNode);
	memset(object,0,sizeof(*object));
	clearSelection("对象已删除");
	g_sceneDirty = 1;historyCommit();
}

static int uvApplyTexture(SceneEditorObject* object,const GRErgb24* pixels,unsigned width,unsigned height,char* error,size_t capacity,void* user)
{
    (void)user;if(!object||!object->mesh)return 0;
    if(pixels) {
        const char* root=getenv("YMGRE_BAKE_DIR");if(!root||!*root)root="baked_scene";char path[4096];
        if(!SceneEditorTexture_Save(pixels,width,height,root,path,sizeof(path),error,capacity))return 0;
        SceneEditorObject staged=*object;staged.color=0xFFFFFFFF;
        if(!SceneEditorTexture_Load(&staged,&g_importContext.MaterialList,path,error,capacity)){unlink(path);return 0;}
        object->color=staged.color;strcpy(object->texturePath,staged.texturePath);
    }
    for(GRE_Object4d mesh=object->mesh;mesh;mesh=mesh->nextObject){YMGRE_Free_Lightmap(mesh->lightmap);mesh->lightmap=NULL;}
    object->bakePath[0]=0;object->bakeFingerprint=object->rayBakeSignature=0;
    SceneEditorInspector_SetObject(object);return 1;
}

static void inspectorChanged(const char* status, void* userData)
{
	(void)userData;
	if (g_selected!=NULL) { normalizeSpotDirection(g_selected); if(g_selected->kind==SCENE_OBJECT_CAMERA)syncCamera(g_selected); }
	if (g_rendererMode) {
		SceneRay_Invalidate(g_rayRenderer);g_rayProgress=0;
		YMGUI_Label_SetText(g_rendererProgress,"光追预览");
	}
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
    object->reflectivity=.9f;object->ior=1.5f;object->transmissionColor=0xFFFFFFFF;
    GRE_Material importedMaterial=YMGRE_Material_Find(&g_importContext.MaterialList,mesh->materiaName);
    if(importedMaterial&&importedMaterial->advanced&&importedMaterial->advanced->rayType){GRE_MaterialAdvanced a=importedMaterial->advanced;
        object->rayType=a->rayType;object->reflectivity=a->reflectivity;object->ior=a->ior;object->mirrorKs=a->raySpecularStrength;
        object->specularPower=a->specularPower?a->specularPower:30;
        for(GRE_Object4d part=mesh;part;part=part->nextObject)part->mirrorKs=object->mirrorKs;
        object->transmissionColor=GY_ARGB(0xFF,a->transmissionColor.R,a->transmissionColor.G,a->transmissionColor.B);}

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
		.color = object->color, .wireframe = object->wireframe, .legacyCapsule=object->autoUv!=5 };
	snprintf(result.name, sizeof(result.name), "%s", object->name);
	GRE_Object4d replacement = SceneEditorPlace_CreateMesh(&result);
	if (replacement == NULL) return 0;
	if (object->autoUv) {
		char error[192];
		SceneEditorObject staged=*object;staged.mesh=replacement;staged.detailA=detailA;staged.detailB=detailB;
		staged.x=staged.y=staged.z=0;staged.rotX=staged.rotY=staged.rotZ=0;staged.scale=1;
		if (!SceneEditorObject_GenerateUv(&staged,object->autoUv,NULL,error,sizeof(error))) {YMGRE_Free_Object(replacement);setStatus(error);return 0;}
	}
	SceneEditorPlace_TransformMesh(replacement, object->x, object->y, object->z,
		0, object->scale, object->wireframe);
	SceneEditorObject pose=*object;pose.mesh=replacement;pose.rotX=pose.rotY=pose.rotZ=0;
	float rotation[3]={object->rotX,object->rotY,object->rotZ};
	for(uint8 axis=0;axis<3;axis++)SceneEditorObject_SetRotationAxis(&pose,axis,rotation[axis]);
    if(object->texturePath[0]) {
        SceneEditorObject staged=*object;staged.mesh=replacement;char error[192];
        if(!SceneEditorTexture_Load(&staged,&g_importContext.MaterialList,object->texturePath,error,sizeof(error))){YMGRE_Free_Object(replacement);setStatus(error);return 0;}
    }
	GRE_Object4d previous = object->mesh;
	uint8 replaced = 0;
	for (GRE_ListNode node = g_objectsList.listhead; node != NULL; node = node->next)
		if (node->data == previous) { node->data = replacement; replaced = 1; break; }
	if (!replaced) { YMGRE_Free_Object(replacement); return 0; }
	object->mesh = replacement;
	object->uvEdited=0;
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
		SceneEditorPlaceResult result={0};result.preserveSourceUv=1;
		result.kind=source->kind==SCENE_OBJECT_CAMERA?SCENE_PLACE_CAMERA:
			(source->kind==SCENE_OBJECT_LIGHT?(source->lightType==GRE_SpotLight?SCENE_PLACE_SPOT_LIGHT:SCENE_PLACE_POINT_LIGHT):
			(SceneEditorPlaceKind)source->primitiveKind);
		result.x=source->x+10;result.y=source->y;result.z=source->z+10;
		result.targetX=source->targetX;result.targetY=source->targetY;result.targetZ=source->targetZ;
		result.scale=source->scale;result.rotationY=source->rotY;result.strength=source->strength;
		result.color=source->color;result.wireframe=source->wireframe;result.shadowsEnabled=source->shadowsEnabled;
		result.detailA=source->detailA;result.detailB=source->detailB;result.legacyCapsule=source->autoUv!=5;
		snprintf(result.name,sizeof(result.name),"%s",copyName);snprintf(result.type,sizeof(result.type),"%s",source->type);
		if(source->kind==SCENE_OBJECT_MESH){
			result.mesh=SceneEditorPlace_CreateMesh(&result);
			if(result.mesh==NULL){g_historyBatch--;setStatus("复制失败：无法创建网格");return;}
			SceneEditorPlace_TransformMesh(result.mesh,result.x,result.y,result.z,result.rotationY,result.scale,result.wireframe);
		}
		objectPlaced(&result,NULL);
		if(g_selected!=NULL)g_selected->fixed=source->fixed;
	}
    if(g_selected&&source->kind==SCENE_OBJECT_MESH){g_selected->rayType=source->rayType;g_selected->analytic=source->analytic;
        g_selected->reflectivity=source->reflectivity;g_selected->ior=source->ior;g_selected->transmissionColor=source->transmissionColor;
        g_selected->mirrorKs=source->mirrorKs;g_selected->specularPower=source->specularPower;
        for(GRE_Object4d part=g_selected->mesh;part;part=part->nextObject)part->mirrorKs=source->mirrorKs;
        if(source->autoUv) {
            char uvError[192];
            if(SceneEditorObject_GenerateUv(g_selected,source->autoUv,NULL,uvError,sizeof(uvError)))g_selected->autoUv=source->autoUv;
            else {g_historyBatch--;g_sceneDirty=1;historyCommit();setStatus(uvError);return;}
        }
        if(source->uvEdited) {
            SceneUvData* uv=SceneUvData_Capture(source->mesh);
            int applied=uv&&SceneUvData_Apply(g_selected->mesh,uv);free(uv);
            if(!applied){g_historyBatch--;g_sceneDirty=1;historyCommit();setStatus("复制对象的手动 UV 失败");return;}
            g_selected->uvEdited=1;
        }
        if(source->texturePath[0]) {
            char error[192];if(!SceneEditorTexture_Load(g_selected,&g_importContext.MaterialList,source->texturePath,error,sizeof(error))){g_historyBatch--;historyCommit();setStatus(error);return;}
        }
        SceneEditorInspector_SetObject(g_selected);}
	g_historyBatch--;g_sceneDirty=1;historyCommit();setStatus("对象已复制");
}

static int saveBakeResource(SceneEditorObject* object, GRE_Lightmap map, uint64_t fingerprint,
	const char* root, char* resource, size_t pathCapacity, char* error, size_t errorCapacity)
{
	gre_material surface=bakeSurface(object);
	if(!SceneBake_SaveMaterial(map,fingerprint,&surface,root,resource,pathCapacity,error,errorCapacity))return 0;
	char directory[SCENE_EDITOR_PATH_CAPACITY];snprintf(directory,sizeof(directory),"%s",resource);
	*strrchr(directory,'/')=0;gre_fvector4d origin={object->x,object->y,object->z,0};
	return SceneModel_ExportBaked(object->mesh,map,&surface,&origin,directory,g_bakedModelPath,
		sizeof(g_bakedModelPath),error,errorCapacity);
}

static void bakeObject(SceneEditorObject* object, void* userData)
{
	(void)userData;
	if (object == NULL || object->kind != SCENE_OBJECT_MESH) return;
	char error[256],resource[SCENE_EDITOR_PATH_CAPACITY];
	GRErgb24 specularColor;uint8 power;bakeSpecularSettings(object,&specularColor,&power);
	gre_fvector4d view={g_activeCamera->x,g_activeCamera->y,g_activeCamera->z,1};
	gre_material surface=bakeSurface(object);gre_material_advanced advanced={0};
	advanced.specularPower=power;surface.advanced=&advanced;surface.specular=specularColor;
	GRE_Lightmap map=SceneBake_CreateSurface(object->mesh,&g_lightsList,256,
		YMGUI_Checkbox_GetChecked(g_bakeLighting),&view,&surface,error,sizeof(error));
	if(!map) {setStatus(error);return;}
    if(object->rayType&&!map->materialOnly) {
        renderScene();prepareRayRenderer();
        if(!SceneRay_BakeSurface(g_rayRenderer,object->mesh,map,g_activeCamera->camera,&g_objectsList,&g_lightsList,&g_importContext.MaterialList)) {
            YMGRE_Free_Lightmap(map);setStatus("光追烘焙失败，原烘焙结果已保留");return;
        }
    }
	const char* root=getenv("YMGRE_BAKE_DIR");if(!root || !*root)root="baked_scene";
	uint64_t fingerprint=SceneBake_ModeFingerprint(object->mesh,&g_lightsList,map->materialOnly);
	if(!saveBakeResource(object,map,fingerprint,root,resource,sizeof(resource),error,sizeof(error))) {
		YMGRE_Free_Lightmap(map);setStatus(error);return;
	}
	YMGRE_Free_Lightmap(object->mesh->lightmap);object->mesh->lightmap=map;
	object->bakeFingerprint=fingerprint;
	object->rayBakeSignature=object->rayType&&!map->materialOnly?rayBakeSignature():0;
	snprintf(object->bakePath,sizeof(object->bakePath),"%s",resource);
	g_sceneDirty=1;historyCommit();
	char status[256];snprintf(status,sizeof(status),"「%.48s」%s，已生成模型、材质和彩色贴图",object->name,map->materialOnly?"材质烘焙完成（不含光照）":(object->rayType?"烘焙完成（当前视角反射／折射已固定）":"烘焙完成（含光照和当前视角高光）"));
	YMGUI_TextInput_SetText(g_bakeExportPath,g_bakedModelPath);
	setStatus(status);if(g_image)YMGUI_Obj_Invalidate(g_image);
	printf("scene bake: object=%s resource=%s triangles=%u: PASS\n",object->name,resource,map->triangleCount);
}

static void bakeSelectedClicked(GYOBJ button)
{
	(void)button;
	if(!g_selected || g_selected->kind!=SCENE_OBJECT_MESH){setStatus("请先选中要烘焙的网格物体");return;}
	bakeObject(g_selected,NULL);
}

static void bakeExportResult(GYOBJ dialog, uint8 accepted, const char* path, void* userData)
{
	(void)dialog;(void)userData;
	SceneEditorObject* object=g_bakeExportObject;g_bakeExportObject=NULL;
	if(!accepted || !path){setStatus("已取消导出");return;}
	if(g_exportModel==2){
		lodStopPreview();
		if(!object||!object->active||!object->mesh){setStatus("LOD 导出对象已不存在");return;}
		float nearPixels,middlePixels,hysteresis;
		if(!lodReadSettings(&nearPixels,&middlePixels,&hysteresis))return;
		float middleRatio,farRatio;
		if(!lodReadRatios(&middleRatio,&farRatio))return;
		int farImage=YMGUI_Dropdown_GetSelected(g_lodFarKind)==1;
		GRE_Object4d levels[3]={0};
		if(!lodPrepareLevels(object->mesh,levels,middleRatio,farRatio,farImage))return;
		grass_impostor *image=farImage?malloc(sizeof(*image)):NULL;
		if(farImage&&(!image||!SceneLodImage_Bake(image,object->mesh,
			&g_importContext.MaterialList,&g_lightsList))){
			free(image);YMGRE_Free_Object(levels[1]);setStatus("八方向图生成失败");return;
		}
		gre_fvector4d origin={object->x,object->y,object->z,0};
		gre_material surface={0};surface.ambient=surface.diffuse=(GRErgb24){
			(object->color>>16)&255,(object->color>>8)&255,object->color&255};
		GRE_List materials[3]={&g_importContext.MaterialList,&g_importContext.MaterialList,
			&g_importContext.MaterialList};
		char resource[SCENE_EDITOR_PATH_CAPACITY],error[256];
		int exported=SceneModel_ExportLODWithImageNamed(levels,materials,image,&surface,&origin,path,
			object->name,
			nearPixels,middlePixels,hysteresis,resource,sizeof(resource),error,sizeof(error));
		YMGRE_Free_Object(levels[1]);YMGRE_Free_Object(levels[2]);free(image);
		if(!exported){setStatus(error);return;}
		snprintf(g_bakeExportDirectory,sizeof(g_bakeExportDirectory),"%s",path);
		YMGUI_TextInput_SetText(g_bakeExportPath,resource);
		setStatus(farImage?"LOD 已导出：近/中网格、八方向图和 object.lod，路径见下方":
			"LOD 已导出：三级网格、材质、贴图和 object.lod，路径见下方");
		printf("scene LOD export: %s: PASS\n",resource);return;
	}
	if(g_exportModel) {
		renderScene();
		if(!object || !object->active || !object->mesh){setStatus("导出对象已不存在");return;}
		char stem[81],resource[SCENE_EDITOR_PATH_CAPACITY],error[256];
		const char* name=strrchr(object->sourcePath,'/');name=name?name+1:object->sourcePath;
		if(!*name)name="Model";
		snprintf(stem,sizeof(stem),"%s",name);char* extension=strrchr(stem,'.');if(extension)*extension=0;
		gre_fvector4d origin={object->x,object->y,object->z,0};
		gre_material surface={0};surface.ambient=surface.diffuse=(GRErgb24){
			(object->color>>16)&255,(object->color>>8)&255,object->color&255};
		if(!SceneModel_Export(object->mesh,&g_importContext.MaterialList,&surface,
			&origin,path,stem,resource,sizeof(resource),error,sizeof(error))){setStatus(error);return;}
		snprintf(g_bakeExportDirectory,sizeof(g_bakeExportDirectory),"%s",path);
		YMGUI_TextInput_SetText(g_bakeExportPath,resource);
		setStatus("模型已导出：.mesh、.material 和 BMP 贴图，完整路径见下方");
		printf("scene model export: %s: PASS\n",resource);return;
	}
	invalidateStaleBakes();
	if(!object || !object->active || !object->mesh || !object->mesh->lightmap ||
		object->bakeFingerprint!=g_bakeExportFingerprint) {
		setStatus("烘焙结果已变化，请重新烘焙后导出");return;
	}
	char resource[SCENE_EDITOR_PATH_CAPACITY],error[256];
	if(!saveBakeResource(object,object->mesh->lightmap,object->bakeFingerprint,path,
		resource,sizeof(resource),error,sizeof(error))) {setStatus(error);return;}
	snprintf(g_bakeExportDirectory,sizeof(g_bakeExportDirectory),"%s",path);
	snprintf(object->bakePath,sizeof(object->bakePath),"%s",resource);
	YMGUI_TextInput_SetText(g_bakeExportPath,g_bakedModelPath);
	g_sceneDirty=1;historyCommit();
	setStatus("烘焙模型、材质和彩色贴图已导出；可直接导入下方 .mesh 文件");
	printf("scene bake export: object=%s material=%s: PASS\n",object->name,resource);
}

static uint8 exportDirectoryEntry(GYOBJ dialog,GYfiledialog_mode mode,
	const char* path,const GYfiledialog_entry* entry,void* userData)
{
	(void)dialog;(void)mode;(void)path;(void)userData;return entry->is_dir;
}

static void showExportDirectory(uint8 model)
{
	invalidateStaleBakes();g_exportModel=model;
	if(!g_selected || g_selected->kind!=SCENE_OBJECT_MESH){setStatus("请先选中要导出的网格物体");return;}
	if(!g_selected->mesh || (!model && !g_selected->mesh->lightmap)){setStatus("当前物体尚未烘焙，请先点击顶部「烘焙」");return;}
	if(!g_bakeExportDialog){setStatus("无法创建导出目录窗口");return;}
	g_bakeExportObject=g_selected;g_bakeExportFingerprint=g_selected->bakeFingerprint;
	const char* initial=g_bakeExportDirectory;
	if(!*initial){initial=getenv("YMGRE_BAKE_DIR");if(!initial || !*initial)initial="baked_scene";}
	char directory[SCENE_EDITOR_PATH_CAPACITY];
	if(!realpath(initial,directory) && !getcwd(directory,sizeof(directory))) {
		g_bakeExportObject=NULL;setStatus("无法读取当前目录");return;
	}
	if(!YMGUI_FileDialog_Show(g_bakeExportDialog,GY_FILE_DIALOG_SELECT_DIRECTORY,directory,"")) {
		g_bakeExportObject=NULL;setStatus("无法打开导出目录，请检查目录权限");return;
	}
	setStatus(model==2?"请选择 LOD 资源导出目录；将生成模型/八方向图和 object.lod":
		(model?"请选择导出目录；模型、材质和贴图将保存到新建的资源文件夹":
		"请选择导出目录；烘焙模型、材质和彩色贴图将保存到新建的资源文件夹"));
}

static void exportModelClicked(GYOBJ button)
{
	(void)button;invalidateStaleBakes();
	showExportDirectory(!g_selected || !g_selected->mesh || !g_selected->mesh->lightmap);
}

static void lodPanelClicked(GYOBJ button)
{
	(void)button;
	if((g_lodPanel->state&GY_STATE_Hidden)&&
	   (!g_selected||g_selected->kind!=SCENE_OBJECT_MESH||!g_selected->mesh)){
		setStatus("请先选中需要制作 LOD 的网格物体");return;
	}
	if(!(g_lodPanel->state&GY_STATE_Hidden)&&g_lodPreview.running)lodStopPreview();
	YMGUI_Obj_SetHidden(g_lodPanel,!(g_lodPanel->state&GY_STATE_Hidden));
}
static void lodPreviewClicked(GYOBJ button)
{
	(void)button;
	if(g_lodPreview.running){lodStopPreview();setStatus("已退出 LOD 连续预览，恢复原模型与机位");return;}
	if(!g_selected||g_selected->kind!=SCENE_OBJECT_MESH||!g_selected->mesh||!g_activeCamera){
		setStatus("请先选中需要预览的网格物体");return;
	}
	if(g_rendererMode!=0){setStatus("请先把视口渲染器切换为光栅化");return;}
	float nearPixels,middlePixels,hysteresis;
	if(!lodReadSettings(&nearPixels,&middlePixels,&hysteresis))return;
	float middleRatio,farRatio;
	if(!lodReadRatios(&middleRatio,&farRatio))return;
	int farImage=YMGUI_Dropdown_GetSelected(g_lodFarKind)==1;
	SceneLodPreview* preview=&g_lodPreview;
	if(!lodPrepareLevels(g_selected->mesh,preview->levels,middleRatio,farRatio,farImage))return;
	if(farImage){
		preview->image=malloc(sizeof(*preview->image));
		if(!preview->image||!SceneLodImage_Bake(preview->image,g_selected->mesh,
			&g_importContext.MaterialList,&g_lightsList)){
			lodStopPreview();setStatus("八方向图生成失败");return;
		}
	}
	if(!lodObjectBounds(g_selected->mesh,&preview->center,&preview->radius)){
		lodStopPreview();setStatus("模型边界无效，无法预览 LOD");return;
	}
	YMGRE_LOD_Init(&preview->descriptor,hysteresis);
	for(int i=0;i<3;i++)
		if(!YMGRE_LOD_Add(&preview->descriptor,i==2&&farImage?YMGRE_LOD_IMAGE:YMGRE_LOD_MESH,
				i==0?nearPixels:i==1?middlePixels:0,NULL,
				i==2&&farImage?(void*)preview->image:(void*)preview->levels[i])){
			lodStopPreview();setStatus("无法建立 LOD 层级");return;
		}
	preview->object=g_selected;preview->source=g_selected->mesh;
	preview->camera=g_activeCamera;preview->activeLevel=255;
	preview->savedCamera[0]=g_activeCamera->x;preview->savedCamera[1]=g_activeCamera->y;
	preview->savedCamera[2]=g_activeCamera->z;preview->savedCamera[3]=g_activeCamera->targetX;
	preview->savedCamera[4]=g_activeCamera->targetY;preview->savedCamera[5]=g_activeCamera->targetZ;
	preview->direction=(gre_fvector4d){g_activeCamera->x-preview->center.x,
		g_activeCamera->y-preview->center.y,g_activeCamera->z-preview->center.z,0};
	float length=sqrtf(preview->direction.x*preview->direction.x+
		preview->direction.y*preview->direction.y+preview->direction.z*preview->direction.z);
	if(length<.001f){preview->direction=(gre_fvector4d){0,.14f,-.99f,0};length=1;}
	preview->direction.x/=length;preview->direction.y/=length;preview->direction.z/=length;
	preview->running=1;
	YMGUI_Button_SetText(g_lodPreviewButton,"停止预览");
	setStatus("LOD 连续预览已启动；相机远近往返，死区内保持当前层级");
}
static void lodExportClicked(GYOBJ button)
{
	(void)button;
	if(g_lodPreview.running)lodStopPreview();
	float nearPixels,middlePixels,hysteresis;
	if(!lodReadSettings(&nearPixels,&middlePixels,&hysteresis))return;
	float middleRatio,farRatio;
	if(!lodReadRatios(&middleRatio,&farRatio))return;
	showExportDirectory(2);
}

static void toggleBakePreview(SceneEditorObject* object, void* userData)
{
	(void)userData;invalidateStaleBakes();
	if(!object || !object->mesh || !object->mesh->lightmap)return;
	if(object->mesh->lightmap->materialOnly){setStatus("此结果未烘焙光照，当前使用实时光照");return;}
	object->mesh->lightmap->enabled=!object->mesh->lightmap->enabled;
	setStatus(object->mesh->lightmap->enabled?"正在预览烘焙光照和固定视角高光":"正在预览实时光照");
	if(g_image)YMGUI_Obj_Invalidate(g_image);
}

static int writeCurrentScene(const char* path,char* error,size_t errorCapacity)
{
	invalidateStaleBakes();
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
	for(uint32 i=0;i<count;++i){free(objects[i].pendingUv);objects[i].pendingUv=NULL;}
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
					.detailB=object->detailB,.color=object->color,.wireframe=object->wireframe,.legacyCapsule=object->autoUv!=5};
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
				free(object->pendingUv);object->pendingUv=NULL;object->active=0;skippedCount++;continue;
			}
			float32 savedRotation[3]={object->rotX,object->rotY,object->rotZ};
			SceneEditorPlace_TransformMesh(object->mesh,object->x,object->y,object->z,
				0,object->scale,object->wireframe);
			object->rotX=object->rotY=object->rotZ=0;
			for(uint8 axis=0;axis<3;axis++)SceneEditorObject_SetRotationAxis(object,axis,savedRotation[axis]);
            if(object->autoUv&&!SceneEditorObject_GenerateUv(object,object->autoUv,NULL,error,sizeof(error))) {
                YMGRE_Free_Object(object->mesh);object->mesh=NULL;break;
            }
			if(object->pendingUv) {
                int applied=SceneUvData_Apply(object->mesh,object->pendingUv);
                free(object->pendingUv);object->pendingUv=NULL;
                if(!applied){YMGRE_Free_Object(object->mesh);object->mesh=NULL;snprintf(error,sizeof(error),"手动 UV 与源网格拓扑不匹配");break;}
                object->uvEdited=1;
            }
            if(object->texturePath[0]&&!SceneEditorTexture_Load(object,&stagedImport.MaterialList,object->texturePath,error,sizeof(error))) {
                YMGRE_Free_Object(object->mesh);object->mesh=NULL;break;
            }
            for (GRE_Object4d part=object->mesh; part!=NULL; part=part->nextObject)
				part->mirrorKs=object->mirrorKs;
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
	if(built!=count){freeStagedScene(staged,count,&stagedObjects,&stagedLights,&stagedImport);
		SceneEditorIo_SetError(error);showSceneOpenError(error);setStatus("场景打开失败，已保留当前场景");return;}
	for(uint32 i=0;i<count;i++) if(staged[i].active && staged[i].kind==SCENE_OBJECT_MESH && staged[i].bakePath[0]) {
		SceneEditorObject* object=&staged[i];
		GRE_Material bakedMaterial=NULL;
		object->mesh->lightmap=SceneBake_LoadMaterial(object->bakePath,object->mesh,&stagedLights,&bakedMaterial,error,sizeof(error));
		if(!object->mesh->lightmap) {
			freeStagedScene(staged,count,&stagedObjects,&stagedLights,&stagedImport);
			showSceneOpenError(error);setStatus(error);return;
		}
		if(bakedMaterial) {
			/* Unique material names preserve separate object albedo resources. */
			char* name=GRE_malloc1(strlen(bakedMaterial->name)+1);
			if(!name){YMGRE_Free_Material(bakedMaterial);freeStagedScene(staged,count,&stagedObjects,&stagedLights,&stagedImport);setStatus("材质加载内存不足");return;}
			strcpy(name,bakedMaterial->name);GRE_free1(object->mesh->materiaName);object->mesh->materiaName=name;
			YMGRE_List_Append(&stagedImport.MaterialList,sizeof(GRE_Material),bakedMaterial);
		}
		object->bakeFingerprint=SceneBake_ModeFingerprint(object->mesh,&stagedLights,object->mesh->lightmap->materialOnly);
	}
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
	lodStopPreview();clearSelection("正在打开场景");
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
	/* Restore runtime material instances before validating reflected-scene bake signatures. */
	renderScene();
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

static SceneEditorObject* uvHistoryRestore(SceneEditorObject* object,int redo,void* user)
{
    (void)user;int ordinal=0,found=0;
    for(int i=0;i<32;i++)if(g_scene[i].active){if(g_scene+i==object){found=1;break;}ordinal++;}
    if(!found)return NULL;
    if((redo&&g_historyIndex+1>=g_historyCount)||(!redo&&!g_historyIndex))return object;
    if(!historyRestore(redo?g_historyIndex+1:g_historyIndex-1))return object;
    for(int i=0;i<32;i++)if(g_scene[i].active&&ordinal--==0) {
        if(g_scene[i].kind!=SCENE_OBJECT_MESH)return NULL;
        selectObject(g_scene+i,redo?"已重做 UV 修改":"已撤销 UV 修改");return g_scene+i;
    }
    return NULL;
}

static void undoClicked(GYOBJ button){(void)button;if(g_historyIndex>0){if(historyRestore(g_historyIndex-1))setStatus("已撤销场景修改");}else setStatus("没有可撤销的修改");}
static void redoClicked(GYOBJ button){(void)button;if(g_historyIndex+1<g_historyCount){if(historyRestore(g_historyIndex+1))setStatus("已重做场景修改");}else setStatus("没有可重做的修改");}

static void createNewScene(void)
{
	lodStopPreview();
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
	g_leftPanel=YMGUI_Creat_Obj_Creat(root,0,56,236,760);
	g_centerPanel=YMGUI_Creat_Obj_Creat(root,244,64,560,648);
	/* Keep the original inspector layout while allowing its bottom controls to receive events. */
	g_rightPanel=YMGUI_Creat_Obj_Creat(root,812,56,212,760);
	GYcolor fg=GY_ARGB(0xFF,0xE4,0xEA,0xF2),muted=GY_ARGB(0xFF,0x9A,0xA7,0xB8),cyan=GY_ARGB(0xFF,0x6E,0xC8,0xE8);
	YMGUI_Obj_SetBgColor(bar,GY_ARGB(0xFF,0x16,0x1C,0x28));YMGUI_Obj_SetBgColor(g_leftPanel,GY_ARGB(0xFF,0x27,0x31,0x42));
	YMGUI_Obj_SetBgColor(g_centerPanel,GY_ARGB(0xFF,0x0E,0x13,0x1C));YMGUI_Obj_SetBgColor(g_rightPanel,GY_ARGB(0xFF,0x32,0x2B,0x3E));
	toolbarButton(bar,18,"编辑",editClicked);SceneEditorPlace_Build(bar,104,ctx,objectPlaced,ui);
	SceneEditorColor_Build(ctx);toolbarButton(bar,190,"新建",addClicked);toolbarButton(bar,276,"保存",saveClicked);toolbarButton(bar,362,"定位",frameClicked);toolbarButton(bar,448,"导入",importClicked);
	transformButton(bar,540,"移动",0);transformButton(bar,598,"旋转",1);transformButton(bar,656,"缩放",2);updateTransformButtons();
	toolbarButton(bar,724,"烘焙",bakeSelectedClicked);
	GYOBJ exportButton=YMGUI_Creat_Button_Creat(bar,810,12,118,30);
	YMGUI_Button_SetText(exportButton,"导出模型");YMGUI_Button_SetClicked(exportButton,exportModelClicked);
	YMGUI_Button_SetColors(exportButton,GY_ARGB(0xFF,0x2E,0x79,0x96),GY_ARGB(0xFF,0x3C,0x86,0xB8));
	GYOBJ lodButton=YMGUI_Creat_Button_Creat(bar,932,12,84,30);
	YMGUI_Button_SetText(lodButton,"制作LOD");YMGUI_Button_SetClicked(lodButton,lodPanelClicked);

	g_editMenu=YMGUI_Creat_Obj_Creat(root,12,48,170,300);YMGUI_Obj_SetBgColor(g_editMenu,GY_ARGB(0xFF,0x25,0x2D,0x3C));YMGUI_Obj_SetHidden(g_editMenu,1);
	menuButton(g_editMenu,8,"打开场景",openClicked);menuButton(g_editMenu,40,"保存场景",saveClicked);
	menuButton(g_editMenu,72,"撤销",undoClicked);menuButton(g_editMenu,104,"重做",redoClicked);
	menuButton(g_editMenu,136,"切换层级",leftClicked);
	menuButton(g_editMenu,200,"切换检查器",rightClicked);menuButton(g_editMenu,232,"重置视图",resetClicked);
	g_referenceToggle=menuButton(g_editMenu,264,"隐藏坐标轴和网格",referenceClicked);
	YMGUI_Layout_Stack(g_editMenu,GY_LAYOUT_VER,4,8,GY_CROSS_START);
	label(g_leftPanel,16,14,190,22,"场景层级",fg);
	ui->tree=YMGUI_Creat_TreeView_Creat(g_leftPanel,12,48,212,600);YMGUI_TreeView_SetRowHeight(ui->tree,28);YMGUI_TreeView_SetIndent(ui->tree,18);
	YMGUI_TreeView_SetSelectCb(ui->tree,treeSelect);YMGUI_TreeView_SetContextCb(ui->tree,treeContext);
	rebuildHierarchyTree();
	g_bakeLighting=YMGUI_Creat_Checkbox_Creat(g_leftPanel,16,650,210,26);
	YMGUI_Checkbox_SetText(g_bakeLighting,"烘焙包含光照");YMGUI_Checkbox_SetChecked(g_bakeLighting,1);
	label(g_leftPanel,16,680,204,22,"右键管理场景对象",muted);label(g_leftPanel,16,706,204,22,"点击项目查看属性",muted);
	g_image=YMGUI_Creat_Image_Creat(g_centerPanel,0,0,560,648);g_imageSource=(GYimg){(const GYpx*)g_colorBuffer,560,648,0,0};YMGUI_Image_SetSrc(g_image,&g_imageSource);
	ui->viewport=YMGUI_Creat_Obj_Creat(g_centerPanel,0,0,560,648);ui->viewport->draw_cb=NULL;ui->viewport->event_cb=viewportEvent;ui->viewport->state|=GY_STATE_Focusable;
	ui->viewportBrand=label(g_centerPanel,18,16,240,22,"YMGRE  /  SCENE BAKER",cyan);ui->viewportCamera=label(g_centerPanel,300,16,242,22,"VIEWPORT  /  Main Camera",fg);
	label(g_centerPanel,18,50,64,24,"渲染器",fg);
	g_rendererSelector=YMGUI_Creat_Dropdown_Creat(g_centerPanel,86,46,160,30);
	YMGUI_Dropdown_AddOption(g_rendererSelector,"光栅化");YMGUI_Dropdown_AddOption(g_rendererSelector,"光线追踪");
	YMGUI_Dropdown_SetSelectedCb(g_rendererSelector,rendererSelected);
	g_rendererProgress=label(g_centerPanel,260,50,190,24,"",muted);
	label(g_centerPanel,18,482,440,24,"LMB select / move    blank drag pans    RMB orbits",muted);
	SceneEditorInspector_Build(g_rightPanel,inspectorChanged,inspectorRemesh,ui);
    SceneEditorInspector_SetUvHistoryCallback(uvHistoryRestore);
    SceneEditorInspector_SetUvMaterials(&g_importContext.MaterialList);
    SceneEditorInspector_SetUvApplyCallback(uvApplyTexture);
	SceneEditorHierarchyOps ops={renameObject,deleteObject,switchCamera,duplicateObject,bakeObject,toggleBakePreview};SceneEditorHierarchy_Build(ctx,&ops,ui);
	SceneEditorImport_Build(ctx,importMesh,ui);
	SceneEditorIo_Build(ctx,saveSceneToPath,loadSceneFromPath,sceneDialogCancelled,ui);
	g_unsavedPrompt=YMGUI_Creat_MsgBox_Creat(ctx);
	g_sceneOpenError=YMGUI_Creat_MsgBox_Creat(ctx);
	g_bakeExportDialog=YMGUI_Creat_FileDialog_Creat(ctx,760,600,SCENE_EDITOR_PATH_CAPACITY-1,255,256);
	if(g_bakeExportDialog) {
		YMGUI_FileDialog_SetFS(g_bakeExportDialog,SceneEditorFileDialog_PosixFS(),NULL);
		YMGUI_FileDialog_SetResultCb(g_bakeExportDialog,bakeExportResult,NULL);
		YMGUI_FileDialog_SetFilterCb(g_bakeExportDialog,exportDirectoryEntry,NULL);
	}
	label(root,250,722,546,24,"导出文件路径（点击后可全选复制）",muted);
	g_bakeExportPath=YMGUI_Creat_TextInput_Creat(root,250,752,548,26,SCENE_EDITOR_PATH_CAPACITY-1);
	/* Keep the status label left of the inspector so it cannot swallow bottom controls. */
	ui->status=label(root,14,786,780,24,"就绪",muted);
	g_lodPanel=YMGUI_Creat_Obj_Creat(root,250,90,548,206);
	YMGUI_Obj_SetBgColor(g_lodPanel,GY_ARGB(0xFF,0x28,0x35,0x45));
	label(g_lodPanel,16,10,500,24,"LOD 制作：配置层级资源、切换阈值并预览导出",fg);
	label(g_lodPanel,16,42,62,26,"近像素",fg);
	g_lodNearInput=YMGUI_Creat_TextInput_Creat(g_lodPanel,80,42,64,26,12);
	YMGUI_TextInput_SetText(g_lodNearInput,"120");
	label(g_lodPanel,168,42,62,26,"中像素",fg);
	g_lodMiddleInput=YMGUI_Creat_TextInput_Creat(g_lodPanel,232,42,64,26,12);
	YMGUI_TextInput_SetText(g_lodMiddleInput,"40");
	label(g_lodPanel,320,42,48,26,"死区",fg);
	g_lodHysteresisInput=YMGUI_Creat_TextInput_Creat(g_lodPanel,372,42,64,26,12);
	YMGUI_TextInput_SetText(g_lodHysteresisInput,"0.10");
	label(g_lodPanel,16,82,90,26,"中级保留",fg);
	g_lodMiddleRatioInput=YMGUI_Creat_TextInput_Creat(g_lodPanel,104,82,62,26,12);
	YMGUI_TextInput_SetText(g_lodMiddleRatioInput,"0.80");
	label(g_lodPanel,178,82,90,26,"远级保留",fg);
	g_lodFarRatioInput=YMGUI_Creat_TextInput_Creat(g_lodPanel,266,82,62,26,12);
	YMGUI_TextInput_SetText(g_lodFarRatioInput,"0.60");
	g_lodFarKind=YMGUI_Creat_Dropdown_Creat(g_lodPanel,350,82,170,30);
	YMGUI_Dropdown_AddOption(g_lodFarKind,"远级：简化模型");
	YMGUI_Dropdown_AddOption(g_lodFarKind,"远级：八方向图");
	label(g_lodPanel,16,118,500,24,"预览/导出时生成资源；选八方向图时远级比例不使用",muted);
	g_lodPreviewButton=YMGUI_Creat_Button_Creat(g_lodPanel,16,158,150,32);
	YMGUI_Button_SetText(g_lodPreviewButton,"连续预览");
	YMGUI_Button_SetClicked(g_lodPreviewButton,lodPreviewClicked);
	GYOBJ lodExportButton=YMGUI_Creat_Button_Creat(g_lodPanel,184,158,150,32);
	YMGUI_Button_SetText(lodExportButton,"导出LOD");
	YMGUI_Button_SetClicked(lodExportButton,lodExportClicked);
	GYOBJ lodCloseButton=YMGUI_Creat_Button_Creat(g_lodPanel,352,158,150,32);
	YMGUI_Button_SetText(lodCloseButton,"关闭");
	YMGUI_Button_SetClicked(lodCloseButton,lodPanelClicked);
	YMGUI_Obj_SetHidden(g_lodPanel,1);
	clearSelection("就绪");YMGUI_Inject_SetCtx(ctx);YMGUI_SetFocus(ctx,ui->viewport);
}

static int runSelfTest(void)
{
	int failures=0;
	#define SELF_CHECK(condition) do { if(!(condition)) { \
		fprintf(stderr, "scene_editor self-test failed at line %d: %s\n", __LINE__, #condition); \
		failures++; } } while(0)
	/* 固定中心和参考轴，验证屏幕极角：向上、向下应分别得到 ±90 度。 */
	g_rotateCenterX=100.0f; g_rotateCenterY=100.0f; g_rotateBasisUX=1.0f; g_rotateBasisUY=0.0f;
	SELF_CHECK(fabsf(rotatePointerAngle(100.0f, 200.0f) - (float32)(YMGRE_Pai * 0.5f)) < 0.001f);
	SELF_CHECK(fabsf(rotatePointerAngle(100.0f, 0.0f) + (float32)(YMGRE_Pai * 0.5f)) < 0.001f);
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
	SELF_CHECK(g_sceneDirty&&mesh!=NULL&&mesh->mesh!=NULL&&mesh->mesh->polygonNum==144&&mesh->autoUv==2&&g_objectsList.len==1);
	viewportEvent(g_ui->viewport,GY_EVENT_ContextRequested);
	SELF_CHECK(g_activeCamera->targetX==mesh->x&&g_activeCamera->targetY==mesh->y&&g_activeCamera->targetZ==mesh->z);
	GRE_Object4d originalMesh=mesh->mesh;
	SELF_CHECK(inspectorRemesh(mesh,8,16,NULL)&&mesh->mesh!=originalMesh&&
		mesh->mesh->polygonNum==256&&mesh->detailA==8&&mesh->detailB==16&&
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
	dragSelectedAxis(VIEW_DRAG_ROTATE_Y,10,0);SELF_CHECK(fabsf(historyObject->rotY-transformRotation)>0.001f);
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

static int runBakeSelfTest(void)
{
#define BAKE_CHECK(condition) do {if(!(condition)){fprintf(stderr,"baker editor self-test line %d: %s\n",__LINE__,#condition);return 1;}}while(0)
	char root[]="/tmp/ymgre-baker-editor-XXXXXX",scenePath[4096],error[256];
	BAKE_CHECK(mkdtemp(root));
	const char* previousRoot=getenv("YMGRE_BAKE_DIR");char* savedRoot=previousRoot?strdup(previousRoot):NULL;
	setenv("YMGRE_BAKE_DIR",root,1);createNewScene();
	g_globalLight->strength=0.15f;SceneEditorObject_SyncHandle(g_globalLight);
	SceneEditorPlaceResult result={.kind=SCENE_PLACE_CUBE,.y=15,.scale=1,.color=GY_ARGB(0xFF,205,165,105)};
	snprintf(result.name,sizeof(result.name),"Baked Cube");snprintf(result.type,sizeof(result.type),"立方体");
	result.mesh=SceneEditorPlace_CreateMesh(&result);BAKE_CHECK(result.mesh);
	SceneEditorPlace_TransformMesh(result.mesh,0,15,0,0,1,0);objectPlaced(&result,NULL);
	SceneEditorObject* cube=g_selected;
	SceneEditorPlaceResult lamp={.kind=SCENE_PLACE_POINT_LIGHT,.x=-40,.y=65,.z=-45,.strength=1.2f,.color=GY_ARGB(0xFF,255,240,220)};
	snprintf(lamp.name,sizeof(lamp.name),"Bake Light");objectPlaced(&lamp,NULL);
	selectObject(cube,"测试烘焙");
	g_activeCamera->x=65;g_activeCamera->y=55;g_activeCamera->z=-90;g_activeCamera->targetY=15;
	renderScene();
	YMGUI_Inject_Pointer(860,27,1);YMGUI_Inject_Pointer(860,27,0);
	BAKE_CHECK(YMGUI_FileDialog_IsShown(g_bakeExportDialog));
	YMGUI_FileDialog_Close(g_bakeExportDialog);bakeExportResult(g_bakeExportDialog,0,NULL,NULL);
	/* Exercise the actual context-menu input routing, not only the callback. */
	SceneEditorHierarchy_Open(cube,100,100,1);
	YMGUI_Inject_Pointer(140,218,1);YMGUI_Inject_Pointer(140,218,0);
	BAKE_CHECK(cube->mesh->lightmap && strstr(cube->bakePath,".material") && cube->mesh->lightmap->enabled);
	GRE_Lightmap previous=cube->mesh->lightmap;
	char oldBakePath[SCENE_EDITOR_PATH_CAPACITY];snprintf(oldBakePath,sizeof(oldBakePath),"%s",cube->bakePath);
	YMGUI_Inject_Pointer(860,27,1);YMGUI_Inject_Pointer(860,27,0);
	BAKE_CHECK(YMGUI_FileDialog_IsShown(g_bakeExportDialog) && YMGUI_FileDialog_GetMode(g_bakeExportDialog)==GY_FILE_DIALOG_SELECT_DIRECTORY);
	YMGUI_FileDialog_Close(g_bakeExportDialog);bakeExportResult(g_bakeExportDialog,0,NULL,NULL);
	BAKE_CHECK(strcmp(cube->bakePath,oldBakePath)==0 && cube->mesh->lightmap==previous);
	YMGUI_Inject_Pointer(860,27,1);YMGUI_Inject_Pointer(860,27,0);
	BAKE_CHECK(YMGUI_FileDialog_Navigate(g_bakeExportDialog,root));
	BAKE_CHECK(YMGUI_FileDialog_NewDirectory(g_bakeExportDialog,"chosen-export"));
	char selectedDirectory[4096];snprintf(selectedDirectory,sizeof(selectedDirectory),"%s/chosen-export",root);
	BAKE_CHECK(YMGUI_FileDialog_Navigate(g_bakeExportDialog,selectedDirectory));
	BAKE_CHECK(YMGUI_FileDialog_Confirm(g_bakeExportDialog));
	BAKE_CHECK(!YMGUI_FileDialog_IsShown(g_bakeExportDialog) && strstr(cube->bakePath,"/chosen-export/bake-"));
	BAKE_CHECK(cube->mesh->lightmap==previous && strstr(YMGUI_TextInput_GetText(g_bakeExportPath),"/baked_model.mesh")!=NULL);
	GRE_Lightmap exported=SceneBake_Load(cube->bakePath,cube->mesh,&g_lightsList,error,sizeof(error));
	BAKE_CHECK(exported);YMGRE_Free_Lightmap(exported);
	/* Failed output must not replace the previous usable map. */
	char blocked[4096];snprintf(blocked,sizeof(blocked),"%s/blocked",root);FILE* f=fopen(blocked,"w");BAKE_CHECK(f);fclose(f);
	setenv("YMGRE_BAKE_DIR",blocked,1);bakeObject(cube,NULL);BAKE_CHECK(cube->mesh->lightmap==previous);
	setenv("YMGRE_BAKE_DIR",root,1);
	SceneEditorHierarchy_Open(cube,100,100,1);
	YMGUI_Inject_Pointer(140,250,1);YMGUI_Inject_Pointer(140,250,0);
	BAKE_CHECK(!previous->enabled);toggleBakePreview(cube,NULL);BAKE_CHECK(previous->enabled);
	snprintf(scenePath,sizeof(scenePath),"%s/cube.scene",root);BAKE_CHECK(writeCurrentScene(scenePath,error,sizeof(error)));
	/* Camera movement must not change or discard the baked illumination. */
	g_activeCamera->x+=10;renderScene();BAKE_CHECK(cube->mesh->lightmap==previous);
	loadSceneFromPath(scenePath,NULL);BAKE_CHECK(strcmp(g_currentScenePath,scenePath)==0);
	cube=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)cube=&g_scene[i];
	BAKE_CHECK(cube && cube->mesh->lightmap && cube->mesh->lightmap->enabled);
	selectObject(cube,"烘焙预览：右键可切换实时光照");renderScene();
	/* Save a reviewable viewport from the actual editor render path. */
	char preview[4096];snprintf(preview,sizeof(preview),"%s/preview.bmp",root);
	GRErgb24* pixels=malloc((size_t)g_target.width*g_target.height*sizeof(GRErgb24));BAKE_CHECK(pixels);
	unsigned drawn=0;for(uint32 i=0;i<(uint32)g_target.width*g_target.height;i++) {
		pixels[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);if(pixels[i].R>80 && pixels[i].R>pixels[i].B)drawn++;
	}
	BAKE_CHECK(drawn>200);YMGRE_Image_LoadTo_Bmp_File(preview,pixels,g_target.width,g_target.height);free(pixels);
	g_globalLight->strength+=0.1f;SceneEditorObject_SyncHandle(g_globalLight);renderScene();
	BAKE_CHECK(!cube->mesh->lightmap && !cube->bakePath[0]);
	g_globalLight->strength-=0.1f;SceneEditorObject_SyncHandle(g_globalLight);
	loadSceneFromPath(scenePath,NULL);
	cube=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)cube=&g_scene[i];
	BAKE_CHECK(cube && cube->mesh->lightmap);
	SceneEditorObject_Translate(cube,1,0,0);renderScene();BAKE_CHECK(!cube->mesh->lightmap);
	loadSceneFromPath(scenePath,NULL);
	cube=NULL;for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)cube=&g_scene[i];
	BAKE_CHECK(cube);
	SceneEditorObject_SetRotationAxis(cube,2,7);SceneEditorObject_SetRotationAxis(cube,0,13);
	SceneEditorObject_SetRotationAxis(cube,1,21);selectObject(cube,"测试顶部烘焙按钮");
	YMGUI_Inject_Pointer(760,27,1);YMGUI_Inject_Pointer(760,27,0);BAKE_CHECK(cube->mesh->lightmap);
	char rotatedPath[4096];snprintf(rotatedPath,sizeof(rotatedPath),"%s/rotated.scene",root);
	BAKE_CHECK(writeCurrentScene(rotatedPath,error,sizeof(error)));
	loadSceneFromPath(rotatedPath,NULL);BAKE_CHECK(strcmp(g_currentScenePath,rotatedPath)==0);
	loadSceneFromPath(scenePath,NULL);
	for(int i=0;i<32;i++)if(g_scene[i].active && g_scene[i].kind==SCENE_OBJECT_MESH)selectObject(&g_scene[i],"烘焙预览：右键可切换实时光照");
	if(savedRoot){setenv("YMGRE_BAKE_DIR",savedRoot,1);free(savedRoot);}else unsetenv("YMGRE_BAKE_DIR");
	renderScene();SceneEditorHierarchy_Open(g_selected,30,360,1);
	YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
	unsigned screenSize=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
	GRErgb24* screen=malloc(screenSize*sizeof(GRErgb24));BAKE_CHECK(screen);
	for(unsigned i=0;i<screenSize;i++) {GYcolor color=GY_PxToColor(g_ui->host.display.buf1[i]);
		screen[i]=(GRErgb24){(color>>16)&255,(color>>8)&255,color&255};}
	snprintf(preview,sizeof(preview),"%s/editor.bmp",root);
	YMGRE_Image_LoadTo_Bmp_File(preview,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);free(screen);
	printf("scene baker editor: PASS (toolbar, directory export, cancel, button, failed write, preview toggle, scene reload, camera, stale bake)\nartifacts: %s\n",root);
#undef BAKE_CHECK
	return 0;
}

#include "test_scene_model.h"
#include "test_scene_renderer.h"
#include "test_scene_glass.h"
#include "test_scene_smooth.h"
#include "test_scene_uv_editor.h"
#include "test_scene_texture_tools.h"

static int runLodUiSelfTest(void)
{
    int failures=0;
#define LOD_CHECK(x) do{if(!(x)){fprintf(stderr,"LOD UI test failed at line %d: %s\n",__LINE__,#x);failures++;goto done;}}while(0)
    char root[]="/tmp/ymgre-lod-ui-XXXXXX";LOD_CHECK(mkdtemp(root)!=NULL);
    createNewScene();
    SceneEditorPlaceResult sphere={.kind=SCENE_PLACE_SPHERE,.x=0,.y=0,.z=0,.scale=1,
        .color=GY_ARGB(0xFF,80,170,90),.detailA=12,.detailB=24};
    snprintf(sphere.name,sizeof(sphere.name),"LOD test sphere");
    snprintf(sphere.type,sizeof(sphere.type),"球体");
    sphere.mesh=SceneEditorPlace_CreateMesh(&sphere);
    LOD_CHECK(sphere.mesh!=NULL);
    SceneEditorPlace_TransformMesh(sphere.mesh,0,0,0,0,1,0);
    objectPlaced(&sphere,NULL);LOD_CHECK(g_selected&&g_selected->mesh);
    GRE_Object4d original=g_selected->mesh;
    renderScene();
    YMGUI_Inject_Pointer(974,27,1);YMGUI_Inject_Pointer(974,27,0);
    LOD_CHECK(!(g_lodPanel->state&GY_STATE_Hidden));
    YMGUI_Obj_Invalidate(g_ui->host.context->root);YMGUI_Refresh(g_ui->host.context);
    unsigned screenCount=(unsigned)g_ui->host.display.hor_res*g_ui->host.display.ver_res;
    GRErgb24 *screen=malloc(screenCount*sizeof(*screen));LOD_CHECK(screen!=NULL);
    for(unsigned i=0;i<screenCount;i++){
        GYcolor color=GY_PxToColor(g_ui->host.display.buf1[i]);
        screen[i]=(GRErgb24){(color>>16)&255,(color>>8)&255,color&255};
    }
    char panelPath[4096];snprintf(panelPath,sizeof(panelPath),"%s/lod-panel.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(panelPath,screen,g_ui->host.display.hor_res,g_ui->host.display.ver_res);
    free(screen);
    YMGUI_TextInput_SetText(g_lodMiddleRatioInput,"0.65");
    YMGUI_TextInput_SetText(g_lodFarRatioInput,"0.35");
    lodPreviewClicked(NULL);LOD_CHECK(g_lodPreview.running);
    uint8 seen[3]={0};
    for(int i=0;i<=64;i++){
        renderScene();seen[g_lodPreview.selection.level]=1;
        LOD_CHECK(g_selected->mesh==original);
    }
    LOD_CHECK(seen[0]&&seen[1]&&seen[2]);
    lodStopPreview();LOD_CHECK(g_selected->mesh==original);
    YMGUI_Dropdown_SetSelected(g_lodFarKind,1);
    lodPreviewClicked(NULL);LOD_CHECK(g_lodPreview.running&&g_lodPreview.image);
    unsigned opaque=0;
    for(int i=0;i<GRASS_TILE*GRASS_TILE;i++)opaque+=g_lodPreview.image->views[0].mask[i]!=0;
    LOD_CHECK(opaque>0);
    for(int i=0;i<=32;i++)renderScene();
    LOD_CHECK(g_lodPreview.selection.level==2&&g_lodPreview.active==NULL&&g_selected->mesh==original);
    GRErgb24 *pixels=malloc((size_t)g_target.width*g_target.height*sizeof(*pixels));LOD_CHECK(pixels!=NULL);
    for(size_t i=0;i<(size_t)g_target.width*g_target.height;i++)pixels[i]=GRE_FramePixel_To_RGB24(g_target.data[i]);
    char framePath[4096];snprintf(framePath,sizeof(framePath),"%s/far-image.bmp",root);
    YMGRE_Image_LoadTo_Bmp_File(framePath,pixels,g_target.width,g_target.height);free(pixels);
    lodStopPreview();
    g_exportModel=2;g_bakeExportObject=g_selected;
    bakeExportResult(NULL,1,root,NULL);
    const char *lodPath=YMGUI_TextInput_GetText(g_bakeExportPath);
    LOD_CHECK(lodPath&&access(lodPath,R_OK)==0);
    FILE *description=fopen(lodPath,"rb");LOD_CHECK(description!=NULL);
    char contents[512]={0};size_t length=fread(contents,1,sizeof(contents)-1,description);
    fclose(description);
    LOD_CHECK(length>0&&strstr(contents,"level image 0 views8.bin")!=NULL);
    YMGRE_LOD_Object parsed;LOD_CHECK(YMGRE_LOD_Parse(&parsed,contents)&&
        parsed.count==3&&parsed.levels[2].kind==YMGRE_LOD_IMAGE);
    char assetPath[4096];snprintf(assetPath,sizeof(assetPath),"%s",lodPath);
    char *slash=strrchr(assetPath,'/');LOD_CHECK(slash!=NULL);
    strcpy(slash+1,"views8.bin");
    LOD_CHECK(strstr(lodPath,"LOD_test_sphere_lod/object.lod")!=NULL);
    char meshAsset[4096];snprintf(meshAsset,sizeof(meshAsset),"%s",lodPath);
    strcpy(strrchr(meshAsset,'/')+1,"LOD_test_sphere_near.mesh");
    LOD_CHECK(access(meshAsset,R_OK)==0);
    strcpy(strrchr(meshAsset,'/')+1,"LOD_test_sphere_middle.mesh");
    LOD_CHECK(access(meshAsset,R_OK)==0);
    char packageDir[4096];snprintf(packageDir,sizeof(packageDir),"%s",lodPath);
    *strrchr(packageDir,'/')=0;
    LOD_CHECK(access(packageDir,R_OK)==0);
    DIR *package=opendir(packageDir);LOD_CHECK(package!=NULL);
    unsigned sharedTextures=0;struct dirent *entry;
    while((entry=readdir(package))!=NULL)
        sharedTextures+=strncmp(entry->d_name,"texture_",8)==0;
    closedir(package);LOD_CHECK(sharedTextures==1);
    grass_impostor *reloaded=malloc(sizeof(*reloaded));LOD_CHECK(reloaded!=NULL);
    int loaded=SceneLodImage_Load(reloaded,assetPath);
    LOD_CHECK(loaded&&reloaded->span>0&&reloaded->views[0].right>=reloaded->views[0].left);
    strcpy(strrchr(assetPath,'/')+1,"view_7.bmp");LOD_CHECK(access(assetPath,R_OK)==0);
    strcpy(strrchr(assetPath,'/')+1,"mask_7.bmp");LOD_CHECK(access(assetPath,R_OK)==0);
    free(reloaded);
    const char *fixtures[2]={YMGRE_GRASS_LOD_FIXTURE,YMGRE_TREE_LOD_FIXTURE};
    for(int fixture=0;fixture<2;fixture++){
        gre_scence context={0};
        GRE_Object4d vegetation=YMGRE_LoadOgreMeshAndMaterial(&context,fixtures[fixture]);
        LOD_CHECK(vegetation!=NULL);
        grass_impostor *views=malloc(sizeof(*views));LOD_CHECK(views!=NULL);
        int baked=SceneLodImage_Bake(views,vegetation,&context.MaterialList,&g_lightsList);
        unsigned coverage=0;
        for(int v=0;v<GRASS_VIEWS;v++)for(int p=0;p<GRASS_TILE*GRASS_TILE;p++)
            coverage+=views->views[v].mask[p]!=0;
        free(views);YMGRE_Free_Object(vegetation);
        YMGRE_List_Clear(&context.MaterialList,YMGRE_Free_Material);
        LOD_CHECK(baked&&coverage>0);
    }
    createNewScene();
    char importError[256];
    LOD_CHECK(SceneEditorImport_Validate(YMGRE_BIRCH_LOD_FIXTURE,importError,sizeof(importError)));
    LOD_CHECK(importMesh(YMGRE_BIRCH_LOD_FIXTURE,0,NULL));
    int partCount=0;
    for(GRE_Object4d part=g_selected->mesh;part;part=part->nextObject){
        partCount++;
        LOD_CHECK(part->boundType==(partCount==1?GRE_Bounding_Box_AABB:GRE_Bounding_Sphere_R));
        for(int i=0;i<part->pointNum;i++){
            gre_fvector4d p=part->pointList[i].pos;
            LOD_CHECK(p.x>=part->BoundingBoxMin.x-0.001f&&p.x<=part->BoundingBoxMax.x+0.001f&&
                p.y>=part->BoundingBoxMin.y-0.001f&&p.y<=part->BoundingBoxMax.y+0.001f&&
                p.z>=part->BoundingBoxMin.z-0.001f&&p.z<=part->BoundingBoxMax.z+0.001f);
            LOD_CHECK(p.x*p.x+p.y*p.y+p.z*p.z<=
                part->BoundingSphereR*part->BoundingSphereR+0.01f);
        }
    }
    LOD_CHECK(partCount==3);
    SceneEditorObject *birch=g_selected;
    g_activeCamera->x=14;g_activeCamera->y=10;g_activeCamera->z=-20;
    g_activeCamera->targetX=0;g_activeCamera->targetY=5;g_activeCamera->targetZ=0;
    uint8 referenceVisible=g_referenceVisible;g_referenceVisible=0;g_selected=NULL;
    renderScene();
    g_selected=birch;g_referenceVisible=referenceVisible;
    unsigned greenPixels=0;
    for(size_t i=0;i<(size_t)g_target.width*g_target.height;i++){
        GRErgb24 color=GRE_FramePixel_To_RGB24(g_target.data[i]);
        greenPixels+=color.G>color.R+8&&color.G>color.B+5;
    }
    LOD_CHECK(greenPixels>100);
    printf("scene LOD UI: mesh and eight-view preview, image package roundtrip: PASS (%s, %s, %s)\n",lodPath,panelPath,framePath);
done:
    lodStopPreview();
#undef LOD_CHECK
    return failures;
}

int main(void)
{
	EditorUi ui={0};g_ui=&ui;setenv("YMGRE_WINDOW_SCALE","1",1);
	if(!YMGRE_DemoHost_Init(&ui.host,1024,816,816))return 1;
	SceneEditorFont_Init();initScene();buildUi(&ui);historyReset();
	SDL_LCD_SetCloseRequestCb(quitRequested,NULL);
	const char* limit=getenv("YMGRE_MAX_FRAMES");ui.frameLimit=limit?atoi(limit):0;
	int selfTestFailures=0;
    if(getenv("YMGRE_TEXTURE_TOOLS")){selfTestFailures+=runTextureToolsSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_UV_SELFTEST")){selfTestFailures+=runUvEditorSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_SMOOTH_SELFTEST")){selfTestFailures+=runSmoothSphereSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_OPTICS_SELFTEST")){selfTestFailures+=runOpticsSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_RENDERER_SELFTEST")!=NULL){selfTestFailures+=runRendererSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_SPECULAR_BAKE_SELFTEST")!=NULL){selfTestFailures+=runSpecularBakeSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_COLOR_BAKE_SELFTEST")!=NULL){selfTestFailures+=runColorBakeSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_TANK_ROUNDTRIP")!=NULL){selfTestFailures+=runTankRoundtrip();ui.frameLimit=1;}
	if(getenv("YMGRE_SCENE_EDITOR_SELFTEST")!=NULL){selfTestFailures=runSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_SCENE_BAKER_SELFTEST")!=NULL){selfTestFailures+=runBakeSelfTest();ui.frameLimit=1;}
	if(getenv("YMGRE_SCENE_LOD_SELFTEST")!=NULL){selfTestFailures+=runLodUiSelfTest();ui.frameLimit=1;}
	YMGUI_Obj_SetBgColor(ui.host.context->root,GY_ARGB(0xFF,0x15,0x1A,0x24));
	while(!g_shouldExit&&SDL_LCD_PumpEvents()){
		renderEditorFrame();
		if(ui.frameLimit>0&&++ui.frames>=ui.frameLimit) break;
		SDL_LCD_Delay(16);
	}
	lodStopPreview();
	for(int i=0;i<32;++i)if(g_scene[i].active&&g_scene[i].kind==SCENE_OBJECT_CAMERA&&g_scene[i].camera!=g_mainCamera->camera)YMGRE_Free_Camera(g_scene[i].camera);
	SceneRay_Destroy(g_rayRenderer);
	YMGRE_Free_RenderWorkspace(g_workspace);YMGRE_Free_Camera(g_mainCamera->camera);
	YMGRE_List_Clear(&g_lightsList,YMGRE_Free_Light);YMGRE_List_Clear(&g_objectsList,YMGRE_Free_Object);
	YMGRE_List_Clear(&g_importContext.MaterialList,YMGRE_Free_Material);
	historyRemoveFrom(0);
	SceneEditorInspector_Shutdown();YMGRE_DemoHost_Destroy(&ui.host);SceneEditorFont_Shutdown();g_ui=NULL;return selfTestFailures?1:0;
}
