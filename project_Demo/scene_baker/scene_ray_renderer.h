#ifndef SCENE_RAY_RENDERER_H
#define SCENE_RAY_RENDERER_H
#include "YMGRE_RayTracing.h"
typedef struct SceneRayRenderer SceneRayRenderer;
typedef struct {
    GRE_Object4d object;
    uint8 type; /* 1 sphere, 2 capped cylinder */
    gre_fvector4d center, axis[3]; /* orthonormal local-to-world basis */
    float radius, halfHeight;
} SceneRayPrimitive;
void SceneRay_SetPrimitives(SceneRayRenderer* renderer,const SceneRayPrimitive* primitives,int count);
/* Freeze the selected mesh's current-view reflection/refraction into its triangle atlas. */
int SceneRay_BakeSurface(SceneRayRenderer* renderer,GRE_Object4d object,GRE_Lightmap map,
    GRE_Camera4d camera,GRE_List objects,GRE_List lights,GRE_List materials);
/* Progressive CPU primary/shadow rays. Output depth is camera-space Z, as in rasterization.
   Owns only acceleration/frame buffers; scene resources remain owned by the editor. */
SceneRayRenderer* SceneRay_Create(void);
void SceneRay_Destroy(SceneRayRenderer* renderer);
void SceneRay_Invalidate(SceneRayRenderer* renderer);
/* Returns -1 on allocation failure, otherwise completion percentage (0..100). */
int SceneRay_Render(SceneRayRenderer* renderer, GRE_Camera4d camera,
    GRE_List objects, GRE_List lights, GRE_List materials);
#endif
