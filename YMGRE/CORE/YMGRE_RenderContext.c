#include "./YMGRE_RenderContext.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"

/*-------------------------------------  渲染目标  ---------------------------------------------*/

//创建自带颜色和深度缓存的渲染目标，颜色缓存格式由 GRE_FramePixel 决定
GRE_RenderTarget YMGRE_Creat_RenderTarget(uint16 width, uint16 height)
{
	GRE_RenderTarget target = GRE_malloc0(sizeof(gre_render_target));
	gre_log_explain(target == NULL, GRE_LOG_Mem0, "渲染目标头内存申请失败");
	target->width = width;
	target->height = height;
	target->data = GRE_ImageBuff_Malloc((size_t)width * height * sizeof(GRE_FramePixel));
	target->zbuff = GRE_ImageBuff_Malloc((size_t)width * height * sizeof(float32));
	gre_log_explain((target->data == NULL) || (target->zbuff == NULL), GRE_LOG_Mem1, "渲染目标缓存申请失败");
	return target;
}

//绑定调用者提供的缓存，只记录地址，不取得外部缓存的释放权
void YMGRE_RenderTarget_Init(GRE_RenderTarget target, uint16 width, uint16 height, GRE_FrameBuffer color, float32* depth)
{
	gre_log_explain((target == NULL) || (color == NULL) || (depth == NULL), GRE_LOG_PtrIO, "渲染目标或外部缓存不存在");
	target->width = width;
	target->height = height;
	target->data = color;
	target->zbuff = depth;
}

//释放 Creat 接口创建的目标及其颜色、深度缓存
void YMGRE_Free_RenderTarget(GRE_RenderTarget target)
{
	if (target == NULL)return;
	GRE_ImageBuff_Free(target->zbuff);
	GRE_ImageBuff_Free(target->data);
	GRE_free0(target);
}

/*-------------------------------------  渲染工作区  ---------------------------------------------*/

//创建可按场景中最大单个物体容量自动扩展的工作区
GRE_RenderWorkspace YMGRE_Creat_RenderWorkspace(void)
{
	GRE_RenderWorkspace workspace = GRE_malloc0(sizeof(gre_render_workspace));
	gre_log_explain(workspace == NULL, GRE_LOG_Mem0, "渲染工作区头内存申请失败");
	GRE_memset(workspace, 0, sizeof(gre_render_workspace));
	workspace->ownsMemory = 1;
	workspace->pointList_wN = NULL;
	workspace->pointWNMax = 0;
	return workspace;
}

//绑定外部静态工作区，适合 MCU 使用固定容量内存
void YMGRE_RenderWorkspace_Init(GRE_RenderWorkspace workspace, GRE_Vertex4d points, uint32 pointMax,
	uint8* polygonHide, GRErgb24* polygonColor, uint32 polygonMax,
	gre_fvector4d* lightPos, uint32 lightMax)
{
	gre_log_explain((workspace == NULL) || (points == NULL) || (polygonHide == NULL) ||
		(polygonColor == NULL) || ((lightMax > 0) && (lightPos == NULL)), GRE_LOG_PtrIO, "渲染工作区或外部缓存不存在");
	workspace->pointList = points;
	workspace->polygonHide = polygonHide;
	workspace->polygonColor = polygonColor;
	workspace->lightPos = lightPos;
	workspace->pointMax = pointMax;
	workspace->polygonMax = polygonMax;
	workspace->lightMax = lightMax;
#if YMGRE_ENABLE_TRANSPARENCY
 workspace->transparentTriangles=NULL;workspace->transparentCapacity=0;
#if YMGRE_ENABLE_RASTER_DISPATCH
 workspace->rasterDispatch=NULL;workspace->rasterDispatchUser=NULL;
#endif
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 workspace->linearColor=NULL;workspace->linearCapacity=0;
#endif
 workspace->materialStatus=0;
	workspace->ownsMemory = 0;
	workspace->pointList_wN = NULL;
	workspace->pointWNMax = 0;
}

#if YMGRE_ENABLE_RASTER_DISPATCH
void YMGRE_RenderWorkspace_SetRasterDispatcher(GRE_RenderWorkspace workspace,
 GRE_RasterDispatch dispatch,void* user)
{
 if(workspace){workspace->rasterDispatch=dispatch;workspace->rasterDispatchUser=user;}
}
#endif

void YMGRE_RenderWorkspace_BindVertexAttributes(GRE_RenderWorkspace workspace,
	GRE_Vertex4d_wN points, uint32 pointMax)
{
	if (workspace == NULL || points == NULL || pointMax == 0) return;
	if (workspace->ownsMemory) return;
	workspace->pointList_wN = points;
	workspace->pointWNMax = pointMax;
}

int YMGRE_RenderWorkspace_EnableVertexAttributes(GRE_RenderWorkspace workspace, uint32 pointNum)
{
	if (workspace == NULL || pointNum == 0) return 0;
	if (workspace->pointList_wN != NULL && workspace->pointWNMax >= pointNum) return 1;
	if (!workspace->ownsMemory) return 0;//固定工作区容量不足时不自动分配
	GRE_Vertex4d_wN points = GRE_malloc1(pointNum * sizeof(gre_vertex4d_wN));
	if (points == NULL) return 0;
	if (workspace->ownsMemory) GRE_free1(workspace->pointList_wN);
	workspace->pointList_wN = points;
	workspace->pointWNMax = pointNum;
	return 1;
}

//确保动态工作区容量足够，外部工作区只检查容量，不在渲染帧内偷偷申请内存
void YMGRE_RenderWorkspace_Reserve(GRE_RenderWorkspace workspace, uint32 pointNum, uint32 polygonNum, uint32 lightNum)
{
	gre_log_explain(workspace == NULL, GRE_LOG_PtrI, "渲染工作区不存在");
	if (!workspace->ownsMemory)
	{
		gre_log_explain((pointNum > workspace->pointMax) || (polygonNum > workspace->polygonMax) ||
			(lightNum > workspace->lightMax), GRE_LOG_Mem1, "外部渲染工作区容量不足");
		return;
	}

	if (pointNum > workspace->pointMax)
	{
		//先申请新缓存再替换旧缓存，扩容后原有临时结果不需要保留
		GRE_Vertex4d points = GRE_malloc1(pointNum * sizeof(gre_vertex4d));
		gre_log_explain(points == NULL, GRE_LOG_Mem1, "渲染顶点缓存申请失败");
		GRE_free1(workspace->pointList);
		workspace->pointList = points;
		workspace->pointMax = pointNum;
	}
	if (polygonNum > workspace->polygonMax)
	{
		//多边形隐藏状态和光照颜色使用相同容量，便于按索引并行访问
		uint8* polygonHide = GRE_malloc1(polygonNum * sizeof(uint8));
		GRErgb24* polygonColor = GRE_malloc1(polygonNum * sizeof(GRErgb24));
		gre_log_explain((polygonHide == NULL) || (polygonColor == NULL), GRE_LOG_Mem1, "渲染多边形缓存申请失败");
		GRE_free1(workspace->polygonHide);
		GRE_free1(workspace->polygonColor);
		workspace->polygonHide = polygonHide;
		workspace->polygonColor = polygonColor;
		workspace->polygonMax = polygonNum;
	}
	if (lightNum > workspace->lightMax)
	{
		gre_fvector4d* lightPos = GRE_malloc1(lightNum * sizeof(gre_fvector4d));
		gre_log_explain(lightPos == NULL, GRE_LOG_Mem1, "渲染灯光缓存申请失败");
		GRE_free1(workspace->lightPos);
		workspace->lightPos = lightPos;
		workspace->lightMax = lightNum;
	}
}

//只释放动态工作区拥有的缓存，外部工作区及其缓存均由调用者管理
void YMGRE_Free_RenderWorkspace(GRE_RenderWorkspace workspace)
{
	if (workspace == NULL)return;
	if (workspace->ownsMemory)
	{
		GRE_free1(workspace->pointList);
		GRE_free1(workspace->pointList_wN);
		GRE_free1(workspace->polygonHide);
		GRE_free1(workspace->polygonColor);
		GRE_free1(workspace->lightPos);
#if YMGRE_ENABLE_TRANSPARENCY
        GRE_free1(workspace->transparentTriangles);
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
        GRE_free1(workspace->linearColor);
#endif
		GRE_free0(workspace);
	}
}

/*-------------------------------------  相机绑定  ---------------------------------------------*/

//给相机切换实际输出目标，不释放相机原有的兼容 img 缓存
void YMGRE_Camera_BindRenderTarget(GRE_Camera4d camera, GRE_RenderTarget target)
{
	gre_log_explain((camera == NULL) || (target == NULL), GRE_LOG_PtrI, "相机或渲染目标不存在");
	camera->target = target;
	//外部目标相机的img只是兼容视图；普通相机则保留自己创建的img以便最终释放
	if (!camera->ownsImageBuffers)
		camera->img = *target;
}

//给相机绑定共享或独立工作区，同一工作区不能被多个渲染任务并行使用
void YMGRE_Camera_BindRenderWorkspace(GRE_Camera4d camera, GRE_RenderWorkspace workspace)
{
	gre_log_explain((camera == NULL) || (workspace == NULL), GRE_LOG_PtrI, "相机或渲染工作区不存在");
	camera->workspace = workspace;
}

//优先返回外部 Target，未绑定时回退到相机自带的兼容 img
GRE_RenderTarget YMGRE_Camera_GetRenderTarget(GRE_Camera4d camera)
{
	gre_log_explain(camera == NULL, GRE_LOG_PtrI, "相机不存在");
	return (camera->target != NULL) ? camera->target : &camera->img;
}

void YMGRE_RenderWorkspace_BindMaterialBuffers(GRE_RenderWorkspace ws,void* packets,size_t bytes,float32* rgb,size_t count)
{
 if(!ws||ws->ownsMemory)return;
#if YMGRE_ENABLE_TRANSPARENCY
 ws->transparentTriangles=packets;ws->transparentCapacity=packets?bytes:0;
#else
 (void)packets;(void)bytes;
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 ws->linearColor=rgb;ws->linearCapacity=rgb?count:0;
#else
 (void)rgb;(void)count;
#endif
}
