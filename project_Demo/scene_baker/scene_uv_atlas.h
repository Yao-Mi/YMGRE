#ifndef SCENE_UV_ATLAS_H
#define SCENE_UV_ATLAS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const float* positions;
    const unsigned* indices;
    unsigned vertexCount, indexCount;
    float* cornerUvs;
    int* cornerCharts;
} SceneUvAtlasMesh;
int SceneUv_AtlasUnwrap(SceneUvAtlasMesh* meshes,unsigned count,int preserveAspect,int* charts,char* error,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
