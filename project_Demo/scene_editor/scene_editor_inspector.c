#include "scene_editor_inspector.h"
#include "scene_editor_color.h"
#include "scene_editor_place.h"

#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Label.h"
#include "YMGUI_List.h"
#include "YMGUI_TextInput.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	GYOBJ checkbox;
	GRE_Object4d part;
} SubmeshControl;

typedef enum {
	VALUE_X, VALUE_Y, VALUE_Z, VALUE_SCALE, VALUE_ROTATION_Y,
	VALUE_TARGET_X, VALUE_TARGET_Y, VALUE_TARGET_Z, VALUE_STRENGTH,
	VALUE_COUNT
} InspectorValue;

typedef struct {
	GYOBJ name, type;
	GYOBJ positionGroup, scaleGroup, topologyGroup, materialGroup, lightGroup, cameraGroup, targetTitle;
	GYOBJ inputs[VALUE_COUNT];
	GYOBJ detailLabels[2], detailInputs[2];
	GYOBJ meshSwatch, lightSwatch, wireframe, fixed, shadows;
	GYOBJ submeshTitle, submeshList;
	SubmeshControl* submeshes;
	uint16 submeshCount;
	SceneEditorObject* submeshObject;
	GRE_Object4d submeshMesh;
	GYOBJ repeatButtons[16];
	uint8 repeatButtonCount;
	uint8 updating;
	SceneEditorObject* object;
	SceneEditorInspectorChangedCb changedCb;
	SceneEditorInspectorRemeshCb remeshCb;
	void* userData;
} InspectorState;

static InspectorState g_inspector;

static GYOBJ inspectorLabel(GYOBJ parent, GYcoord x, GYcoord y, GYcoord w, GYcoord h,
	const char* text, GYcolor color)
{
	GYOBJ label = YMGUI_Creat_Label_Creat(parent, x, y, w, h);
	if (label != NULL) {
		YMGUI_Label_SetText(label, text);
		YMGUI_Label_SetTextColor(label, color);
	}
	return label;
}

static GYOBJ transparentGroup(GYOBJ parent, GYcoord y, GYcoord height)
{
	GYOBJ group = YMGUI_Creat_Obj_Creat(parent, 0, y, 212, height);
	if (group != NULL) group->draw_cb = NULL;
	return group;
}

static void notifyChanged(const char* status)
{
	if (g_inspector.changedCb != NULL)
		g_inspector.changedCb(status, g_inspector.userData);
}

static float32* objectValue(InspectorValue value)
{
	SceneEditorObject* object = g_inspector.object;
	if (object == NULL) return NULL;
	switch (value) {
	case VALUE_X: return &object->x;
	case VALUE_Y: return &object->y;
	case VALUE_Z: return &object->z;
	case VALUE_SCALE: return &object->scale;
	case VALUE_ROTATION_Y: return &object->rotY;
	case VALUE_TARGET_X: return &object->targetX;
	case VALUE_TARGET_Y: return &object->targetY;
	case VALUE_TARGET_Z: return &object->targetZ;
	default: return &object->strength;
	}
}

static void setInputValue(InspectorValue value)
{
	float32* current = objectValue(value);
	if (current == NULL || g_inspector.inputs[value] == NULL) return;
	char text[32];
	snprintf(text, sizeof(text), "%.2f", *current);
	YMGUI_TextInput_SetText(g_inspector.inputs[value], text);
}

static int applyValue(InspectorValue value, float32 next)
{
	SceneEditorObject* object = g_inspector.object;
	float32* current = objectValue(value);
	if (object == NULL || current == NULL || !isfinite(next)) return 0;
	if (value == VALUE_SCALE) {
		if (object->kind != SCENE_OBJECT_MESH || next <= 0.001f) return 0;
		SceneEditorObject_SetScale(object, next);
	} else if (value == VALUE_ROTATION_Y) {
		if (object->kind != SCENE_OBJECT_MESH) return 0;
		SceneEditorObject_SetRotationY(object, next);
	} else if (value <= VALUE_Z) {
		float32 delta = next - *current;
		SceneEditorObject_Translate(object,
			value == VALUE_X ? delta : 0,
			value == VALUE_Y ? delta : 0,
			value == VALUE_Z ? delta : 0);
	} else {
		if (value == VALUE_STRENGTH && (object->kind != SCENE_OBJECT_LIGHT || next < 0.0f)) return 0;
		*current = next;
		SceneEditorObject_SyncHandle(object);
	}
	notifyChanged(value == VALUE_STRENGTH ? "灯光强度已更新" :
		(value >= VALUE_TARGET_X && value <= VALUE_TARGET_Z ? "相机注视目标已更新" :
		(value == VALUE_ROTATION_Y ? "Y 轴旋转已更新" :
		(value == VALUE_SCALE ? "缩放已更新" : "位置已更新"))));
	return 1;
}

static void inputChanged(InspectorValue value, const char* text)
{
	char* end = NULL;
	float32 next = strtof(text, &end);
	if (text[0] != '\0' && end != text && *end == '\0') applyValue(value, next);
}

#define INPUT_CB(name, value) static void name(GYOBJ input, const char* text) \
	{ (void)input; inputChanged(value, text); }
INPUT_CB(xChanged, VALUE_X)
INPUT_CB(yChanged, VALUE_Y)
INPUT_CB(zChanged, VALUE_Z)
INPUT_CB(scaleChanged, VALUE_SCALE)
INPUT_CB(rotationYChanged, VALUE_ROTATION_Y)
INPUT_CB(targetXChanged, VALUE_TARGET_X)
INPUT_CB(targetYChanged, VALUE_TARGET_Y)
INPUT_CB(targetZChanged, VALUE_TARGET_Z)
INPUT_CB(strengthChanged, VALUE_STRENGTH)

static void stepValue(InspectorValue value, float32 delta)
{
	float32* current = objectValue(value);
	if (current != NULL && applyValue(value, *current + delta)) setInputValue(value);
}

#define STEP_CB(name, value, delta) static void name(GYOBJ button) \
	{ (void)button; stepValue(value, delta); }
STEP_CB(xPlus, VALUE_X, 1.0f) STEP_CB(xMinus, VALUE_X, -1.0f)
STEP_CB(yPlus, VALUE_Y, 1.0f) STEP_CB(yMinus, VALUE_Y, -1.0f)
STEP_CB(zPlus, VALUE_Z, 1.0f) STEP_CB(zMinus, VALUE_Z, -1.0f)
STEP_CB(scalePlus, VALUE_SCALE, 0.1f) STEP_CB(scaleMinus, VALUE_SCALE, -0.1f)
STEP_CB(rotationPlus, VALUE_ROTATION_Y, 5.0f) STEP_CB(rotationMinus, VALUE_ROTATION_Y, -5.0f)
STEP_CB(txPlus, VALUE_TARGET_X, 1.0f) STEP_CB(txMinus, VALUE_TARGET_X, -1.0f)
STEP_CB(tyPlus, VALUE_TARGET_Y, 1.0f) STEP_CB(tyMinus, VALUE_TARGET_Y, -1.0f)
STEP_CB(tzPlus, VALUE_TARGET_Z, 1.0f) STEP_CB(tzMinus, VALUE_TARGET_Z, -1.0f)

static void colorSelected(GYcolor color, void* userData)
{
	(void)userData;
	if (g_inspector.object == NULL) return;
	SceneEditorObject_SetColor(g_inspector.object, color);
	GYOBJ swatch = g_inspector.object->kind == SCENE_OBJECT_LIGHT ?
		g_inspector.lightSwatch : g_inspector.meshSwatch;
	YMGUI_Button_SetColors(swatch, color, color);
	notifyChanged(g_inspector.object->kind == SCENE_OBJECT_LIGHT ? "灯光颜色已更新" : "物体颜色已更新");
}

static void colorClicked(GYOBJ button)
{
	(void)button;
	if (g_inspector.object != NULL)
		SceneEditorColor_Open(g_inspector.object->color, colorSelected, NULL);
}

static void wireframeChanged(GYOBJ checkbox, uint8 checked)
{
	(void)checkbox;
	if (g_inspector.object == NULL || g_inspector.object->kind != SCENE_OBJECT_MESH) return;
	g_inspector.object->wireframe = checked;
	for (GRE_Object4d part = g_inspector.object->mesh; part != NULL; part = part->nextObject)
		part->wireFrame = checked;
	notifyChanged(checked ? "线框已显示" : "线框已隐藏");
}

static void fixedChanged(GYOBJ checkbox, uint8 checked)
{
	(void)checkbox;
	if (g_inspector.updating || g_inspector.object == NULL ||
		g_inspector.object->kind != SCENE_OBJECT_MESH) return;
	g_inspector.object->fixed = checked;
	notifyChanged(checked ? "对象已固定；视口不会拾取或拖动" : "对象已解锁；可从视口拾取和拖动");
}

static void submeshChanged(GYOBJ checkbox, uint8 checked)
{
	if (g_inspector.updating || g_inspector.object == NULL) return;
	for (uint16 i = 0; i < g_inspector.submeshCount; ++i) {
		if (g_inspector.submeshes[i].checkbox != checkbox) continue;
		if (g_inspector.submeshes[i].part != NULL)
			g_inspector.submeshes[i].part->isVisible = checked;
		notifyChanged(checked ? "子网格已显示" : "子网格已隐藏");
		return;
	}
}

static void rebuildSubmeshList(SceneEditorObject* object, uint16 count)
{
	if (g_inspector.submeshList != NULL) YMGUI_Free_ObjFree(g_inspector.submeshList);
	free(g_inspector.submeshes);
	g_inspector.submeshes = NULL;
	g_inspector.submeshCount = 0;
	g_inspector.submeshObject = object;
	g_inspector.submeshMesh = object != NULL ? object->mesh : NULL;
	g_inspector.submeshList = YMGUI_Creat_List_Creat(g_inspector.materialGroup, 8, 120, 196, 132);
	if (count == 0 || g_inspector.submeshList == NULL) return;
	g_inspector.submeshes = calloc(count, sizeof(*g_inspector.submeshes));
	if (g_inspector.submeshes == NULL) return;
	uint16 row = 0;
	for (GRE_Object4d part = object->mesh; part != NULL && row < count; part = part->nextObject, ++row) {
		GYOBJ item = YMGUI_List_AddItem(g_inspector.submeshList, "", 24);
		if (item == NULL) break;
		char label[64];
		const char* material = part->materiaName != NULL && part->materiaName[0] != '\0' ?
			part->materiaName : "未命名材质";
		snprintf(label, sizeof(label), "%u  %.16s", (unsigned)(row + 1), material);
		g_inspector.submeshes[row].checkbox = YMGUI_Creat_Checkbox_Creat(item, 2, 1, 172, 22);
		if (g_inspector.submeshes[row].checkbox == NULL) break;
		g_inspector.submeshes[row].part = part;
		YMGUI_Checkbox_SetText(g_inspector.submeshes[row].checkbox, label);
		YMGUI_Checkbox_SetChanged(g_inspector.submeshes[row].checkbox, submeshChanged);
	}
	g_inspector.submeshCount = row;
}

static void shadowsChanged(GYOBJ checkbox, uint8 checked)
{
	(void)checkbox;
	if (g_inspector.object == NULL || g_inspector.object->kind != SCENE_OBJECT_LIGHT ||
		g_inspector.object->lightType == GRE_GlobalLight) return;
	g_inspector.object->shadowsEnabled = checked;
	SceneEditorObject_SyncHandle(g_inspector.object);
	notifyChanged(checked ? "灯光阴影已启用" : "灯光阴影已关闭");
}

static uint16 detailMinimum(const SceneEditorObject* object, uint8 index)
{
	if (object->primitiveKind == SCENE_PLACE_PLANE) return 1;
	if (object->primitiveKind == SCENE_PLACE_SPHERE) return index == 0 ? 2 : 3;
	if (object->primitiveKind == SCENE_PLACE_TORUS) return 3;
	if (object->primitiveKind == SCENE_PLACE_CAPSULE) return index == 0 ? 1 : 3;
	return 3;
}

static int parseDetailInput(GYOBJ input, uint16 minimum, uint16* result)
{
	char* end = NULL;
	const char* text = YMGUI_TextInput_GetText(input);
	long value = strtol(text, &end, 10);
	if (text[0] == '\0' || end == text || *end != '\0' || value < minimum || value > 128) return 0;
	*result = (uint16)value;
	return 1;
}

static void detailChanged(GYOBJ input, const char* text)
{
	(void)input; (void)text;
	SceneEditorObject* object = g_inspector.object;
	if (g_inspector.updating || object == NULL || g_inspector.remeshCb == NULL) return;
	uint16 detailA, detailB = object->detailB;
	if (!parseDetailInput(g_inspector.detailInputs[0], detailMinimum(object, 0), &detailA)) return;
	if (!(g_inspector.detailInputs[1]->state & GY_STATE_Hidden) &&
		!parseDetailInput(g_inspector.detailInputs[1], detailMinimum(object, 1), &detailB)) return;
	if (detailA == object->detailA && detailB == object->detailB) return;
	if (g_inspector.remeshCb(object, detailA, detailB, g_inspector.userData))
		notifyChanged("网格细分已重新生成");
}

static GYOBJ smallButton(GYOBJ parent, GYcoord x, GYcoord y, const char* text,
	GYbtn_clicked_cb clicked)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, x, y, 28, 26);
	YMGUI_Button_SetText(button, text);
	YMGUI_Button_SetClicked(button, clicked);
	YMGUI_Button_SetRepeat(button, 400, 80);
	if (g_inspector.repeatButtonCount < sizeof(g_inspector.repeatButtons) / sizeof(g_inspector.repeatButtons[0]))
		g_inspector.repeatButtons[g_inspector.repeatButtonCount++] = button;
	return button;
}

static void valueRow(GYOBJ parent, InspectorValue value, GYcoord y, const char* axis,
	GYcolor axisColor, GYbtn_clicked_cb plus, GYbtn_clicked_cb minus, GYti_changed_cb changed)
{
	inspectorLabel(parent, 12, y + 4, 18, 20, axis, axisColor);
	smallButton(parent, 34, y, "+", plus);
	g_inspector.inputs[value] = YMGUI_Creat_TextInput_Creat(parent, 66, y, 92, 26);
	YMGUI_TextInput_SetChanged(g_inspector.inputs[value], changed);
	smallButton(parent, 162, y, "-", minus);
}

void SceneEditorInspector_Build(GYOBJ parent, SceneEditorInspectorChangedCb changedCb,
	SceneEditorInspectorRemeshCb remeshCb, void* userData)
{
	GYcolor fg = GY_ARGB(0xFF,0xE4,0xEA,0xF2), muted = GY_ARGB(0xFF,0x9A,0xA7,0xB8);
	GYcolor cyan = GY_ARGB(0xFF,0x6E,0xC8,0xE8);
	g_inspector.changedCb = changedCb; g_inspector.remeshCb = remeshCb; g_inspector.userData = userData;
	inspectorLabel(parent, 16, 14, 180, 22, "检查器", fg);
	g_inspector.name = inspectorLabel(parent, 12, 54, 188, 20, "未选择对象", muted);
	g_inspector.type = inspectorLabel(parent, 12, 80, 188, 20, "类型：--", muted);

	g_inspector.positionGroup = transparentGroup(parent, 112, 132);
	inspectorLabel(g_inspector.positionGroup, 16, 0, 180, 20, "位置", cyan);
	valueRow(g_inspector.positionGroup, VALUE_X, 28, "X", GY_ARGB(0xFF,0xE0,0x48,0x48), xPlus, xMinus, xChanged);
	valueRow(g_inspector.positionGroup, VALUE_Y, 60, "Y", GY_ARGB(0xFF,0x52,0xCD,0x70), yPlus, yMinus, yChanged);
	valueRow(g_inspector.positionGroup, VALUE_Z, 92, "Z", GY_ARGB(0xFF,0x46,0x87,0xEB), zPlus, zMinus, zChanged);

	g_inspector.scaleGroup = transparentGroup(parent, 250, 96);
	inspectorLabel(g_inspector.scaleGroup, 16, 0, 180, 20, "变换", cyan);
	valueRow(g_inspector.scaleGroup, VALUE_SCALE, 28, "S", muted, scalePlus, scaleMinus, scaleChanged);
	valueRow(g_inspector.scaleGroup, VALUE_ROTATION_Y, 60, "R", GY_ARGB(0xFF,0x52,0xCD,0x70),
		rotationPlus, rotationMinus, rotationYChanged);

	g_inspector.topologyGroup = transparentGroup(parent, 356, 96);
	inspectorLabel(g_inspector.topologyGroup, 16, 0, 180, 20, "网格细分", cyan);
	for (int i = 0; i < 2; ++i) {
		g_inspector.detailLabels[i] = inspectorLabel(g_inspector.topologyGroup, 12, 30 + i * 32,
			62, 24, i == 0 ? "网格 A" : "网格 B", muted);
		g_inspector.detailInputs[i] = YMGUI_Creat_TextInput_Creat(g_inspector.topologyGroup,
			78, 28 + i * 32, 112, 26);
		YMGUI_TextInput_SetChanged(g_inspector.detailInputs[i], detailChanged);
	}

	g_inspector.materialGroup = transparentGroup(parent, 458, 162);
	inspectorLabel(g_inspector.materialGroup, 16, 0, 180, 20, "材质", cyan);
	inspectorLabel(g_inspector.materialGroup, 12, 34, 54, 26, "颜色", muted);
	g_inspector.meshSwatch = YMGUI_Creat_Button_Creat(g_inspector.materialGroup, 70, 32, 88, 28);
	YMGUI_Button_SetClicked(g_inspector.meshSwatch, colorClicked);
	g_inspector.wireframe = YMGUI_Creat_Checkbox_Creat(g_inspector.materialGroup, 12, 68, 94, 26);
	YMGUI_Checkbox_SetText(g_inspector.wireframe, "显示线框");
	YMGUI_Checkbox_SetChanged(g_inspector.wireframe, wireframeChanged);
	g_inspector.fixed = YMGUI_Creat_Checkbox_Creat(g_inspector.materialGroup, 108, 68, 92, 26);
	YMGUI_Checkbox_SetText(g_inspector.fixed, "固定");
	YMGUI_Checkbox_SetChanged(g_inspector.fixed, fixedChanged);
	g_inspector.submeshTitle = inspectorLabel(g_inspector.materialGroup, 12, 98, 188, 20, "子网格", cyan);
	g_inspector.submeshList = YMGUI_Creat_List_Creat(g_inspector.materialGroup, 8, 120, 196, 132);

	g_inspector.lightGroup = transparentGroup(parent, 324, 148);
	inspectorLabel(g_inspector.lightGroup, 16, 0, 180, 20, "灯光", cyan);
	inspectorLabel(g_inspector.lightGroup, 12, 34, 54, 26, "颜色", muted);
	g_inspector.lightSwatch = YMGUI_Creat_Button_Creat(g_inspector.lightGroup, 70, 32, 88, 28);
	YMGUI_Button_SetClicked(g_inspector.lightSwatch, colorClicked);
	inspectorLabel(g_inspector.lightGroup, 12, 78, 52, 24, "强度", muted);
	g_inspector.inputs[VALUE_STRENGTH] = YMGUI_Creat_TextInput_Creat(g_inspector.lightGroup, 70, 74, 120, 28);
	YMGUI_TextInput_SetChanged(g_inspector.inputs[VALUE_STRENGTH], strengthChanged);
	g_inspector.shadows = YMGUI_Creat_Checkbox_Creat(g_inspector.lightGroup, 12, 110, 180, 26);
	YMGUI_Checkbox_SetText(g_inspector.shadows, "启用阴影");
	YMGUI_Checkbox_SetChanged(g_inspector.shadows, shadowsChanged);

	g_inspector.cameraGroup = transparentGroup(parent, 324, 132);
	g_inspector.targetTitle = inspectorLabel(g_inspector.cameraGroup, 16, 0, 180, 20, "注视目标", cyan);
	valueRow(g_inspector.cameraGroup, VALUE_TARGET_X, 28, "X", GY_ARGB(0xFF,0xE0,0x48,0x48), txPlus, txMinus, targetXChanged);
	valueRow(g_inspector.cameraGroup, VALUE_TARGET_Y, 60, "Y", GY_ARGB(0xFF,0x52,0xCD,0x70), tyPlus, tyMinus, targetYChanged);
	valueRow(g_inspector.cameraGroup, VALUE_TARGET_Z, 92, "Z", GY_ARGB(0xFF,0x46,0x87,0xEB), tzPlus, tzMinus, targetZChanged);
}

void SceneEditorInspector_SetObject(SceneEditorObject* object)
{
	g_inspector.updating = 1;
	g_inspector.object = object;
	if (object == NULL) {
		YMGUI_Label_SetText(g_inspector.name, "未选择对象");
		YMGUI_Label_SetText(g_inspector.type, "类型：--");
		YMGUI_Obj_SetHidden(g_inspector.positionGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.scaleGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.topologyGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.materialGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.lightGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.cameraGroup, 1);
		YMGUI_Obj_SetHidden(g_inspector.submeshTitle, 1);
		YMGUI_Obj_SetHidden(g_inspector.submeshList, 1);
		g_inspector.updating = 0;
		return;
	}
	char text[96];
	snprintf(text, sizeof(text), "选中：%.20s", object->name); YMGUI_Label_SetText(g_inspector.name, text);
	snprintf(text, sizeof(text), "类型：%.16s", object->type); YMGUI_Label_SetText(g_inspector.type, text);
	uint8 hasPosition = object->kind != SCENE_OBJECT_LIGHT || object->lightType != GRE_GlobalLight;
	uint8 hasTarget = object->kind == SCENE_OBJECT_CAMERA ||
		(object->kind == SCENE_OBJECT_LIGHT && object->lightType == GRE_SpotLight);
	YMGUI_Obj_SetHidden(g_inspector.positionGroup, !hasPosition);
	YMGUI_Obj_SetHidden(g_inspector.scaleGroup, object->kind != SCENE_OBJECT_MESH);
	uint8 hasTopology = object->kind == SCENE_OBJECT_MESH &&
		(object->primitiveKind == SCENE_PLACE_PLANE ||
		(object->primitiveKind >= SCENE_PLACE_SPHERE && object->primitiveKind <= SCENE_PLACE_CAPSULE));
	YMGUI_Obj_SetHidden(g_inspector.topologyGroup, !hasTopology);
	YMGUI_Obj_SetHidden(g_inspector.materialGroup, object->kind != SCENE_OBJECT_MESH);
	YMGUI_Obj_SetHidden(g_inspector.lightGroup, object->kind != SCENE_OBJECT_LIGHT);
	YMGUI_Obj_SetHidden(g_inspector.cameraGroup, !hasTarget);
	if (hasTarget) {
		g_inspector.cameraGroup->area.y = 250;
		YMGUI_Label_SetText(g_inspector.targetTitle,
			object->kind == SCENE_OBJECT_CAMERA ? "注视目标" : "照射目标");
	}
	if (object->kind == SCENE_OBJECT_LIGHT)
		g_inspector.lightGroup->area.y = !hasPosition ? 112 : (hasTarget ? 390 : 250);
	YMGUI_Obj_SetHidden(g_inspector.shadows,
		object->kind != SCENE_OBJECT_LIGHT || object->lightType == GRE_GlobalLight);
	for (int i = 0; i < VALUE_COUNT; ++i) setInputValue((InspectorValue)i);
	if (object->kind == SCENE_OBJECT_MESH) {
		g_inspector.materialGroup->area.y = hasTopology ? 458 : 356;
		if (hasTopology) {
			char value[16];
			snprintf(value, sizeof(value), "%u", object->detailA);
			YMGUI_TextInput_SetText(g_inspector.detailInputs[0], value);
			uint8 hasDetailB = object->primitiveKind == SCENE_PLACE_PLANE ||
				object->primitiveKind == SCENE_PLACE_SPHERE ||
				object->primitiveKind == SCENE_PLACE_TORUS ||
				object->primitiveKind == SCENE_PLACE_CAPSULE;
			YMGUI_Obj_SetHidden(g_inspector.detailLabels[1], !hasDetailB);
			YMGUI_Obj_SetHidden(g_inspector.detailInputs[1], !hasDetailB);
			if (hasDetailB) {
				snprintf(value, sizeof(value), "%u", object->detailB);
				YMGUI_TextInput_SetText(g_inspector.detailInputs[1], value);
			}
			const char* first = object->primitiveKind == SCENE_PLACE_PLANE ? "行" :
				(object->primitiveKind == SCENE_PLACE_SPHERE ? "纬向" :
				(object->primitiveKind == SCENE_PLACE_TORUS ? "主环" :
				(object->primitiveKind == SCENE_PLACE_CAPSULE ? "半球" : "环向")));
			YMGUI_Label_SetText(g_inspector.detailLabels[0], first);
			YMGUI_Label_SetText(g_inspector.detailLabels[1],
				object->primitiveKind == SCENE_PLACE_PLANE ? "列" :
				(object->primitiveKind == SCENE_PLACE_TORUS ? "截面" : "经向"));
		}
		YMGUI_Button_SetColors(g_inspector.meshSwatch, object->color, object->color);
		YMGUI_Checkbox_SetChecked(g_inspector.wireframe, object->wireframe);
		YMGUI_Checkbox_SetChecked(g_inspector.fixed, object->fixed);
		uint16 submeshCount = 0;
		for (GRE_Object4d part = object->mesh; part != NULL; part = part->nextObject) submeshCount++;
		uint8 showSubmeshes = submeshCount > 1;
		YMGUI_Obj_SetHidden(g_inspector.submeshTitle, !showSubmeshes);
		if (g_inspector.submeshObject != object || g_inspector.submeshMesh != object->mesh ||
			g_inspector.submeshCount != submeshCount)
			rebuildSubmeshList(object, submeshCount);
		YMGUI_Obj_SetHidden(g_inspector.submeshList, !showSubmeshes);
		for (uint16 i = 0; i < g_inspector.submeshCount; ++i)
			YMGUI_Checkbox_SetChecked(g_inspector.submeshes[i].checkbox,
				g_inspector.submeshes[i].part->isVisible);
		g_inspector.materialGroup->area.h = showSubmeshes ? 260 : 100;
	} else if (object->kind == SCENE_OBJECT_LIGHT) {
		YMGUI_Obj_SetHidden(g_inspector.submeshTitle, 1);
		YMGUI_Obj_SetHidden(g_inspector.submeshList, 1);
		YMGUI_Button_SetColors(g_inspector.lightSwatch, object->color, object->color);
		if (object->lightType != GRE_GlobalLight)
			YMGUI_Checkbox_SetChecked(g_inspector.shadows, object->shadowsEnabled);
	}
	g_inspector.updating = 0;
}

void SceneEditorInspector_Shutdown(void)
{
	free(g_inspector.submeshes);
	g_inspector.submeshes = NULL;
	g_inspector.submeshCount = 0;
}

void SceneEditorInspector_Tick(uint16 elapsedMs)
{
	for (uint8 i = 0; i < g_inspector.repeatButtonCount; ++i)
		YMGUI_Button_Tick(g_inspector.repeatButtons[i], elapsedMs);
}
