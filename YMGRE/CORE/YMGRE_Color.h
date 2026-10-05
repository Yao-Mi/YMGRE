#ifndef YMGRE_COLOR_H
#define YMGRE_COLOR_H
#include "../OPOBJ/YMGRE_OBJ.h"
#if YMGRE_ENABLE_LINEAR_COLOR
/* Borrowed immutable 256-entry table for bulk sampling. */
const float32* YMGRE_Color_DecodeTable(void);
float32 YMGRE_Color_Decode(uint8 value);
uint8 YMGRE_Color_Output(float32 linear);
int YMGRE_Color_BeginFrame(GRE_Camera4d cam,GRE_RenderWorkspace workspace,GRErgb24 background);
void YMGRE_Color_EndFrame(GRE_Camera4d cam);
#endif
#endif
