#ifndef NATURE_GRASS_IMPOSTOR_H
#define NATURE_GRASS_IMPOSTOR_H
#include "YMGRE_Rendering_Pipeline.h"
#define GRASS_VIEWS 8
#define GRASS_TILE 64
typedef struct {
    GRErgb24 color[GRASS_TILE*GRASS_TILE];
    unsigned char mask[GRASS_TILE*GRASS_TILE];
    int left,top,right,bottom;
} grass_view;
typedef struct {
    grass_view views[GRASS_VIEWS];
    gre_fvector4d center;
    float span;
} grass_impostor;
int grass_impostor_bake(grass_impostor *out,GRE_Object4d source,GRE_List materials,GRE_List lights);
void grass_impostor_draw(const grass_impostor *asset,GRE_Camera4d camera,float x,float y,float z,float scale,float angle);
#endif
