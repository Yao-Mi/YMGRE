#include "scene_editor_import.h"

#include "YMGUI_Button.h"
#include "YMGUI_Checkbox.h"
#include "YMGUI_Label.h"
#include "YMGUI_TreeView.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
	GYOBJ modal, tree, pathLabel, status, wireframe;
	char root[PATH_MAX];
	char selected[PATH_MAX];
	SceneEditorImportCb importCb;
	void* userData;
} ImportState;

static ImportState g_import;
static void loadDirectory(GYTREENODE parent, const char* path);

static void reloadCurrentDirectory(void)
{
	g_import.selected[0] = '\0';
	YMGUI_Label_SetText(g_import.pathLabel, g_import.root);
	YMGUI_TreeView_Clear(g_import.tree);
	loadDirectory(NULL, g_import.root);
}

static void joinPath(char* output, size_t capacity, const char* directory, const char* name)
{
	size_t length = strlen(directory);
	if (length >= capacity) length = capacity - 1;
	memcpy(output, directory, length);
	if (length > 0 && output[length - 1] != '/' && length + 1 < capacity) output[length++] = '/';
	size_t index = 0;
	while (name[index] != '\0' && length + 1 < capacity) output[length++] = name[index++];
	output[length] = '\0';
}

static void buildPath(GYTREENODE node, char* output, size_t capacity)
{
	if (node == NULL) { snprintf(output, capacity, "%s", g_import.root); return; }
	buildPath(YMGUI_TreeView_NodeParent(node), output, capacity);
	char path[PATH_MAX];
	joinPath(path, sizeof(path), output, YMGUI_TreeView_NodeName(node));
	snprintf(output, capacity, "%s", path);
}

static uint8 isMeshFile(const char* name)
{
	const char* extension = strrchr(name, '.');
	return extension != NULL && strcmp(extension, ".mesh") == 0;
}

static uint8 validateDependencies(const char* meshPath, char* message, size_t capacity)
{
	if (access(meshPath, R_OK) != 0) {
		snprintf(message, capacity, "无法读取网格文件：%.48s", strrchr(meshPath, '/') != NULL ? strrchr(meshPath, '/') + 1 : meshPath);
		return 0;
	}
	size_t length = strlen(meshPath);
	if (length < 5 || length + 4 >= PATH_MAX) {
		snprintf(message, capacity, "网格文件路径无效");
		return 0;
	}
	char materialPath[PATH_MAX];
	snprintf(materialPath, sizeof(materialPath), "%s", meshPath);
	snprintf(materialPath + length - 5, sizeof(materialPath) - length + 5, ".material");
	FILE* material = fopen(materialPath, "r");
	if (material == NULL) {
		const char* name = strrchr(materialPath, '/');
		snprintf(message, capacity, "缺少或无法读取同名材质：%.48s", name != NULL ? name + 1 : materialPath);
		return 0;
	}
	char directory[PATH_MAX];
	snprintf(directory, sizeof(directory), "%s", materialPath);
	char* slash = strrchr(directory, '/');
	if (slash != NULL) *slash = '\0'; else snprintf(directory, sizeof(directory), ".");
	char line[1024];
	while (fgets(line, sizeof(line), material) != NULL) {
		char* cursor = line;
		while (*cursor == ' ' || *cursor == '\t') cursor++;
		if (strncmp(cursor, "texture", 7) != 0 || (cursor[7] != ' ' && cursor[7] != '\t')) continue;
		cursor += 7;
		while (*cursor == ' ' || *cursor == '\t') cursor++;
		char texture[PATH_MAX]; size_t count = 0;
		while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n' &&
			*cursor != ' ' && *cursor != '\t' && count + 1 < sizeof(texture))
			texture[count++] = *cursor++;
		texture[count] = '\0';
		if (count == 0) continue;
		char texturePath[PATH_MAX];
		if (texture[0] == '/') snprintf(texturePath, sizeof(texturePath), "%s", texture);
		else joinPath(texturePath, sizeof(texturePath), directory, texture);
		if (access(texturePath, R_OK) != 0) {
			fclose(material);
			snprintf(message, capacity, "材质引用的贴图不存在：%.48s", texture);
			return 0;
		}
	}
	if (ferror(material)) {
		fclose(material);
		snprintf(message, capacity, "读取 .material 文件时发生错误");
		return 0;
	}
	fclose(material);
	return 1;
}

static void loadDirectory(GYTREENODE parent, const char* path)
{
	DIR* directory = opendir(path);
	if (directory == NULL) return;
	struct dirent* entry;
	while ((entry = readdir(directory)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
		char full[PATH_MAX]; struct stat info;
		joinPath(full, sizeof(full), path, entry->d_name);
		if (stat(full, &info) == 0 && S_ISDIR(info.st_mode))
			YMGUI_TreeView_AddNode(g_import.tree, parent, entry->d_name, 1);
	}
	rewinddir(directory);
	while ((entry = readdir(directory)) != NULL) {
		if (!isMeshFile(entry->d_name)) continue;
		char full[PATH_MAX]; struct stat info;
		joinPath(full, sizeof(full), path, entry->d_name);
		if (stat(full, &info) == 0 && S_ISREG(info.st_mode))
			YMGUI_TreeView_AddNode(g_import.tree, parent, entry->d_name, 0);
	}
	closedir(directory);
}

static void treeExpanded(GYOBJ tree, GYTREENODE node)
{
	(void)tree;
	char path[PATH_MAX]; buildPath(node, path, sizeof(path));
	loadDirectory(node, path);
}

static void treeSelected(GYOBJ tree, GYTREENODE node)
{
	(void)tree;
	buildPath(node, g_import.selected, sizeof(g_import.selected));
	YMGUI_Label_SetText(g_import.pathLabel, g_import.selected);
	YMGUI_Label_SetText(g_import.status,
		YMGUI_TreeView_NodeIsDir(node) ? "展开目录并选择 .mesh 文件" : "已选择 Ogre .mesh 文件");
}

static void treeActivated(GYOBJ tree, GYTREENODE node)
{
	(void)tree;
	if (YMGUI_TreeView_NodeIsDir(node)) {
		char path[PATH_MAX];
		buildPath(node, path, sizeof(path));
		snprintf(g_import.root, sizeof(g_import.root), "%s", path);
		reloadCurrentDirectory();
		YMGUI_Label_SetText(g_import.status, "已进入目录；可用上一级返回");
	}
}

static void upClicked(GYOBJ button)
{
	(void)button;
	if (strcmp(g_import.root, "/") != 0) {
		size_t length = strlen(g_import.root);
		while (length > 1 && g_import.root[length - 1] == '/') g_import.root[--length] = '\0';
		char* slash = strrchr(g_import.root, '/');
		if (slash == g_import.root) g_import.root[1] = '\0';
		else if (slash != NULL) *slash = '\0';
		else snprintf(g_import.root, sizeof(g_import.root), ".");
	}
	reloadCurrentDirectory();
	YMGUI_Label_SetText(g_import.status, "已返回上一级目录");
}

static void enterClicked(GYOBJ button)
{
	(void)button;
	GYTREENODE selected = YMGUI_TreeView_GetSelectedNode(g_import.tree);
	if (selected == NULL || !YMGUI_TreeView_NodeIsDir(selected)) {
		YMGUI_Label_SetText(g_import.status, "请先选择一个目录");
		return;
	}
	treeActivated(g_import.tree, selected);
}

static void closeClicked(GYOBJ button)
{
	(void)button; YMGUI_Obj_SetHidden(g_import.modal, 1);
}

static void importClicked(GYOBJ button)
{
	(void)button;
	if (!isMeshFile(g_import.selected)) {
		YMGUI_Label_SetText(g_import.status, "请先选择 .mesh 文件");
		return;
	}
	char message[160];
	if (!validateDependencies(g_import.selected, message, sizeof(message))) {
		YMGUI_Label_SetText(g_import.status, message);
		return;
	}
	uint8 wireframe = YMGUI_Checkbox_GetChecked(g_import.wireframe);
	if (g_import.importCb != NULL && g_import.importCb(g_import.selected, wireframe, g_import.userData))
		YMGUI_Obj_SetHidden(g_import.modal, 1);
	else
		YMGUI_Label_SetText(g_import.status, "资源完整，但网格解析失败；文件可能损坏或格式不受支持");
}

static GYOBJ dialogButton(GYOBJ parent, GYcoord x, const char* text, GYbtn_clicked_cb callback)
{
	GYOBJ button = YMGUI_Creat_Button_Creat(parent, x, 504, 104, 30);
	YMGUI_Button_SetText(button, text); YMGUI_Button_SetClicked(button, callback);
	return button;
}

void SceneEditorImport_Build(GYCTX context, SceneEditorImportCb importCb, void* userData)
{
	memset(&g_import, 0, sizeof(g_import));
	g_import.importCb = importCb; g_import.userData = userData;
	const char* configured = getenv("YMGRE_IMPORT_ROOT");
	if (configured != NULL && configured[0] != '\0') snprintf(g_import.root, sizeof(g_import.root), "%s", configured);
	else if (getcwd(g_import.root, sizeof(g_import.root)) == NULL) snprintf(g_import.root, sizeof(g_import.root), ".");
	GYOBJ top = YMGUI_Ctx_GetTopLayer(context);
	g_import.modal = YMGUI_Creat_Obj_Creat(top, 0, 0, 1024, 680);
	YMGUI_Obj_SetBgColor(g_import.modal, GY_ARGB(0xFF,0x10,0x14,0x1C));
	GYOBJ dialog = YMGUI_Creat_Obj_Creat(g_import.modal, 112, 54, 800, 570);
	YMGUI_Obj_SetBgColor(dialog, GY_ARGB(0xFF,0x27,0x31,0x42));
	GYOBJ title = YMGUI_Creat_Label_Creat(dialog, 20, 16, 760, 24);
	YMGUI_Label_SetText(title, "导入外部 Ogre 网格");
	dialogButton(dialog, 20, "上一级", upClicked)->area.y = 46;
	dialogButton(dialog, 136, "进入目录", enterClicked)->area.y = 46;
	g_import.pathLabel = YMGUI_Creat_Label_Creat(dialog, 252, 50, 528, 24);
	g_import.tree = YMGUI_Creat_TreeView_Creat(dialog, 20, 82, 760, 348);
	YMGUI_TreeView_SetRowHeight(g_import.tree, 27); YMGUI_TreeView_SetIndent(g_import.tree, 18);
	YMGUI_TreeView_SetExpandCb(g_import.tree, treeExpanded);
	YMGUI_TreeView_SetSelectCb(g_import.tree, treeSelected);
	YMGUI_TreeView_SetActivateCb(g_import.tree, treeActivated);
	g_import.wireframe = YMGUI_Creat_Checkbox_Creat(dialog, 20, 442, 260, 26);
	YMGUI_Checkbox_SetText(g_import.wireframe, "显示渲染线框");
	GYOBJ dependencyHint = YMGUI_Creat_Label_Creat(dialog, 294, 444, 486, 22);
	YMGUI_Label_SetText(dependencyHint, "要求：同目录同名 .material，且其引用贴图必须存在");
	g_import.status = YMGUI_Creat_Label_Creat(dialog, 20, 474, 760, 24);
	dialogButton(dialog, 560, "取消", closeClicked);
	dialogButton(dialog, 676, "导入", importClicked);
	YMGUI_Obj_SetHidden(g_import.modal, 1);
}

void SceneEditorImport_Open(void)
{
	reloadCurrentDirectory();
	YMGUI_Label_SetText(g_import.status, "请选择 .mesh；同目录必须有同名 .material 及其引用贴图");
	YMGUI_Checkbox_SetChecked(g_import.wireframe, 0);
	YMGUI_Obj_SetHidden(g_import.modal, 0);
}
