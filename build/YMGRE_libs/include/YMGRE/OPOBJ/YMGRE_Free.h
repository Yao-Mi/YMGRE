#ifndef _YMGRE_FREE_H
#define _YMGRE_FREE_H

#include"./YMGRE_OBJ.h"

void YMGRE_Free_VectorF4d(GRE_Fvector4d pthis);
void YMGRE_Free_FMat4x4(GRE_FMat4x4 pthis);
void YMGRE_Free_Light(void* data);
void YMGRE_Free_Camera(void* data);
void YMGRE_Free_Object(void* data);
void YMGRE_Free_Lightmap(GRE_Lightmap lightmap);
void YMGRE_Free_Material(void* data);
void YMGRE_Free_Terrain(void* data);
#endif

