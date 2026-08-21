#ifndef YMGRE_RENDERING_PIPELINE_H
#define YMGRE_RENDERING_PIPELINE_H
#include "YMGRE_Coordinates_Transform.h"
#include"./YMGRE_List.h"

void YMGRE_Camera_PolygonPipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList);//多边形物体
void YMGRE_Camera_TanglePipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList);//三角形物体

#endif // !YMGRE_RENDERING_PIPELINE_H

