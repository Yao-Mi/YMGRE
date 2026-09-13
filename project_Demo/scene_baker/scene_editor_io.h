#ifndef SCENE_EDITOR_IO_H
#define SCENE_EDITOR_IO_H

#include <stddef.h>
#include "scene_editor_model.h"

typedef void (*SceneEditorIoPathCb)(const char* path, void* userData);
typedef void (*SceneEditorIoCancelCb)(void* userData);

void SceneEditorIo_Build(GYCTX context, SceneEditorIoPathCb saveCb,
	SceneEditorIoPathCb openCb, SceneEditorIoCancelCb cancelCb, void* userData);
void SceneEditorIo_OpenSave(void);
void SceneEditorIo_OpenLoad(void);
void SceneEditorIo_SetError(const char* message);
void SceneEditorIo_Close(void);
void SceneEditorIo_ResetPath(void);
uint8 SceneEditorIo_GetIgnoreLoadErrors(void);
void SceneEditorIo_SetIgnoreLoadErrors(uint8 ignore);

int SceneEditorIo_Write(const char* path, const SceneEditorObject* objects, uint32 count,
	int32 mainCameraIndex, int32 activeCameraIndex, char* error, size_t errorCapacity);
/* On success the caller owns objects[i].pendingUv; apply/free it before installing a live object. */
int SceneEditorIo_Read(const char* path, SceneEditorObject* objects, uint32 capacity,
	uint32* count, int32* mainCameraIndex, int32* activeCameraIndex,
	char* error, size_t errorCapacity);

#endif
