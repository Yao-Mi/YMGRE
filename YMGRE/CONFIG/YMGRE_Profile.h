#ifndef YMGRE_PROFILE_H
#define YMGRE_PROFILE_H
#include "YMGRE_PubType.h"
#ifndef YMGRE_PROFILE_RENDER_STAGES
#define YMGRE_PROFILE_RENDER_STAGES 0
#endif
/* Optional serial diagnostic. Slots: clear, transform, cull, lighting,
 * reserved, vertex raster, advanced batch, reserved. Nested slots do not sum.
 * Use a caller clock (cycles or ticks); ARM M-profile defaults to DWT.
 * Enabling DWT and avoiding counter rollover are the board port's responsibility.
 * Configure/reset/read only while rendering is idle; no concurrent writers. */
extern uint32 YMGRE_ProfileCycles[8];
typedef uint32 (*GRE_ProfileClock)(void* user);
void YMGRE_Profile_SetClock(GRE_ProfileClock clock,void* user);
void YMGRE_Profile_Reset(void);
uint32 YMGRE_ProfileNow(void);
#endif
