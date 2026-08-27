#include "scene_editor_hierarchy.h"

#include "YMGUI_Button.h"
#include "YMGUI_Label.h"
#include "YMGUI_Layout.h"
#include "YMGUI_TextInput.h"
#include <string.h>

typedef struct {
	GYOBJ backdrop, menu, renameButton, switchButton, deleteButton, duplicateButton, renameModal, renameInput;
	SceneEditorObject* object;
	SceneEditorHierarchyOps ops;
	void* userData;
} HierarchyState;

static HierarchyState g_hierarchy;

static void closeMenu(void)
{
	YMGUI_Obj_SetHidden(g_hierarchy.menu, 1);
	YMGUI_Obj_SetHidden(g_hierarchy.backdrop, 1);
}

static void backdropEvent(GYOBJ object, GYEvent event)
{
	(void)object;
	if (event == GY_EVENT_Clicked) closeMenu();
}

static GYOBJ actionButton(GYOBJ parent, GYcoord y, const char* text, GYbtn_clicked_cb clicked)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, 8, y, 158, 28);
	YMGUI_Button_SetText(button, text);
	YMGUI_Button_SetClicked(button, clicked);
	return button;
}

static void renameClicked(GYOBJ button)
{
	(void)button;
	if (g_hierarchy.object == NULL) return;
	YMGUI_TextInput_SetText(g_hierarchy.renameInput, g_hierarchy.object->name);
	closeMenu();
	YMGUI_Obj_SetHidden(g_hierarchy.renameModal, 0);
}

static void renameCancel(GYOBJ button)
{
	(void)button; YMGUI_Obj_SetHidden(g_hierarchy.renameModal, 1);
}

static void renameConfirm(GYOBJ button)
{
	(void)button;
	const char* name = YMGUI_TextInput_GetText(g_hierarchy.renameInput);
	if (g_hierarchy.object != NULL && name[0] != '\0' && g_hierarchy.ops.renameObject != NULL)
		g_hierarchy.ops.renameObject(g_hierarchy.object, name, g_hierarchy.userData);
	YMGUI_Obj_SetHidden(g_hierarchy.renameModal, 1);
}

static void deleteClicked(GYOBJ button)
{
	(void)button;
	closeMenu();
	if (g_hierarchy.object != NULL && g_hierarchy.ops.deleteObject != NULL)
		g_hierarchy.ops.deleteObject(g_hierarchy.object, g_hierarchy.userData);
}

static void switchClicked(GYOBJ button)
{
	(void)button;
	closeMenu();
	if (g_hierarchy.object != NULL && g_hierarchy.ops.switchCamera != NULL)
		g_hierarchy.ops.switchCamera(g_hierarchy.object, g_hierarchy.userData);
}

static void duplicateClicked(GYOBJ button)
{
	(void)button;closeMenu();
	if(g_hierarchy.object!=NULL&&g_hierarchy.ops.duplicateObject!=NULL)
		g_hierarchy.ops.duplicateObject(g_hierarchy.object,g_hierarchy.userData);
}

void SceneEditorHierarchy_Build(GYCTX context, const SceneEditorHierarchyOps* ops, void* userData)
{
	memset(&g_hierarchy, 0, sizeof(g_hierarchy));
	if (ops != NULL) g_hierarchy.ops = *ops;
	g_hierarchy.userData = userData;
	GYOBJ top = YMGUI_Ctx_GetTopLayer(context);
	g_hierarchy.backdrop = YMGUI_Creat_Obj_Creat(top, 0, 0, 1024, 680);
	g_hierarchy.backdrop->draw_cb = NULL;
	g_hierarchy.backdrop->event_cb = backdropEvent;
	g_hierarchy.menu = YMGUI_Creat_Obj_Creat(top, 0, 0, 174, 144);
	YMGUI_Obj_SetBgColor(g_hierarchy.menu, GY_ARGB(0xFF,0x25,0x2D,0x3C));
	g_hierarchy.renameButton = actionButton(g_hierarchy.menu, 8, "重命名", renameClicked);
	g_hierarchy.deleteButton = actionButton(g_hierarchy.menu, 40, "删除", deleteClicked);
	g_hierarchy.switchButton = actionButton(g_hierarchy.menu, 72, "切换到此视角", switchClicked);
	g_hierarchy.duplicateButton = actionButton(g_hierarchy.menu, 104, "克隆", duplicateClicked);
	YMGUI_Obj_SetHidden(g_hierarchy.backdrop, 1);
	YMGUI_Obj_SetHidden(g_hierarchy.menu, 1);

	g_hierarchy.renameModal = YMGUI_Creat_Obj_Creat(top, 0, 0, 1024, 680);
	YMGUI_Obj_SetBgColor(g_hierarchy.renameModal, GY_ARGB(0xFF,0x10,0x14,0x1C));
	GYOBJ dialog = YMGUI_Creat_Obj_Creat(g_hierarchy.renameModal, 312, 210, 400, 210);
	YMGUI_Obj_SetBgColor(dialog, GY_ARGB(0xFF,0x27,0x31,0x42));
	GYOBJ title = YMGUI_Creat_Label_Creat(dialog, 20, 18, 360, 24);
	YMGUI_Label_SetText(title, "重命名场景对象");
	g_hierarchy.renameInput = YMGUI_Creat_TextInput_Creat(dialog, 24, 68, 352, 30, 63);
	GYOBJ cancel = YMGUI_Creat_Button_Creat(dialog, 184, 142, 90, 30);
	YMGUI_Button_SetText(cancel, "取消"); YMGUI_Button_SetClicked(cancel, renameCancel);
	GYOBJ confirm = YMGUI_Creat_Button_Creat(dialog, 286, 142, 90, 30);
	YMGUI_Button_SetText(confirm, "确定"); YMGUI_Button_SetClicked(confirm, renameConfirm);
	YMGUI_Obj_SetHidden(g_hierarchy.renameModal, 1);
}

void SceneEditorHierarchy_Open(SceneEditorObject* object, GYcoord screenX, GYcoord screenY,
	uint8 canDelete)
{
	if (object == NULL) return;
	g_hierarchy.object = object;
	if (screenX > 842) screenX = 842;
	if (screenY > 568) screenY = 568;
	g_hierarchy.menu->area.x = screenX;
	g_hierarchy.menu->area.y = screenY;
	uint8 showSwitch=object->kind==SCENE_OBJECT_CAMERA;
	uint8 showDuplicate=!(object->kind==SCENE_OBJECT_LIGHT&&object->lightType==GRE_GlobalLight);
	YMGUI_Obj_SetHidden(g_hierarchy.deleteButton, !canDelete);
	YMGUI_Obj_SetHidden(g_hierarchy.switchButton, !showSwitch);
	YMGUI_Obj_SetHidden(g_hierarchy.duplicateButton, !showDuplicate);
	/* Stack 会跳过 Hidden 子项，动态重排可用命令，菜单不留下空行。 */
	YMGUI_Layout_Stack(g_hierarchy.menu,GY_LAYOUT_VER,4,8,GY_CROSS_START);
	GYcoord rows=1+(canDelete?1:0)+(showSwitch?1:0)+(showDuplicate?1:0);
	g_hierarchy.menu->area.h=8+rows*28+(rows-1)*4+8;
	YMGUI_Obj_SetHidden(g_hierarchy.backdrop, 0);
	YMGUI_Obj_SetHidden(g_hierarchy.menu, 0);
}
