#ifndef YMGRE_PBR_H
#define YMGRE_PBR_H
#include "../OPOBJ/YMGRE_OBJ.h"
#include "YMGRE_List.h"
#if YMGRE_ENABLE_PBR
/* Internal band renderer: identical coverage/shading, restricted to [yBegin,yEnd). */
void YMGRE_PBR_FillRows(GRE_Vertex4d_wN vertices,GRE_Polygon4d polygon,GRE_Material material,
 GRE_List lights,gre_fvector4d* positions,GRE_FMat4x4 worldToCamera,GRE_Camera4d camera,int yBegin,int yEnd);
void YMGRE_PBR_Fill(GRE_Vertex4d_wN vertices,GRE_Polygon4d polygon,GRE_Material material,
 GRE_List lights,gre_fvector4d* positions,GRE_FMat4x4 worldToCamera,GRE_Camera4d camera);
#endif
#endif
