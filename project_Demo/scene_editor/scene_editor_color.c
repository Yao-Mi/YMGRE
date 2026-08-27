#include "scene_editor_color.h"

#include "YMGUI_Button.h"
#include "YMGUI_ColorPicker.h"
#include "YMGUI_Label.h"

typedef struct {
	GYOBJ modal;
	GYOBJ picker;
	SceneEditorColorSelectedCb selectedCb;
	void* userData;
} ColorDialogState;

static ColorDialogState g_colorDialog;

static GYOBJ dialogButton(GYOBJ parent, GYcoord x, const char* text, GYbtn_clicked_cb clicked)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, x, 318, 90, 30);
	if (button) {
		YMGUI_Button_SetText(button, text);
		YMGUI_Button_SetColors(button, GY_ARGB(0xFF,0x2C,0x35,0x48), GY_ARGB(0xFF,0x3C,0x86,0xB8));
		YMGUI_Button_SetClicked(button, clicked);
	}
	return button;
}

static void colorCancel(GYOBJ button)
{
	(void)button;
	YMGUI_Obj_SetHidden(g_colorDialog.modal, 1);
}

static void colorConfirm(GYOBJ button)
{
	(void)button;
	GYcolor color = YMGUI_ColorPicker_GetColor(g_colorDialog.picker);
	YMGUI_Obj_SetHidden(g_colorDialog.modal, 1);
	if (g_colorDialog.selectedCb) g_colorDialog.selectedCb(color, g_colorDialog.userData);
}

void SceneEditorColor_Build(GYCTX context)
{
	GYOBJ top = YMGUI_Ctx_GetTopLayer(context);
	g_colorDialog.modal = YMGUI_Creat_Obj_Creat(top, 0, 0, 1024, 680);
	YMGUI_Obj_SetBgColor(g_colorDialog.modal, GY_ARGB(0xFF,0x10,0x14,0x1C));
	GYOBJ dialog = YMGUI_Creat_Obj_Creat(g_colorDialog.modal, 312, 136, 400, 400);
	YMGUI_Obj_SetBgColor(dialog, GY_ARGB(0xFF,0x27,0x31,0x42));
	GYOBJ title = YMGUI_Creat_Label_Creat(dialog, 20, 16, 360, 28);
	YMGUI_Label_SetText(title, "选择颜色");
	YMGUI_Label_SetTextColor(title, GY_ARGB(0xFF,0x6E,0xC8,0xE8));
	g_colorDialog.picker = YMGUI_Creat_ColorPicker_Creat(dialog, 46, 58, 308, 236);
	dialogButton(dialog, 204, "取消", colorCancel);
	dialogButton(dialog, 304, "确定", colorConfirm);
	YMGUI_Obj_SetHidden(g_colorDialog.modal, 1);
}

void SceneEditorColor_Open(GYcolor initialColor, SceneEditorColorSelectedCb selectedCb, void* userData)
{
	if (g_colorDialog.modal == NULL || g_colorDialog.picker == NULL) return;
	g_colorDialog.selectedCb = selectedCb;
	g_colorDialog.userData = userData;
	YMGUI_ColorPicker_SetColor(g_colorDialog.picker, initialColor);
	YMGUI_Obj_SetHidden(g_colorDialog.modal, 0);
}
