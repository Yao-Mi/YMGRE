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
int YMGRE_RenderWorkspace_EnableProjectionCache(GRE_RenderWorkspace workspace, uint32 pointNum);
void YMGRE_RenderWorkspace_BindProjectionCache(GRE_RenderWorkspace workspace,
	gre_fvector4d* positions, uint8* codes, uint32 pointMax);
void YMGRE_Free_RenderWorkspace(GRE_RenderWorkspace workspace);//释放独立渲染工作区

void YMGRE_Camera_BindRenderTarget(GRE_Camera4d camera, GRE_RenderTarget target);//给相机绑定输出目标
void YMGRE_Camera_BindRenderWorkspace(GRE_Camera4d camera, GRE_RenderWorkspace workspace);//给相机绑定共享或独立工作区
GRE_RenderTarget YMGRE_Camera_GetRenderTarget(GRE_Camera4d camera);//取得相机当前输出目标

/* Bind optional material scratch to a fixed workspace. Sizes are bytes / float count.
   Query packet stride to budget transparency storage. Contents are overwritten per frame. */
size_t YMGRE_Material_TransparentPacketSize(void);
#if YMGRE_ENABLE_RASTER_DISPATCH
/* NULL restores serial rendering. Borrowed synchronous dispatcher: no mandatory
   OS/thread dependency. Unsupported scene modes automatically use the serial path. */
void YMGRE_RenderWorkspace_SetRasterDispatcher(GRE_RenderWorkspace workspace,
 GRE_RasterDispatch dispatch,void* user);
#endif
void YMGRE_RenderWorkspace_BindMaterialBuffers(GRE_RenderWorkspace workspace,
 void* transparentPackets,size_t packetBytes,float32* linearRGB,size_t floatCount);
/** @brief 准备可复用面序缓冲；拥有型工作区在 Free 时释放。
 * @param faces 最大面数，当前上限 65536；0 不申请。
 * @return 1 容量满足，0 外部工作区或分配失败。失败保留原缓冲。
 */
int YMGRE_RenderWorkspace_ReserveFaceOrder(GRE_RenderWorkspace workspace,uint32 faces);
#endif // !YMGRE_RENDERCONTEXT_H
