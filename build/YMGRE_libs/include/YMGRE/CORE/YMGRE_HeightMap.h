#ifndef YMGRE_HEIGHT_MAP_H
#define YMGRE_HEIGHT_MAP_H

#include "../OPOBJ/YMGRE_OBJ.h"

// Samples a temporary RGB height image through vertex UVs and bakes the
// displacement into object geometry. Positive heightScale raises bright areas;
// heightBias is the normalized zero level.
int YMGRE_Object_ApplyHeightMap(GRE_Object4d object, const GRErgb24* heightMap,
	uint16 width, uint16 height, float32 heightScale, float32 heightBias);

#endif
