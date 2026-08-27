#include "scene_editor_place.h"
#include "scene_editor_color.h"

#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Label.h"
#include "YMGUI_TextInput.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_Free.h"
#include "YMGRE_MathBase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	GYOBJ menu, modal, title, error;
	GYOBJ name, x, y, z, positionLabels[3];
	GYOBJ targetLabels[3], targetInputs[3];
	GYOBJ rotationLabel, rotationY, scaleLabel, scale;
	GYOBJ detailLabels[2], detailInputs[2];
	GYOBJ strengthLabel, strength, colorLabel, colorSwatch, wireframe, shadows;
	GYcolor color;
	SceneEditorPlaceKind kind;
	SceneEditorPlaceCreatedCb createdCb;
	void* userData;
} PlaceState;

static PlaceState g_place;

static GYOBJ placeLabel(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
	const char* text, GYcolor color)
{
	GYOBJ label = YMGUI_Creat_Label_Creat(parent, x, y, w, h);
	if (label != NULL) { YMGUI_Label_SetText(label, text); YMGUI_Label_SetTextColor(label, color); }
	return label;
}

static GYOBJ placeButton(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w,
	const char* text, GYbtn_clicked_cb clicked)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, x, y, w, 28);
	if (button != NULL) {
		YMGUI_Button_SetText(button, text);
		YMGUI_Button_SetColors(button, GY_ARGB(0xFF,0x2C,0x35,0x48), GY_ARGB(0xFF,0x3C,0x86,0xB8));
		YMGUI_Button_SetClicked(button, clicked);
	}
	return button;
}

static const char* kindName(SceneEditorPlaceKind kind)
{
	switch (kind) {
	case SCENE_PLACE_PLANE: return "平面";
	case SCENE_PLACE_CUBE: return "立方体";
	case SCENE_PLACE_BOX: return "长方体";
	case SCENE_PLACE_SPHERE: return "球体";
	case SCENE_PLACE_CYLINDER: return "圆柱";
	case SCENE_PLACE_CONE: return "圆锥";
	case SCENE_PLACE_TORUS: return "圆环";
	case SCENE_PLACE_CAPSULE: return "胶囊";
	case SCENE_PLACE_POINT_LIGHT: return "点光源";
	case SCENE_PLACE_SPOT_LIGHT: return "聚光灯";
	default: return "相机";
	}
}

static uint8 isMeshKind(SceneEditorPlaceKind kind) { return kind <= SCENE_PLACE_CAPSULE; }

static GYcolor defaultColor(SceneEditorPlaceKind kind)
{
	switch (kind) {
	case SCENE_PLACE_BOX: return GY_ARGB(0xFF,205,125,76);
	case SCENE_PLACE_SPHERE: return GY_ARGB(0xFF,92,176,126);
	case SCENE_PLACE_CYLINDER: return GY_ARGB(0xFF,205,105,155);
	case SCENE_PLACE_CONE: return GY_ARGB(0xFF,224,180,72);
	case SCENE_PLACE_TORUS: return GY_ARGB(0xFF,135,105,205);
	case SCENE_PLACE_CAPSULE: return GY_ARGB(0xFF,70,180,190);
	case SCENE_PLACE_POINT_LIGHT:
	case SCENE_PLACE_SPOT_LIGHT: return GY_ARGB(0xFF,255,238,190);
	default: return GY_ARGB(0xFF,92,155,220);
	}
}

static void setSwatchColor(GYcolor color)
{
	g_place.color = color;
	YMGUI_Button_SetColors(g_place.colorSwatch, color, color);
}

static void placeColorSelected(GYcolor color, void* userData)
{
	(void)userData; setSwatchColor(color);
}

static void colorClicked(GYOBJ button)
{
	(void)button; SceneEditorColor_Open(g_place.color, placeColorSelected, NULL);
}

static void setPairHidden(GYOBJ label, GYOBJ input, uint8 hidden)
{
	YMGUI_Obj_SetHidden(label, hidden); YMGUI_Obj_SetHidden(input, hidden);
}

static void showProperties(SceneEditorPlaceKind kind)
{
	g_place.kind = kind;
	char title[64];
	snprintf(title, sizeof(title), "放置  /  %s", kindName(kind));
	YMGUI_Label_SetText(g_place.title, title);
	YMGUI_TextInput_SetText(g_place.name, kindName(kind));
	YMGUI_TextInput_SetText(g_place.x, "0");
	YMGUI_TextInput_SetText(g_place.y, isMeshKind(kind) ? "0" : "60");
	YMGUI_TextInput_SetText(g_place.z, kind == SCENE_PLACE_CAMERA ? "-160" : "0");
	YMGUI_Label_SetText(g_place.positionLabels[1], isMeshKind(kind) ? "底面 Y" : "位置 Y");
	for (int i = 0; i < 3; ++i) YMGUI_TextInput_SetText(g_place.targetInputs[i], "0");
	YMGUI_TextInput_SetText(g_place.rotationY, "0");
	YMGUI_TextInput_SetText(g_place.scale, "1");
	YMGUI_TextInput_SetText(g_place.strength, "1");
	YMGUI_TextInput_SetText(g_place.detailInputs[0], "20");
	YMGUI_TextInput_SetText(g_place.detailInputs[1], "20");
	setSwatchColor(defaultColor(kind));
	YMGUI_Checkbox_SetChecked(g_place.wireframe, 1);
	YMGUI_Checkbox_SetChecked(g_place.shadows, 0);
	uint8 mesh = isMeshKind(kind);
	uint8 target = kind == SCENE_PLACE_SPOT_LIGHT || kind == SCENE_PLACE_CAMERA;
	for (int i = 0; i < 3; ++i) setPairHidden(g_place.targetLabels[i], g_place.targetInputs[i], !target);
	setPairHidden(g_place.rotationLabel, g_place.rotationY, !mesh);
	setPairHidden(g_place.scaleLabel, g_place.scale, !mesh);
	uint8 detailA = kind == SCENE_PLACE_PLANE || (kind >= SCENE_PLACE_SPHERE && kind <= SCENE_PLACE_CAPSULE);
	uint8 detailB = kind == SCENE_PLACE_PLANE || kind == SCENE_PLACE_SPHERE ||
		kind == SCENE_PLACE_TORUS || kind == SCENE_PLACE_CAPSULE;
	setPairHidden(g_place.detailLabels[0], g_place.detailInputs[0], !detailA);
	setPairHidden(g_place.detailLabels[1], g_place.detailInputs[1], !detailB);
	if (kind == SCENE_PLACE_PLANE) {
		YMGUI_Label_SetText(g_place.detailLabels[0], "行网格"); YMGUI_Label_SetText(g_place.detailLabels[1], "列网格");
	} else if (kind == SCENE_PLACE_SPHERE) {
		YMGUI_Label_SetText(g_place.detailLabels[0], "纬向网格"); YMGUI_Label_SetText(g_place.detailLabels[1], "经向网格");
	} else if (kind == SCENE_PLACE_TORUS) {
		YMGUI_Label_SetText(g_place.detailLabels[0], "主环网格"); YMGUI_Label_SetText(g_place.detailLabels[1], "截面网格");
	} else if (kind == SCENE_PLACE_CAPSULE) {
		YMGUI_Label_SetText(g_place.detailLabels[0], "半球网格"); YMGUI_Label_SetText(g_place.detailLabels[1], "经向网格");
	} else YMGUI_Label_SetText(g_place.detailLabels[0], "环向网格");
	uint8 light = kind == SCENE_PLACE_POINT_LIGHT || kind == SCENE_PLACE_SPOT_LIGHT;
	setPairHidden(g_place.strengthLabel, g_place.strength, !light);
	YMGUI_Obj_SetHidden(g_place.shadows, !light);
	YMGUI_Obj_SetHidden(g_place.colorLabel, kind == SCENE_PLACE_CAMERA);
	YMGUI_Obj_SetHidden(g_place.colorSwatch, kind == SCENE_PLACE_CAMERA);
	YMGUI_Obj_SetHidden(g_place.wireframe, !mesh);
	YMGUI_Label_SetText(g_place.error, "");
	YMGUI_Obj_SetHidden(g_place.menu, 1); YMGUI_Obj_SetHidden(g_place.modal, 0);
}

#define PLACE_CB(name, kind) static void name(GYOBJ button) { (void)button; showProperties(kind); }
PLACE_CB(cubeClicked, SCENE_PLACE_CUBE)
PLACE_CB(planeClicked, SCENE_PLACE_PLANE)
PLACE_CB(boxClicked, SCENE_PLACE_BOX)
PLACE_CB(sphereClicked, SCENE_PLACE_SPHERE)
PLACE_CB(cylinderClicked, SCENE_PLACE_CYLINDER)
PLACE_CB(coneClicked, SCENE_PLACE_CONE)
PLACE_CB(torusClicked, SCENE_PLACE_TORUS)
PLACE_CB(capsuleClicked, SCENE_PLACE_CAPSULE)
PLACE_CB(pointClicked, SCENE_PLACE_POINT_LIGHT)
PLACE_CB(spotClicked, SCENE_PLACE_SPOT_LIGHT)
PLACE_CB(cameraClicked, SCENE_PLACE_CAMERA)

static void placeClicked(GYOBJ button)
{
	(void)button; YMGUI_Obj_SetHidden(g_place.menu, !(g_place.menu->state & GY_STATE_Hidden));
}

static void cancelClicked(GYOBJ button) { (void)button; YMGUI_Obj_SetHidden(g_place.modal, 1); }

static int parseFloat(GYOBJ input, float32* value)
{
	char* end = NULL; const char* text = YMGUI_TextInput_GetText(input);
	*value = strtof(text, &end);
	return text[0] != '\0' && end != text && *end == '\0' && *value == *value;
}

static int parseDetail(GYOBJ input, uint16 minimum, uint16* value)
{
	char* end = NULL; const char* text = YMGUI_TextInput_GetText(input);
	long parsed = strtol(text, &end, 10);
	if (text[0] == '\0' || end == text || *end != '\0' || parsed < minimum || parsed > 128) return 0;
	*value = (uint16)parsed; return 1;
}

GRE_Object4d SceneEditorPlace_CreateMesh(const SceneEditorPlaceResult* result)
{
	GRErgb24 color = { (uint8)(result->color >> 16), (uint8)(result->color >> 8), (uint8)result->color };
	switch (result->kind) {
	case SCENE_PLACE_PLANE: return YMGRE_MeshGener_RectPlane(48, 48, result->detailA, result->detailB,
		color, (char*)result->name, "");
	case SCENE_PLACE_CUBE: return YMGRE_MeshGener_Cube(30, color, (char*)result->name, "");
	case SCENE_PLACE_BOX: return YMGRE_MeshGener_Box(42, 24, 30, color, (char*)result->name, "");
	case SCENE_PLACE_SPHERE: return YMGRE_MeshGener_Sphere(18, result->detailA, result->detailB, color, (char*)result->name, "");
	case SCENE_PLACE_CYLINDER: return YMGRE_MeshGener_Cylinder(15, 32, result->detailA, color, (char*)result->name, "");
	case SCENE_PLACE_CONE: return YMGRE_MeshGener_Cone(17, 34, result->detailA, color, (char*)result->name, "");
	case SCENE_PLACE_TORUS: return YMGRE_MeshGener_Torus(20, 6, result->detailA, result->detailB, color, (char*)result->name, "");
	default: return YMGRE_MeshGener_Capsule(12, 26, result->detailA, result->detailB, color, (char*)result->name, "");
	}
}

void SceneEditorPlace_TransformMesh(GRE_Object4d mesh, float32 x, float32 y, float32 z,
	float32 rotationY, float32 scale, uint8 wireframe)
{
	if (mesh == NULL) return;
	GRE_FMat4x4 rotation = YMGRE_FMat4x4_RotateY_Cal(rotationY);
	for (GRE_Object4d part = mesh; part != NULL; part = part->nextObject) {
		for (int i = 0; i < part->pointNum; ++i) {
			gre_fvector4d rotated;
			YMGRE_Fvector4d_MatMultTo(rotation, &part->pointList[i].pos, &rotated);
			part->pointList[i].pos = rotated;
		}
		part->WorldCoordinate = (gre_fvector4d){ x, y, z, 1 };
		part->scale = scale;
		part->wireFrame = wireframe;
		YMGRE_Object_LocalToWorld(part);
	}
	YMGRE_Free_FMat4x4(rotation);
}

static void confirmClicked(GYOBJ button)
{
	(void)button;
	SceneEditorPlaceResult result = { .kind = g_place.kind, .scale = 1, .strength = 1,
		.detailA = 20, .detailB = 20, .color = g_place.color };
	const char* name = YMGUI_TextInput_GetText(g_place.name);
	uint8 mesh = isMeshKind(result.kind);
	uint8 target = result.kind == SCENE_PLACE_SPOT_LIGHT || result.kind == SCENE_PLACE_CAMERA;
	uint8 light = result.kind == SCENE_PLACE_POINT_LIGHT || result.kind == SCENE_PLACE_SPOT_LIGHT;
	int valid = name[0] != '\0' && parseFloat(g_place.x, &result.x) &&
		parseFloat(g_place.y, &result.y) && parseFloat(g_place.z, &result.z);
	if (target) valid = valid && parseFloat(g_place.targetInputs[0], &result.targetX) &&
		parseFloat(g_place.targetInputs[1], &result.targetY) && parseFloat(g_place.targetInputs[2], &result.targetZ);
	if (mesh) valid = valid && parseFloat(g_place.rotationY, &result.rotationY) &&
		parseFloat(g_place.scale, &result.scale) && result.scale > 0.0f;
	if (light) valid = valid && parseFloat(g_place.strength, &result.strength) && result.strength >= 0.0f;
	if (result.kind == SCENE_PLACE_PLANE) valid = valid && parseDetail(g_place.detailInputs[0], 1, &result.detailA) && parseDetail(g_place.detailInputs[1], 1, &result.detailB);
	else if (result.kind == SCENE_PLACE_SPHERE) valid = valid && parseDetail(g_place.detailInputs[0], 2, &result.detailA) && parseDetail(g_place.detailInputs[1], 3, &result.detailB);
	else if (result.kind == SCENE_PLACE_CYLINDER || result.kind == SCENE_PLACE_CONE) valid = valid && parseDetail(g_place.detailInputs[0], 3, &result.detailA);
	else if (result.kind == SCENE_PLACE_TORUS) valid = valid && parseDetail(g_place.detailInputs[0], 3, &result.detailA) && parseDetail(g_place.detailInputs[1], 3, &result.detailB);
	else if (result.kind == SCENE_PLACE_CAPSULE) valid = valid && parseDetail(g_place.detailInputs[0], 1, &result.detailA) && parseDetail(g_place.detailInputs[1], 3, &result.detailB);
	if (!valid) { YMGUI_Label_SetText(g_place.error, "输入错误，网格范围 3 - 128"); return; }
	snprintf(result.name, sizeof(result.name), "%s", name);
	snprintf(result.type, sizeof(result.type), "%s", kindName(result.kind));
	result.wireframe = mesh ? YMGUI_Checkbox_GetChecked(g_place.wireframe) : 0;
	result.shadowsEnabled = light ? YMGUI_Checkbox_GetChecked(g_place.shadows) : 0;
	if (mesh) {
		result.mesh = SceneEditorPlace_CreateMesh(&result);
		if (result.mesh == NULL) { YMGUI_Label_SetText(g_place.error, "创建失败"); return; }
		/* Mesh placement Y is a ground clearance; store the actual center position. */
		float32 minimumY = result.mesh->pointList[0].pos.y;
		for (GRE_Object4d part = result.mesh; part != NULL; part = part->nextObject)
			for (int i = 0; i < part->pointNum; ++i)
				minimumY = GREMin(minimumY, part->pointList[i].pos.y);
		result.y -= minimumY * result.scale;
		SceneEditorPlace_TransformMesh(result.mesh, result.x, result.y, result.z,
			result.rotationY, result.scale, result.wireframe);
	}
	if (g_place.createdCb != NULL) g_place.createdCb(&result, g_place.userData);
	else if (result.mesh != NULL) YMGRE_Free_Object(result.mesh);
	YMGUI_Obj_SetHidden(g_place.modal, 1);
}

static GYOBJ addInput(GYOBJ dialog, GYcoord y, const char* caption, GYOBJ* labelOut)
{
	GYOBJ label = placeLabel(dialog, 22, y, 112, 26, caption, GY_ARGB(0xFF,0xB4,0xBF,0xCE));
	if (labelOut != NULL) *labelOut = label;
	return YMGUI_Creat_TextInput_Creat(dialog, 142, y, 244, 28, 63);
}

void SceneEditorPlace_Build(GYOBJ toolbar, GYcoord buttonX, GYCTX context,
	SceneEditorPlaceCreatedCb createdCb, void* userData)
{
	memset(&g_place, 0, sizeof(g_place));
	g_place.createdCb = createdCb; g_place.userData = userData;
	placeButton(toolbar, buttonX, 12, 78, "放置", placeClicked);
	GYOBJ top = YMGUI_Ctx_GetTopLayer(context);
	g_place.menu = YMGUI_Creat_Obj_Creat(top, buttonX - 6, 48, 174, 368);
	YMGUI_Obj_SetBgColor(g_place.menu, GY_ARGB(0xFF,0x25,0x2D,0x3C));
	GYbtn_clicked_cb callbacks[] = { planeClicked, cubeClicked, boxClicked, sphereClicked, cylinderClicked, coneClicked,
		torusClicked, capsuleClicked, pointClicked, spotClicked, cameraClicked };
	for (int i = 0; i < 11; ++i) placeButton(g_place.menu, 8, 8 + i * 32, 158, kindName((SceneEditorPlaceKind)i), callbacks[i]);
	YMGUI_Obj_SetHidden(g_place.menu, 1);

	g_place.modal = YMGUI_Creat_Obj_Creat(top, 0, 0, 1024, 680);
	YMGUI_Obj_SetBgColor(g_place.modal, GY_ARGB(0xFF,0x10,0x14,0x1C));
	GYOBJ dialog = YMGUI_Creat_Obj_Creat(g_place.modal, 292, 36, 440, 608);
	YMGUI_Obj_SetBgColor(dialog, GY_ARGB(0xFF,0x27,0x31,0x42));
	g_place.title = placeLabel(dialog, 20, 14, 400, 28, "放置", GY_ARGB(0xFF,0x6E,0xC8,0xE8));
	g_place.name = addInput(dialog, 50, "名称", NULL);
	g_place.x = addInput(dialog, 88, "位置 X", &g_place.positionLabels[0]);
	g_place.y = addInput(dialog, 126, "位置 Y", &g_place.positionLabels[1]);
	g_place.z = addInput(dialog, 164, "位置 Z", &g_place.positionLabels[2]);
	const char* targets[] = { "目标 X", "目标 Y", "目标 Z" };
	for (int i = 0; i < 3; ++i) g_place.targetInputs[i] = addInput(dialog, 202 + i * 38, targets[i], &g_place.targetLabels[i]);
	g_place.rotationY = addInput(dialog, 202, "旋转 Y", &g_place.rotationLabel);
	g_place.scale = addInput(dialog, 240, "缩放", &g_place.scaleLabel);
	g_place.detailInputs[0] = addInput(dialog, 278, "网格 A", &g_place.detailLabels[0]);
	g_place.detailInputs[1] = addInput(dialog, 316, "网格 B", &g_place.detailLabels[1]);
	g_place.strength = addInput(dialog, 202, "强度", &g_place.strengthLabel);
	g_place.colorLabel = placeLabel(dialog, 22, 370, 112, 28, "颜色", GY_ARGB(0xFF,0xB4,0xBF,0xCE));
	g_place.colorSwatch = placeButton(dialog, 142, 370, 100, "", colorClicked);
	g_place.wireframe = YMGUI_Creat_Checkbox_Creat(dialog, 142, 412, 180, 28);
	YMGUI_Checkbox_SetText(g_place.wireframe, "显示线框");
	g_place.shadows = YMGUI_Creat_Checkbox_Creat(dialog, 142, 412, 180, 28);
	YMGUI_Checkbox_SetText(g_place.shadows, "启用阴影");
	g_place.error = placeLabel(dialog, 24, 456, 392, 28, "", GY_ARGB(0xFF,0xEE,0x7C,0x72));
	placeButton(dialog, 224, 554, 90, "取消", cancelClicked);
	placeButton(dialog, 324, 554, 90, "确定", confirmClicked);
	YMGUI_Obj_SetHidden(g_place.modal, 1);
}
