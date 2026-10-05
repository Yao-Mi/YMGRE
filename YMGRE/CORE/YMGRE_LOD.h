#ifndef YMGRE_LOD_H
#define YMGRE_LOD_H

#include "../OPOBJ/YMGRE_OBJ.h"

/* Assets stay with the application. A level may refer to a mesh, a prepared
 * image (including a multi-view impostor), or a resource built in code. */
#define YMGRE_LOD_MAX_LEVELS 8
#define YMGRE_LOD_ASSET_MAX 128

typedef enum { YMGRE_LOD_MESH = 1, YMGRE_LOD_IMAGE = 2 } YMGRE_LOD_Kind;
typedef struct {
    YMGRE_LOD_Kind kind;
    float32 min_pixels; /* projected diameter required for this level */
    char asset[YMGRE_LOD_ASSET_MAX];
    void *resource;     /* application-owned; may be bound after parsing */
} YMGRE_LOD_Level;
typedef struct {
    YMGRE_LOD_Level levels[YMGRE_LOD_MAX_LEVELS];
    uint8 count;
    float32 hysteresis; /* fraction of each transition threshold, [0,1) */
} YMGRE_LOD_Object;
typedef struct {
    uint8 level;
    uint8 initialized;
} YMGRE_LOD_Instance;

void YMGRE_LOD_Init(YMGRE_LOD_Object *lod, float32 hysteresis);
/* Levels are added from finest to coarsest. The last threshold must be zero.
 * Code-created resources may pass NULL for asset when resource is non-NULL. */
int YMGRE_LOD_Add(YMGRE_LOD_Object *lod, YMGRE_LOD_Kind kind,
                  float32 min_pixels, const char *asset, void *resource);
/* Text format: YMGRE_LOD 1, hysteresis <fraction>, level <mesh|image>
 * <min_pixels> <asset>. Lines starting with # are comments. Parsing is
 * transactional; file access and resource loading belong to the caller. */
int YMGRE_LOD_Parse(YMGRE_LOD_Object *lod, const char *text);
typedef void *(*YMGRE_LOD_ResolveAsset)(YMGRE_LOD_Kind kind, const char *asset, void *context);
/* Resolve all unbound names once during resource preparation. On failure,
 * leave the object untouched so callers can retry after loading assets. */
int YMGRE_LOD_Resolve(YMGRE_LOD_Object *lod, YMGRE_LOD_ResolveAsset resolve, void *context);
float32 YMGRE_LOD_ProjectedDiameter(GRE_Camera4d camera,
                                    gre_fvector4d center, float32 radius);
const YMGRE_LOD_Level *YMGRE_LOD_Select(const YMGRE_LOD_Object *lod,
                                       YMGRE_LOD_Instance *instance,
                                       float32 projected_pixels);
const YMGRE_LOD_Level *YMGRE_LOD_SelectCamera(const YMGRE_LOD_Object *lod,
                                             YMGRE_LOD_Instance *instance,
                                             GRE_Camera4d camera,
                                             gre_fvector4d center, float32 radius);

#endif
