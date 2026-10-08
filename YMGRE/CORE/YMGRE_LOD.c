#include "YMGRE_LOD.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void YMGRE_LOD_Init(YMGRE_LOD_Object *lod, float32 hysteresis)
{
    if (!lod) return;
    memset(lod, 0, sizeof(*lod));
    lod->hysteresis = isfinite(hysteresis) && hysteresis >= 0 && hysteresis < 1 ? hysteresis : 0;
}

int YMGRE_LOD_Add(YMGRE_LOD_Object *lod, YMGRE_LOD_Kind kind,
                  float32 min_pixels, const char *asset, void *resource)
{
    if (!lod || lod->count >= YMGRE_LOD_MAX_LEVELS ||
        (kind != YMGRE_LOD_MESH && kind != YMGRE_LOD_IMAGE) ||
        !isfinite(min_pixels) || min_pixels < 0 || (!asset && !resource) ||
        (asset && strlen(asset) >= YMGRE_LOD_ASSET_MAX) ||
        (lod->count && min_pixels >= lod->levels[lod->count-1].min_pixels)) return 0;
    YMGRE_LOD_Level *level = &lod->levels[lod->count++];
    level->kind = kind;
    level->min_pixels = min_pixels;
    strcpy(level->asset, asset ? asset : "");
    level->resource = resource;
    return 1;
}

int YMGRE_LOD_Parse(YMGRE_LOD_Object *lod, const char *text)
{
    if (!lod || !text) return 0;
    YMGRE_LOD_Object parsed;
    YMGRE_LOD_Init(&parsed, 0);
    int version = 0, seen_hysteresis = 0;
    const char *line = text;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end-line) : strlen(line);
        if (len >= 256) return 0;
        char row[256], extra;
        memcpy(row, line, len); row[len] = 0;
        char *p = row;
        while (isspace((unsigned char)*p)) ++p;
        if (*p && *p != '#') {
            if (!version) {
                if (sscanf(p, "YMGRE_LOD %d %c", &version, &extra) != 1 || version != 1) return 0;
            } else if (!strncmp(p, "hysteresis", 10) && isspace((unsigned char)p[10])) {
                float h;
                if (seen_hysteresis || parsed.count || sscanf(p, "hysteresis %f %c", &h, &extra) != 1 ||
                    !isfinite(h) || h < 0 || h >= 1) return 0;
                parsed.hysteresis = h; seen_hysteresis = 1;
            } else {
                char kind[16], asset[YMGRE_LOD_ASSET_MAX];
                float threshold;
                if (sscanf(p, "level %15s %f %127s %c", kind, &threshold, asset, &extra) != 3 ||
                    !YMGRE_LOD_Add(&parsed, !strcmp(kind, "mesh") ? YMGRE_LOD_MESH :
                                  !strcmp(kind, "image") ? YMGRE_LOD_IMAGE : (YMGRE_LOD_Kind)0,
                                  threshold, asset, NULL)) return 0;
            }
        }
        line = end ? end+1 : line+len;
    }
    if (!version || !parsed.count || parsed.levels[parsed.count-1].min_pixels != 0) return 0;
    *lod = parsed;
    return 1;
}

int YMGRE_LOD_Resolve(YMGRE_LOD_Object *lod, YMGRE_LOD_ResolveAsset resolve, void *context)
{
    if (!lod || !resolve || !lod->count || lod->count > YMGRE_LOD_MAX_LEVELS) return 0;
    void *resources[YMGRE_LOD_MAX_LEVELS];
    for (uint8 i=0; i<lod->count; ++i) {
        resources[i]=lod->levels[i].resource;
        if (!resources[i]) resources[i]=resolve(lod->levels[i].kind,lod->levels[i].asset,context);
        if (!resources[i]) return 0;
    }
    for (uint8 i=0; i<lod->count; ++i) lod->levels[i].resource=resources[i];
    return 1;
}

float32 YMGRE_LOD_ProjectedDiameter(GRE_Camera4d camera,
                                    gre_fvector4d center, float32 radius)
{
    if (!camera || !isfinite(radius) || radius <= 0 || !camera->img.height) return 0;
    float32 dx = center.x-camera->pos.x, dy = center.y-camera->pos.y, dz = center.z-camera->pos.z;
    float32 depth = dx*camera->move.cn.x + dy*camera->move.cn.y + dz*camera->move.cn.z;
    float32 plane_height = camera->perspectPlane.pU-camera->perspectPlane.pD;
    if (depth+radius <= 0 || plane_height <= 0) return 0;
    /* A sphere touching the near plane occupies at least the whole view. */
    if (depth <= radius) return (float32)camera->img.height * 2;
    return 2*radius*camera->perspectPlane.Dis*camera->img.height/(depth*plane_height);
}

const YMGRE_LOD_Level *YMGRE_LOD_Select(const YMGRE_LOD_Object *lod,
                                       YMGRE_LOD_Instance *instance,
                                       float32 projected_pixels)
{
    if (!lod || !instance || !lod->count || lod->count > YMGRE_LOD_MAX_LEVELS ||
        lod->levels[lod->count-1].min_pixels != 0) return NULL;
    if (!isfinite(projected_pixels) || projected_pixels < 0) projected_pixels = 0;
    if (!instance->initialized || instance->level >= lod->count) {
        instance->level = lod->count-1;
        for (uint8 i=0; i<lod->count; ++i)
            if (projected_pixels >= lod->levels[i].min_pixels) { instance->level=i; break; }
        instance->initialized = 1;
    } else {
        while (instance->level+1 < lod->count &&
               projected_pixels < lod->levels[instance->level].min_pixels*(1-lod->hysteresis))
            ++instance->level;
        while (instance->level > 0 &&
               projected_pixels >= lod->levels[instance->level-1].min_pixels*(1+lod->hysteresis))
            --instance->level;
    }
    return &lod->levels[instance->level];
}

const YMGRE_LOD_Level *YMGRE_LOD_SelectCamera(const YMGRE_LOD_Object *lod,
                                             YMGRE_LOD_Instance *instance,
                                             GRE_Camera4d camera,
                                             gre_fvector4d center, float32 radius)
{
    return YMGRE_LOD_Select(lod, instance, YMGRE_LOD_ProjectedDiameter(camera, center, radius));
}
