#ifndef YMGRE_RENDERCONTEXT_H
#define YMGRE_RENDERCONTEXT_H

#include "../OPOBJ/YMGRE_OBJ.h"

GRE_RenderTarget YMGRE_Creat_RenderTarget(uint16 width, uint16 height);//创建并持有颜色、深度缓存
void YMGRE_RenderTarget_Init(GRE_RenderTarget target, uint16 width, uint16 height, GRE_FrameBuffer color, float32* depth);//绑定外部缓存，不接管所有权
void YMGRE_Free_RenderTarget(GRE_RenderTarget target);//只释放 Creat 接口返回的目标

GRE_RenderWorkspace YMGRE_Creat_RenderWorkspace(void);//创建可自动扩容的渲染工作区
void YMGRE_RenderWorkspace_Init(GRE_RenderWorkspace workspace, GRE_Vertex4d points, uint32 pointMax,
	uint8* polygonHide, GRErgb24* polygonColor, uint32 polygonMax,
	gre_fvector4d* lightPos, uint32 lightMax);//绑定外部固定容量缓存，不自动扩容
void YMGRE_RenderWorkspace_Reserve(GRE_RenderWorkspace workspace, uint32 pointNum, uint32 polygonNum, uint32 lightNum);//确保动态工作区容量足够
int YMGRE_RenderWorkspace_EnableVertexAttributes(GRE_RenderWorkspace workspace, uint32 pointNum);//按需申请高级顶点工作缓存
void YMGRE_RenderWorkspace_BindVertexAttributes(GRE_RenderWorkspace workspace,
	GRE_Vertex4d_wN points, uint32 pointMax);//给固定工作区绑定调用者持有的高级缓存
void YMGRE_Free_RenderWorkspace(GRE_RenderWorkspace workspace);//释放独立渲染工作区

void YMGRE_Camera_BindRenderTarget(GRE_Camera4d camera, GRE_RenderTarget target);//给相机绑定输出目标
void YMGRE_Camera_BindRenderWorkspace(GRE_Camera4d camera, GRE_RenderWorkspace workspace);//给相机绑定共享或独立工作区
GRE_RenderTarget YMGRE_Camera_GetRenderTarget(GRE_Camera4d camera);//取得相机当前输出目标

#endif // !YMGRE_RENDERCONTEXT_H
