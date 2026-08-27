#include "scene_editor_io.h"
#include "scene_editor_file_dialog.h"
#include "scene_editor_place.h"

#include "YMGUI_FileDialog.h"
#include "YMGUI_Checkbox.h"
#include <errno.h>
#include "mxml.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define YMGRE_SCENE_NS "https://ymgre.dev/schema/scene/1"
#define YMGRE_SCENE_VERSION "1"

typedef struct {
	GYOBJ dialog, ignoreErrors;
	SceneEditorIoPathCb saveCb, openCb;
	SceneEditorIoCancelCb cancelCb;
	void* userData;
	char lastPath[SCENE_EDITOR_PATH_CAPACITY];
} SceneIoState;

static SceneIoState g_io;

static const char* localName(mxml_node_t * node)
{
	const char* name = mxmlGetElement(node);
	const char* colon = name != NULL ? strchr(name, ':') : NULL;
	return colon != NULL ? colon + 1 : name;
}

static const char* sceneWhitespace(void* data, mxml_node_t* node, mxml_ws_t where)
{
	(void)data; static char whitespace[40];
	if (mxmlGetType(node) != MXML_TYPE_ELEMENT ||
		(where != MXML_WS_BEFORE_OPEN && where != MXML_WS_BEFORE_CLOSE)) return NULL;
	int depth = 0;
	for (mxml_node_t* parent = mxmlGetParent(node); parent != NULL; parent = mxmlGetParent(parent))
		if (mxmlGetType(parent) == MXML_TYPE_ELEMENT) depth++;
	if (where == MXML_WS_BEFORE_CLOSE) {
		mxml_node_t* first = mxmlGetFirstChild(node);
		if (first == NULL || mxmlGetType(first) != MXML_TYPE_ELEMENT) return NULL;
	}
	if (depth > 18) depth = 18;
	whitespace[0] = '\n'; for (int i = 0; i < depth; ++i) whitespace[i + 1] = '\t';
	whitespace[depth + 1] = '\0'; return whitespace;
}

static mxml_node_t * child(mxml_node_t * parent, const char* name)
{
	for (mxml_node_t * node = parent != NULL ? mxmlGetFirstChild(parent) : NULL; node != NULL;
		node = mxmlGetNextSibling(node))
		if (mxmlGetType(node) == MXML_TYPE_ELEMENT && strcmp(localName(node), name) == 0) return node;
	return NULL;
}

static int copyProp(mxml_node_t * node, const char* name, char* output, size_t capacity)
{
	if (node == NULL) return 0;
	const char* value = mxmlElementGetAttr(node, name);
	if (value == NULL) return 0;
	snprintf(output, capacity, "%s", value); return 1;
}

static int copyNsProp(mxml_node_t * node, const char* name, char* output, size_t capacity)
{
	if (node == NULL) return 0;
	char qualified[80]; snprintf(qualified, sizeof(qualified), "ymgre:%s", name);
	const char* value = mxmlElementGetAttr(node, qualified);
	if (value == NULL) return 0;
	snprintf(output, capacity, "%s", value); return 1;
}

static int parseFloatProp(mxml_node_t * node, const char* name, float32* value, float32 fallback)
{
	char text[64]; *value = fallback;
	if (!copyProp(node, name, text, sizeof(text))) return node == NULL ? 0 : 1;
	char* end = NULL; float parsed = strtof(text, &end);
	if (end == text || *end != '\0' || !isfinite(parsed)) return 0;
	*value = parsed; return 1;
}

static int parseNsFloatProp(mxml_node_t * node, const char* name, float32* value, float32 fallback)
{
	char text[64]; *value = fallback;
	if (!copyNsProp(node, name, text, sizeof(text))) return 1;
	char* end = NULL; float parsed = strtof(text, &end);
	if (end == text || *end != '\0' || !isfinite(parsed)) return 0;
	*value = parsed; return 1;
}

static int parseNsIntProp(mxml_node_t * node, const char* name, uint16* value, uint16 fallback)
{
	char text[32]; *value = fallback;
	if (!copyNsProp(node, name, text, sizeof(text))) return 1;
	char* end = NULL; long parsed = strtol(text, &end, 10);
	if (end == text || *end != '\0' || parsed < 0 || parsed > 128) return 0;
	*value = (uint16)parsed; return 1;
}

static uint8 parseBoolText(const char* text, uint8 fallback)
{
	if (text == NULL) return fallback;
	if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0) return 1;
	if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0) return 0;
	return fallback;
}

static void setFloatProp(mxml_node_t * node, const char* name, float32 value)
{
	mxmlElementSetAttrf(node, name, "%.9g", value);
}

static void setNsFloatProp(mxml_node_t * node, mxml_node_t * ns, const char* name, float32 value)
{
	(void)ns; char qualified[80]; snprintf(qualified, sizeof(qualified), "ymgre:%s", name);
	mxmlElementSetAttrf(node, qualified, "%.9g", value);
}

static void setNsIntProp(mxml_node_t * node, mxml_node_t * ns, const char* name, uint32 value)
{
	(void)ns; char qualified[80]; snprintf(qualified, sizeof(qualified), "ymgre:%s", name);
	mxmlElementSetAttrf(node, qualified, "%u", value);
}

static void colorText(GYcolor color, char output[8])
{
	snprintf(output, 8, "#%02X%02X%02X", (unsigned)((color >> 16) & 0xFF),
		(unsigned)((color >> 8) & 0xFF), (unsigned)(color & 0xFF));
}

static int parseColorText(const char* text, GYcolor* color)
{
	if (text == NULL || text[0] != '#' || strlen(text) != 7) return 0;
	char* end = NULL; unsigned long rgb = strtoul(text + 1, &end, 16);
	if (end != text + 7 || *end != '\0' || rgb > 0xFFFFFF) return 0;
	*color = GY_ARGB(0xFF, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); return 1;
}

static void addColor(mxml_node_t * parent, const char* name, GYcolor color)
{
	mxml_node_t * node = mxmlNewElement(parent, name);
	setFloatProp(node, "r", (float32)((color >> 16) & 0xFF) / 255.0f);
	setFloatProp(node, "g", (float32)((color >> 8) & 0xFF) / 255.0f);
	setFloatProp(node, "b", (float32)(color & 0xFF) / 255.0f);
}

static int readColor(mxml_node_t * node, GYcolor* color)
{
	float32 r, g, b;
	if (node == NULL || !parseFloatProp(node, "r", &r, 1) || !parseFloatProp(node, "g", &g, 1) ||
		!parseFloatProp(node, "b", &b, 1) || r < 0 || r > 1 || g < 0 || g > 1 || b < 0 || b > 1) return 0;
	*color = GY_ARGB(0xFF, (uint8)lroundf(r * 255), (uint8)lroundf(g * 255), (uint8)lroundf(b * 255)); return 1;
}

static const char* primitiveName(uint8 kind)
{
	static const char* names[] = { "plane", "cube", "box", "sphere", "cylinder", "cone", "torus", "capsule" };
	return kind <= SCENE_PLACE_CAPSULE ? names[kind] : NULL;
}

static int primitiveKind(const char* name, uint8* kind)
{
	for (uint8 i = 0; i <= SCENE_PLACE_CAPSULE; ++i)
		if (name != NULL && strcmp(name, primitiveName(i)) == 0) { *kind = i; return 1; }
	return 0;
}

static void sceneDirectory(const char* path, char* directory, size_t capacity)
{
	snprintf(directory, capacity, "%s", path); char* slash = strrchr(directory, '/');
	if (slash == directory) directory[1] = '\0';
	else if (slash != NULL) *slash = '\0';
	else snprintf(directory, capacity, ".");
}

static void relativeResourcePath(const char* scenePath, const char* resourcePath,
	char* output, size_t capacity)
{
	char directory[SCENE_EDITOR_PATH_CAPACITY], absoluteDirectory[SCENE_EDITOR_PATH_CAPACITY];
	sceneDirectory(scenePath, directory, sizeof(directory));
	if (realpath(directory, absoluteDirectory) == NULL || resourcePath[0] != '/') {
		snprintf(output, capacity, "%s", resourcePath); return;
	}
	size_t common = 0, lastSlash = 0;
	while (absoluteDirectory[common] != '\0' && resourcePath[common] != '\0' &&
		absoluteDirectory[common] == resourcePath[common]) {
		if (absoluteDirectory[common] == '/') lastSlash = common;
		common++;
	}
	if (absoluteDirectory[common] == '\0' && resourcePath[common] == '/') lastSlash = common;
	size_t used = 0;
	for (const char* cursor = absoluteDirectory + lastSlash + 1; *cursor != '\0'; ++cursor)
		if (*cursor == '/' && used + 3 < capacity) { memcpy(output + used, "../", 3); used += 3; }
	if (absoluteDirectory[lastSlash + 1] != '\0' && used + 3 < capacity) { memcpy(output + used, "../", 3); used += 3; }
	snprintf(output + used, capacity - used, "%s", resourcePath + lastSlash + 1);
}

static int resolveResourcePath(const char* scenePath, const char* resourcePath,
	char* output, size_t capacity)
{
	char candidate[SCENE_EDITOR_PATH_CAPACITY];
	if (resourcePath[0] == '/') snprintf(candidate, sizeof(candidate), "%s", resourcePath);
	else {
		char directory[SCENE_EDITOR_PATH_CAPACITY]; sceneDirectory(scenePath, directory, sizeof(directory));
		if (snprintf(candidate, sizeof(candidate), "%s/%s", directory, resourcePath) >= (int)sizeof(candidate)) return 0;
	}
	if (realpath(candidate, output) == NULL) { snprintf(output, capacity, "%s", candidate); return 0; }
	return 1;
}

static void addTransform(mxml_node_t * node, const SceneEditorObject* object)
{
	mxml_node_t * position = mxmlNewElement(node, "position");
	setFloatProp(position, "x", object->x); setFloatProp(position, "y", object->y); setFloatProp(position, "z", object->z);
	mxml_node_t * rotation = mxmlNewElement(node, "rotation");
	float32 half = object->rotY * (float32)M_PI / 360.0f;
	setFloatProp(rotation, "qx", 0); setFloatProp(rotation, "qy", sinf(half));
	setFloatProp(rotation, "qz", 0); setFloatProp(rotation, "qw", cosf(half));
	setNsFloatProp(rotation, node, "rotX", object->rotX); setNsFloatProp(rotation, node, "rotZ", object->rotZ);
	mxml_node_t * scale = mxmlNewElement(node, "scale");
	float32 value = object->kind == SCENE_OBJECT_MESH ? object->scale : 1.0f;
	setFloatProp(scale, "x", value); setFloatProp(scale, "y", value); setFloatProp(scale, "z", value);
}

static void addEditorProperties(mxml_node_t * node, const SceneEditorObject* object)
{
	mxml_node_t * data = mxmlNewElement(node, "userData");
	const char* names[] = { "fixed", "visible", "wireframe" };
	uint8 values[] = { object->fixed, object->visible, object->wireframe };
	for (uint8 i = 0; i < 3; ++i) {
		mxml_node_t * property = mxmlNewElement(data, "property");
		mxmlElementSetAttr(property, "name", names[i]); mxmlElementSetAttr(property, "type", "bool");
		mxmlElementSetAttr(property, "data", values[i] ? "true" : "false");
	}
}

int SceneEditorIo_Write(const char* path, const SceneEditorObject* objects, uint32 count,
	int32 mainCameraIndex, int32 activeCameraIndex, char* error, size_t errorCapacity)
{
	if (path == NULL || path[0] == '\0' || objects == NULL || count == 0 || count > 32 ||
		mainCameraIndex < 0 || activeCameraIndex < 0 || (uint32)mainCameraIndex >= count ||
		(uint32)activeCameraIndex >= count) {
		snprintf(error, errorCapacity, "场景保存参数无效"); return 0;
	}
	mxml_node_t * document = mxmlNewXML("1.0");
	mxml_node_t * root = mxmlNewElement(document, "scene"); mxml_node_t * ns = root;
	mxmlElementSetAttr(root, "xmlns:ymgre", YMGRE_SCENE_NS);
	mxmlElementSetAttr(root, "formatVersion", "1.0");
	mxmlElementSetAttr(root, "generator", "YMGRE Scene Editor");
	mxmlElementSetAttr(root, "ymgre:formatVersion", YMGRE_SCENE_VERSION);
	char reference[32]; snprintf(reference, sizeof(reference), "ymgre-node-%d", mainCameraIndex);
	mxmlElementSetAttr(root, "ymgre:mainCamera", reference);
	snprintf(reference, sizeof(reference), "ymgre-node-%d", activeCameraIndex);
	mxmlElementSetAttr(root, "ymgre:activeCamera", reference);
	mxml_node_t * environment = mxmlNewElement(root, "environment");
	for (uint32 i = 0; i < count; ++i) if (objects[i].kind == SCENE_OBJECT_LIGHT && objects[i].lightType == GRE_GlobalLight) {
		addColor(environment, "colourAmbient", objects[i].color);
		mxmlElementSetAttr(environment, "ymgre:globalLightName", objects[i].name);
		setNsFloatProp(environment, ns, "ambientStrength", objects[i].strength); break;
	}
	mxml_node_t * nodes = mxmlNewElement(root, "nodes");
	for (uint32 i = 0; i < count; ++i) {
		const SceneEditorObject* object = &objects[i];
		if (object->kind == SCENE_OBJECT_LIGHT && object->lightType == GRE_GlobalLight) continue;
		mxml_node_t * node = mxmlNewElement(nodes, "node");
		mxmlElementSetAttr(node, "name", object->name);
		char id[32]; snprintf(id, sizeof(id), "ymgre-node-%u", i); mxmlElementSetAttr(node, "id", id);
		addTransform(node, object); addEditorProperties(node, object);
		if (object->kind == SCENE_OBJECT_MESH) {
			mxml_node_t * entity = mxmlNewElement(node, "entity");
			mxmlElementSetAttr(entity, "name", object->name);
			if (object->primitiveKind == 0xFF) {
				char resource[SCENE_EDITOR_PATH_CAPACITY]; relativeResourcePath(path, object->sourcePath, resource, sizeof(resource));
				mxmlElementSetAttr(entity, "meshFile", resource);
			} else mxmlElementSetAttr(entity, "ymgre:primitive", primitiveName(object->primitiveKind));
			setNsIntProp(entity, ns, "detailA", object->detailA); setNsIntProp(entity, ns, "detailB", object->detailB);
			char color[8]; colorText(object->color, color); mxmlElementSetAttr(entity, "ymgre:color", color);
		} else if (object->kind == SCENE_OBJECT_LIGHT) {
			mxml_node_t * light = mxmlNewElement(node, "light");
			mxmlElementSetAttr(light, "name", object->name);
			mxmlElementSetAttr(light, "type", object->lightType == GRE_SpotLight ? "spot" : "point");
			mxmlElementSetAttr(light, "castShadows", object->shadowsEnabled ? "true" : "false");
			setFloatProp(light, "powerScale", object->strength); addColor(light, "colourDiffuse", object->color);
			mxml_node_t * target = mxmlNewElement(light, "ymgre:target");
			setFloatProp(target, "x", object->targetX); setFloatProp(target, "y", object->targetY); setFloatProp(target, "z", object->targetZ);
		} else {
			mxml_node_t * camera = mxmlNewElement(node, "camera");
			mxmlElementSetAttr(camera, "name", object->name);
			mxmlElementSetAttr(camera, "projectionType", "perspective"); setFloatProp(camera, "fov", 45);
			mxml_node_t * clipping = mxmlNewElement(camera, "clipping");
			setFloatProp(clipping, "near", 0.01f); setFloatProp(clipping, "far", 1000);
			mxml_node_t * target = mxmlNewElement(camera, "ymgre:target");
			setFloatProp(target, "x", object->targetX); setFloatProp(target, "y", object->targetY); setFloatProp(target, "z", object->targetZ);
		}
	}
	char temporaryPath[SCENE_EDITOR_PATH_CAPACITY + 32];
	if (snprintf(temporaryPath, sizeof(temporaryPath), "%s.tmp.%ld", path, (long)getpid()) >= (int)sizeof(temporaryPath)) {
		mxmlDelete(document); snprintf(error, errorCapacity, "场景文件路径过长"); return 0;
	}
	mxml_options_t* options = mxmlOptionsNew();
	mxmlOptionsSetWhitespaceCallback(options, sceneWhitespace, NULL);
	mxmlOptionsSetWrapMargin(options, 0);
	bool saved = mxmlSaveFilename(document, options, temporaryPath);
	mxmlOptionsDelete(options); mxmlDelete(document);
	if (!saved || rename(temporaryPath, path) != 0) {
		snprintf(error, errorCapacity, "写入场景文件失败：%s", strerror(errno)); unlink(temporaryPath); return 0;
	}
	return 1;
}

static int readTransform(mxml_node_t * node, SceneEditorObject* object)
{
	mxml_node_t* position = child(node, "position");
	mxml_node_t* scale = child(node, "scale");
	mxml_node_t* rotation = child(node, "rotation");
	if (!parseFloatProp(position, "x", &object->x, 0) || !parseFloatProp(position, "y", &object->y, 0) ||
		!parseFloatProp(position, "z", &object->z, 0) || !parseFloatProp(scale, "x", &object->scale, 1)) return 0;
	float32 qy, qw;
	if (!parseFloatProp(rotation, "qy", &qy, 0) || !parseFloatProp(rotation, "qw", &qw, 1)) return 0;
	object->rotY = 2.0f * atan2f(qy, qw) * 180.0f / (float32)M_PI;
	parseNsFloatProp(rotation, "rotX", &object->rotX, 0);parseNsFloatProp(rotation, "rotZ", &object->rotZ, 0);
	return 1;
}

static void readEditorProperties(mxml_node_t * node, SceneEditorObject* object)
{
	mxml_node_t * data = child(node, "userData");
	for (mxml_node_t * property = data != NULL ? mxmlGetFirstChild(data) : NULL; property != NULL;
		property = mxmlGetNextSibling(property)) {
		if (mxmlGetType(property) != MXML_TYPE_ELEMENT || strcmp(localName(property), "property") != 0) continue;
		char name[32], value[32]; if (!copyProp(property, "name", name, sizeof(name)) || !copyProp(property, "data", value, sizeof(value))) continue;
		if (strcmp(name, "fixed") == 0) object->fixed = parseBoolText(value, object->fixed);
		else if (strcmp(name, "visible") == 0) object->visible = parseBoolText(value, object->visible);
		else if (strcmp(name, "wireframe") == 0) object->wireframe = parseBoolText(value, object->wireframe);
	}
}

static int readTarget(mxml_node_t * owner, SceneEditorObject* object)
{
	mxml_node_t * target = child(owner, "target");
	return parseFloatProp(target, "x", &object->targetX, 0) && parseFloatProp(target, "y", &object->targetY, 0) &&
		parseFloatProp(target, "z", &object->targetZ, 0);
}

int SceneEditorIo_Read(const char* path, SceneEditorObject* objects, uint32 capacity,
	uint32* count, int32* mainCameraIndex, int32* activeCameraIndex,
	char* error, size_t errorCapacity)
{
	if (path == NULL || path[0] == '\0' || objects == NULL || count == NULL || mainCameraIndex == NULL ||
		activeCameraIndex == NULL || capacity == 0) {
		snprintf(error, errorCapacity, "场景打开参数无效"); return 0;
	}
	mxml_options_t* options = mxmlOptionsNew();
	mxmlOptionsSetTypeValue(options, MXML_TYPE_OPAQUE);
	mxml_node_t * document = mxmlLoadFilename(NULL, options, path); mxmlOptionsDelete(options);
	if (document == NULL) { snprintf(error, errorCapacity, "无法解析 .scene XML 文件"); return 0; }
	mxml_node_t * root = mxmlFindElement(document, document, "scene", NULL, NULL, MXML_DESCEND_ALL); char version[16];
	if (root == NULL ||
		!copyNsProp(root, "formatVersion", version, sizeof(version)) || strcmp(version, YMGRE_SCENE_VERSION) != 0) goto invalid_file;
	memset(objects, 0, sizeof(*objects) * capacity); *count = 0; *mainCameraIndex = *activeCameraIndex = -1;
	mxml_node_t* environment = child(root, "environment");
	mxml_node_t* ambient = child(environment, "colourAmbient");
	if (environment == NULL || ambient == NULL || *count >= capacity) goto invalid_file;
	SceneEditorObject* global = &objects[(*count)++]; global->active = 1; global->kind = SCENE_OBJECT_LIGHT;
	global->lightType = GRE_GlobalLight; global->visible = 1; snprintf(global->type, sizeof(global->type), "全局光照");
	if (!copyNsProp(environment, "globalLightName", global->name, sizeof(global->name))) snprintf(global->name, sizeof(global->name), "Global Light");
	if (!parseNsFloatProp(environment, "ambientStrength", &global->strength, 1) || !readColor(ambient, &global->color)) goto invalid_file;
	char ids[32][64] = {{0}}; snprintf(ids[0], sizeof(ids[0]), "ymgre-global-light");
	mxml_node_t * nodes = child(root, "nodes");
	for (mxml_node_t * node = nodes != NULL ? mxmlGetFirstChild(nodes) : NULL; node != NULL;
		node = mxmlGetNextSibling(node)) {
		if (mxmlGetType(node) != MXML_TYPE_ELEMENT || strcmp(localName(node), "node") != 0) continue;
		mxml_node_t* entity = child(node, "entity");
		mxml_node_t* light = child(node, "light");
		mxml_node_t* camera = child(node, "camera");
		if (entity == NULL && light == NULL && camera == NULL) continue;
		if (*count >= capacity || (entity != NULL) + (light != NULL) + (camera != NULL) != 1) goto invalid_file;
		SceneEditorObject* object = &objects[*count]; object->active = 1; object->visible = 1;
		if (!copyProp(node, "name", object->name, sizeof(object->name)) || !copyProp(node, "id", ids[*count], sizeof(ids[*count])) ||
			!readTransform(node, object)) goto invalid_file;
		readEditorProperties(node, object);
			if (entity != NULL) {
			object->kind = SCENE_OBJECT_MESH; char meshFile[SCENE_EDITOR_PATH_CAPACITY], primitive[32], color[16];
				if (copyProp(entity, "meshFile", meshFile, sizeof(meshFile))) {
					object->primitiveKind = 0xFF;
					resolveResourcePath(path, meshFile, object->sourcePath, sizeof(object->sourcePath));
				snprintf(object->type, sizeof(object->type), "外部网格");
			} else if (copyNsProp(entity, "primitive", primitive, sizeof(primitive)) && primitiveKind(primitive, &object->primitiveKind))
				snprintf(object->type, sizeof(object->type), "%.23s", primitive);
			else goto invalid_file;
			if (!parseNsIntProp(entity, "detailA", &object->detailA, 20) || !parseNsIntProp(entity, "detailB", &object->detailB, 20)) goto invalid_file;
			object->color = GY_ARGB(0xFF,128,128,128);
			if (copyNsProp(entity, "color", color, sizeof(color)) && !parseColorText(color, &object->color)) goto invalid_file;
		} else if (light != NULL) {
			object->kind = SCENE_OBJECT_LIGHT; char type[24], shadows[16];
			if (!copyProp(light, "type", type, sizeof(type))) goto invalid_file;
			object->lightType = strcmp(type, "spot") == 0 ? GRE_SpotLight : (strcmp(type, "point") == 0 ? GRE_PointLight : GRE_GlobalLight);
			if (object->lightType == GRE_GlobalLight) goto invalid_file;
			snprintf(object->type, sizeof(object->type), "%s", object->lightType == GRE_SpotLight ? "聚光灯" : "点光源");
			if (!parseFloatProp(light, "powerScale", &object->strength, 1) || !readColor(child(light, "colourDiffuse"), &object->color) ||
				!readTarget(light, object)) goto invalid_file;
			object->shadowsEnabled = copyProp(light, "castShadows", shadows, sizeof(shadows)) ? parseBoolText(shadows, 0) : 0;
		} else {
			object->kind = SCENE_OBJECT_CAMERA; snprintf(object->type, sizeof(object->type), "相机");
			if (!readTarget(camera, object)) goto invalid_file;
		}
		(*count)++;
	}
	char mainReference[64], activeReference[64];
	if (!copyNsProp(root, "mainCamera", mainReference, sizeof(mainReference)) ||
		!copyNsProp(root, "activeCamera", activeReference, sizeof(activeReference))) goto invalid_file;
	for (uint32 i = 0; i < *count; ++i) {
		if (strcmp(ids[i], mainReference) == 0) *mainCameraIndex = (int32)i;
		if (strcmp(ids[i], activeReference) == 0) *activeCameraIndex = (int32)i;
	}
	if (*mainCameraIndex < 0 || *activeCameraIndex < 0 || objects[*mainCameraIndex].kind != SCENE_OBJECT_CAMERA ||
		objects[*activeCameraIndex].kind != SCENE_OBJECT_CAMERA) goto invalid_file;
	mxmlDelete(document); return 1;
invalid_file:
	mxmlDelete(document); snprintf(error, errorCapacity, "`.scene` 文件不属于受支持的 YMGRE DotScene 子集"); return 0;
}

static uint8 allowOverwrite(GYOBJ dialog, const char* path, void* user)
{
	(void)dialog; (void)path; (void)user; return 1;
}

static uint8 hasSceneExtension(const char* path)
{
	size_t length = strlen(path);
	return length >= 6 && strcmp(path + length - 6, ".scene") == 0;
}

static uint8 visibleSceneEntry(GYOBJ dialog, GYfiledialog_mode mode,
	const char* parentPath, const GYfiledialog_entry* entry, void* user)
{
	(void)dialog; (void)mode; (void)parentPath; (void)user;
	return entry->is_dir || hasSceneExtension(entry->name);
}

static void dialogResult(GYOBJ dialog, uint8 accepted, const char* path, void* user)
{
	(void)user;
	if (!accepted || path == NULL) {
		if (g_io.cancelCb != NULL) g_io.cancelCb(g_io.userData);
		return;
	}
	char normalized[SCENE_EDITOR_PATH_CAPACITY];
	if (YMGUI_FileDialog_GetMode(dialog) == GY_FILE_DIALOG_SAVE_FILE && !hasSceneExtension(path)) {
		if (snprintf(normalized, sizeof(normalized), "%s.scene", path) >= (int)sizeof(normalized)) {
			SceneEditorIo_SetError("场景文件路径过长"); return;
		}
		path = normalized;
	}
	snprintf(g_io.lastPath, sizeof(g_io.lastPath), "%s", path);
	SceneEditorIoPathCb callback = YMGUI_FileDialog_GetMode(dialog) == GY_FILE_DIALOG_SAVE_FILE ?
		g_io.saveCb : g_io.openCb;
	if (callback != NULL) callback(path, g_io.userData);
}

void SceneEditorIo_Build(GYCTX context, SceneEditorIoPathCb saveCb,
	SceneEditorIoPathCb openCb, SceneEditorIoCancelCb cancelCb, void* userData)
{
	memset(&g_io, 0, sizeof(g_io)); g_io.saveCb = saveCb; g_io.openCb = openCb;
	g_io.cancelCb = cancelCb; g_io.userData = userData;
	g_io.dialog = YMGUI_Creat_FileDialog_Creat(context, 760, 600,
		SCENE_EDITOR_PATH_CAPACITY - 1, 255, 256);
	if (g_io.dialog == NULL) return;
	YMGUI_FileDialog_SetFS(g_io.dialog, SceneEditorFileDialog_PosixFS(), NULL);
	YMGUI_FileDialog_SetResultCb(g_io.dialog, dialogResult, NULL);
	YMGUI_FileDialog_SetOverwriteCb(g_io.dialog, allowOverwrite);
	YMGUI_FileDialog_SetFilterCb(g_io.dialog, visibleSceneEntry, NULL);
	GYcoord cardX=(context->top_layer->area.w-760)/2;
	GYcoord cardY=(context->top_layer->area.h-600)/2;
	g_io.ignoreErrors=YMGUI_Creat_Checkbox_Creat(g_io.dialog,cardX+16,cardY+570,300,28);
	if(g_io.ignoreErrors!=NULL)YMGUI_Checkbox_SetText(g_io.ignoreErrors,"强制忽略资源错误");
}

static void openDialog(uint8 saving)
{
	if (g_io.dialog == NULL) return;
	if(g_io.ignoreErrors!=NULL){
		YMGUI_Obj_SetHidden(g_io.ignoreErrors,saving);
		if(!saving)YMGUI_Checkbox_SetChecked(g_io.ignoreErrors,0);
	}
	char fullPath[SCENE_EDITOR_PATH_CAPACITY];
	const char* configured = getenv("YMGRE_SCENE_PATH");
	if (g_io.lastPath[0] != '\0') snprintf(fullPath, sizeof(fullPath), "%s", g_io.lastPath);
	else if (configured != NULL && configured[0] != '\0') snprintf(fullPath, sizeof(fullPath), "%s", configured);
	else if (getcwd(fullPath, sizeof(fullPath)) != NULL) {
		size_t length = strlen(fullPath); snprintf(fullPath + length, sizeof(fullPath) - length,
			"%sscene.scene", length > 0 && fullPath[length - 1] == '/' ? "" : "/");
	} else snprintf(fullPath, sizeof(fullPath), "./scene.scene");
	char directory[SCENE_EDITOR_PATH_CAPACITY]; sceneDirectory(fullPath, directory, sizeof(directory));
	const char* slash = strrchr(fullPath, '/'); const char* name = slash != NULL ? slash + 1 : fullPath;
	if (!YMGUI_FileDialog_Show(g_io.dialog,
		saving ? GY_FILE_DIALOG_SAVE_FILE : GY_FILE_DIALOG_OPEN_FILE, directory, name)) {
		char cwd[SCENE_EDITOR_PATH_CAPACITY];
		if (getcwd(cwd, sizeof(cwd)) != NULL)
			YMGUI_FileDialog_Show(g_io.dialog,
				saving ? GY_FILE_DIALOG_SAVE_FILE : GY_FILE_DIALOG_OPEN_FILE,
				cwd, saving ? "scene.scene" : "");
	}
}

void SceneEditorIo_OpenSave(void) { openDialog(1); }
void SceneEditorIo_OpenLoad(void) { openDialog(0); }
void SceneEditorIo_SetError(const char* message)
{
	if (message != NULL && message[0] != '\0') fprintf(stderr, "scene editor: %s\n", message);
}
void SceneEditorIo_Close(void) { if (g_io.dialog != NULL) YMGUI_FileDialog_Close(g_io.dialog); }
void SceneEditorIo_ResetPath(void) { g_io.lastPath[0] = '\0'; }
uint8 SceneEditorIo_GetIgnoreLoadErrors(void)
{
	return g_io.ignoreErrors!=NULL?YMGUI_Checkbox_GetChecked(g_io.ignoreErrors):0;
}
void SceneEditorIo_SetIgnoreLoadErrors(uint8 ignore)
{
	if(g_io.ignoreErrors!=NULL)YMGUI_Checkbox_SetChecked(g_io.ignoreErrors,ignore?1:0);
}
