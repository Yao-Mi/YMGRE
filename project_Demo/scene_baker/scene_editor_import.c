#include "scene_editor_import.h"
#include "scene_editor_file_dialog.h"
#include "scene_editor_model.h"

#include "YMGUI_Checkbox.h"
#include "YMGUI_FileDialog.h"
#include "YMGUI_MsgBox.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define IMPORT_DIALOG_WIDTH 760
#define IMPORT_DIALOG_HEIGHT 600

typedef struct {
	GYOBJ dialog, wireframe, errorBox;
	char root[SCENE_EDITOR_PATH_CAPACITY];
	char lastPath[SCENE_EDITOR_PATH_CAPACITY];
	SceneEditorImportCb importCb;
	void* userData;
} ImportState;

static ImportState g_import;
static void openImport(uint8 resetOptions);

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

static uint8 isMeshFile(const char* name)
{
	const char* extension = strrchr(name, '.');
	if (extension == NULL || strlen(extension) != 5) return 0;
	const char expected[] = ".mesh";
	for (uint8 i = 0; i < 5; ++i) {
		char value = extension[i];
		if (value >= 'A' && value <= 'Z') value = (char)(value - 'A' + 'a');
		if (value != expected[i]) return 0;
	}
	return 1;
}

static void materialPathForMesh(const char* meshPath, char* materialPath, size_t capacity)
{
	size_t length = strlen(meshPath);
	snprintf(materialPath, capacity, "%s", meshPath);
	if (length >= 5 && length + 4 < capacity)
		snprintf(materialPath + length - 5, capacity - length + 5, ".material");
}

static const char* pathName(const char* path)
{
	const char* slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

uint8 SceneEditorImport_Validate(const char* meshPath, char* message, size_t capacity)
{
	if (meshPath == NULL || !isMeshFile(meshPath)) {
		snprintf(message, capacity, "请选择 Ogre .mesh 文件");
		return 0;
	}
	if (access(meshPath, R_OK) != 0) {
		snprintf(message, capacity, "无法读取网格文件：%.48s", strrchr(meshPath, '/') != NULL ? strrchr(meshPath, '/') + 1 : meshPath);
		return 0;
	}
	size_t length = strlen(meshPath);
	if (length < 5 || length + 4 >= PATH_MAX) {
		snprintf(message, capacity, "网格文件路径无效");
		return 0;
	}
	char materialPath[PATH_MAX]; materialPathForMesh(meshPath, materialPath, sizeof(materialPath));
	FILE* material = fopen(materialPath, "r");
	if (material == NULL) {
		snprintf(message, capacity, "材质文件不存在或无法读取：%.72s", pathName(materialPath));
		return 0;
	}
	char directory[PATH_MAX];
	snprintf(directory, sizeof(directory), "%s", materialPath);
	char* slash = strrchr(directory, '/');
	if (slash != NULL) *slash = '\0'; else snprintf(directory, sizeof(directory), ".");
	char line[1024], currentMaterial[96] = "未命名材质";
	while (fgets(line, sizeof(line), material) != NULL) {
		char* cursor = line;
		while (*cursor == ' ' || *cursor == '\t') cursor++;
		if (strncmp(cursor, "material", 8) == 0 && (cursor[8] == ' ' || cursor[8] == '\t')) {
			cursor += 8; while (*cursor == ' ' || *cursor == '\t') cursor++;
			size_t count = 0;
			while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n' &&
				*cursor != ' ' && *cursor != '\t' && *cursor != ':' && count + 1 < sizeof(currentMaterial))
				currentMaterial[count++] = *cursor++;
			currentMaterial[count] = '\0';
			continue;
		}
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
			snprintf(message, capacity, "材质「%.48s」（%.48s）引用的贴图不存在：%.72s",
				currentMaterial, pathName(materialPath), texture);
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

static uint8 materialScriptDefines(FILE* material, const char* requiredName)
{
	char line[1024]; rewind(material);
	while (fgets(line, sizeof(line), material) != NULL) {
		char* cursor = line; while (*cursor == ' ' || *cursor == '\t') cursor++;
		if (strncmp(cursor, "material", 8) != 0 || (cursor[8] != ' ' && cursor[8] != '\t')) continue;
		cursor += 8; while (*cursor == ' ' || *cursor == '\t') cursor++;
		char name[128]; size_t count = 0;
		while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n' &&
			*cursor != ' ' && *cursor != '\t' && *cursor != ':' && count + 1 < sizeof(name))
			name[count++] = *cursor++;
		name[count] = '\0';
		if (strcmp(name, requiredName) == 0) return 1;
	}
	return 0;
}

uint8 SceneEditorImport_ValidateLoadedMaterials(const char* meshPath, GRE_Object4d mesh,
	char* message, size_t capacity)
{
	if (meshPath == NULL || mesh == NULL) {
		snprintf(message, capacity, "无法检查网格使用的材质"); return 0;
	}
	char materialPath[PATH_MAX]; materialPathForMesh(meshPath, materialPath, sizeof(materialPath));
	FILE* material = fopen(materialPath, "r");
	if (material == NULL) {
		snprintf(message, capacity, "材质文件不存在或无法读取：%.72s", pathName(materialPath)); return 0;
	}
	for (GRE_Object4d part = mesh; part != NULL; part = part->nextObject) {
		if (part->materiaName == NULL || part->materiaName[0] == '\0') continue;
		if (!materialScriptDefines(material, part->materiaName)) {
			snprintf(message, capacity, "材质「%.72s」未在材质文件 %.72s 中定义",
				part->materiaName, pathName(materialPath));
			fclose(material); return 0;
		}
	}
	fclose(material); return 1;
}

static uint8 visibleMeshEntry(GYOBJ dialog, GYfiledialog_mode mode,
	const char* parentPath, const GYfiledialog_entry* entry, void* user)
{
	(void)dialog; (void)mode; (void)parentPath; (void)user;
	/* Dependency validation belongs to importResult; orphan meshes remain visible. */
	return entry->is_dir || isMeshFile(entry->name);
}

static void reopenImport(GYOBJ messageBox, int index)
{
	(void)messageBox; (void)index; openImport(0);
}

static void showImportError(const char* message)
{
	if (g_import.errorBox == NULL) {
		fprintf(stderr, "scene editor import: %s\n", message); return;
	}
	YMGUI_MsgBox_ClearButtons(g_import.errorBox);
	YMGUI_MsgBox_SetTitle(g_import.errorBox, "无法导入网格");
	YMGUI_MsgBox_SetText(g_import.errorBox, message);
	YMGUI_MsgBox_AddButton(g_import.errorBox, "返回", reopenImport);
	YMGUI_MsgBox_Show(g_import.errorBox);
}

static void importResult(GYOBJ dialog, uint8 accepted, const char* path, void* user)
{
	(void)dialog; (void)user;
	if (!accepted || path == NULL) return;
	snprintf(g_import.lastPath, sizeof(g_import.lastPath), "%s", path);
	char message[160];
	if (!SceneEditorImport_Validate(path, message, sizeof(message))) {
		showImportError(message); return;
	}
	uint8 wireframe = g_import.wireframe != NULL ?
		YMGUI_Checkbox_GetChecked(g_import.wireframe) : 0;
	if (g_import.importCb == NULL || !g_import.importCb(path, wireframe, g_import.userData))
		showImportError("资源完整，但网格解析失败；文件可能损坏或格式不受支持");
}

void SceneEditorImport_Build(GYCTX context, SceneEditorImportCb importCb, void* userData)
{
	memset(&g_import, 0, sizeof(g_import));
	g_import.importCb = importCb; g_import.userData = userData;
	const char* configured = getenv("YMGRE_IMPORT_ROOT");
	if (configured != NULL && configured[0] != '\0')
		snprintf(g_import.root, sizeof(g_import.root), "%s", configured);
	else if (getcwd(g_import.root, sizeof(g_import.root)) == NULL)
		snprintf(g_import.root, sizeof(g_import.root), ".");
	g_import.dialog = YMGUI_Creat_FileDialog_Creat(context, IMPORT_DIALOG_WIDTH,
		IMPORT_DIALOG_HEIGHT, SCENE_EDITOR_PATH_CAPACITY - 1, 255, 256);
	if (g_import.dialog == NULL) return;
	YMGUI_FileDialog_SetFS(g_import.dialog, SceneEditorFileDialog_PosixFS(), NULL);
	YMGUI_FileDialog_SetResultCb(g_import.dialog, importResult, NULL);
	YMGUI_FileDialog_SetFilterCb(g_import.dialog, visibleMeshEntry, NULL);
	GYcoord cardX = (context->top_layer->area.w - IMPORT_DIALOG_WIDTH) / 2;
	GYcoord cardY = (context->top_layer->area.h - IMPORT_DIALOG_HEIGHT) / 2;
	g_import.wireframe = YMGUI_Creat_Checkbox_Creat(g_import.dialog,
		cardX + 16, cardY + IMPORT_DIALOG_HEIGHT - 30, 260, 28);
	if (g_import.wireframe != NULL)
		YMGUI_Checkbox_SetText(g_import.wireframe, "显示渲染线框");
	g_import.errorBox = YMGUI_Creat_MsgBox_Creat(context);
}

static void openImport(uint8 resetOptions)
{
	if (g_import.dialog == NULL) return;
	char directory[SCENE_EDITOR_PATH_CAPACITY], name[256] = "";
	if (g_import.lastPath[0] != '\0') {
		snprintf(directory, sizeof(directory), "%s", g_import.lastPath);
		char* slash = strrchr(directory, '/');
		if (slash != NULL) {
			snprintf(name, sizeof(name), "%s", slash + 1);
			if (slash == directory) directory[1] = '\0'; else *slash = '\0';
		} else {
			size_t length = strlen(directory);
			if (length >= sizeof(name)) length = sizeof(name) - 1;
			memcpy(name, directory, length); name[length] = '\0';
			snprintf(directory, sizeof(directory), ".");
		}
	} else snprintf(directory, sizeof(directory), "%s", g_import.root);
	if (resetOptions && g_import.wireframe != NULL)
		YMGUI_Checkbox_SetChecked(g_import.wireframe, 0);
	if (!YMGUI_FileDialog_Show(g_import.dialog, GY_FILE_DIALOG_OPEN_FILE, directory, name)) {
		if (getcwd(directory, sizeof(directory)) != NULL)
			YMGUI_FileDialog_Show(g_import.dialog, GY_FILE_DIALOG_OPEN_FILE, directory, "");
	}
}

void SceneEditorImport_Open(void) { openImport(1); }
