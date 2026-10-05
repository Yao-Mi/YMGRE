#ifndef YMGRE_LOD_SIMPLIFY_H
#define YMGRE_LOD_SIMPLIFY_H

#include "../OPOBJ/YMGRE_OBJ.h"

typedef struct {
    float32 target_ratio;       /* 0 < ratio <= 1, applied per material submesh */
    float32 max_relative_error; /* QEM edge-collapse limit relative to box diagonal; 0 = no limit */
    uint8 prune_components;     /* only tiny (<=16-face) pieces; not error-bounded */
} YMGRE_LOD_SimplifyOptions;
typedef struct {
    uint32 input_triangles;
    uint32 output_triangles;
    uint32 collapsed_triangles;
    uint32 pruned_triangles;
} YMGRE_LOD_SimplifyResult;

/* One-time resource preparation. Returns an independently owned object chain;
 * free it with YMGRE_Free_Object. Source and its materials are never changed.
 * Handles triangle meshes and per-vertex UVs/normals/colors. Per-face baked
 * lightmaps are rejected because collapsing topology invalidates their atlas. */
GRE_Object4d YMGRE_LOD_SimplifyMesh(GRE_Object4d source,
                                    YMGRE_LOD_SimplifyOptions options,
                                    YMGRE_LOD_SimplifyResult *result);

#endif
