#include "./YMGRE_Coordinates_Transform.h"
#include "./YMGRE_MathBase.h"

//从局部坐标变换到世界坐标
void YMGRE_Object_LocalToWorld(GRE_Object4d myobj)
{
	gre_log_explain(myobj == NULL, GRE_LOG_PtrI, "输入的物体不存在");
	GRE_Vertex4d localpos = myobj->pointList;
	gre_fvector4d worldmove;

	YMGRE_Fvector4d_AssTo(&worldmove, &myobj->WorldCoordinate);
	worldmove.w = 0; //作为偏移量，是一个向量，该分量是0

	float32 lambda = myobj->scale;
	for (uint32 i = 0,danum= myobj->pointNum; i < danum; i++)
	{
		//尺度变换
		YMGRE_Fvector4d_ScaleTo(&localpos[i].pos, lambda);
		// 移动到世界坐标上
		YMGRE_Fvector4d_AddTo(&localpos[i].pos, &worldmove);
	}
}

//世界坐标变换到相机坐标
void YMGRE_Object_WorldToCamera(GRE_Object4d myobj, GRE_FMat4x4 camera)
{
	gre_log_explain((myobj == NULL)||(camera == NULL), GRE_LOG_PtrI, "输入的物体或相机不存在");
	GRE_Vertex4d localpos = myobj->pointList;
	GRE_Vertex4d localpos2 = myobj->pointList_;
	for (uint32 i = 0, danum = myobj->pointNum; i < danum; i++)
	{
		//变换到相机坐标系
		YMGRE_Fvector4d_MatMultTo(camera , &localpos[i].pos, &localpos2[i].pos);
	}
}


//相机坐标变换到视平面坐标
void YMGRE_Object_CameraToViewPlane(GRE_Object4d myobj, float32 viewPlaneDis)
{
	gre_log_explain(myobj == NULL, GRE_LOG_PtrI, "输入的物体不存在");
	gre_log_explain(viewPlaneDis< 1e-9f, GRE_LOG_PtrI, "视平面距离需>0");
	GRE_Vertex4d localpos = myobj->pointList_;

	for (uint32 i = 0, danum = myobj->pointNum; i < danum; i++)
	{
		//透视投影变换
		float32 pk = viewPlaneDis / localpos[i].pos.z;
		localpos[i].pos.x *= pk;
		localpos[i].pos.y *= pk;
	}
}

//视平面变换到窗口坐标
void YMGRE_Object_ViewPlaneToWindows(GRE_Object4d myobj, GRE_Camera4d mycam)
{
	gre_log_explain(mycam == NULL, GRE_LOG_PtrI, "输入的相机不存在");
	float32 viewPlaneW= mycam->perspectPlane.pR- mycam->perspectPlane.pL;
	float32 viewPlaneH= mycam->perspectPlane.pU- mycam->perspectPlane.pD;
	float32 winW = mycam->img.width;
	float32 winH = mycam->img.height;
	gre_log_explain(myobj == NULL, GRE_LOG_PtrI, "输入的物体不存在");
	//尺度变换
	float pkw = winW / viewPlaneW;
	float pkh = winH / viewPlaneH;
	float w_2 = winW / 2.0f;
	float h_2 = winH / 2.0f;
	
	GRE_Vertex4d localpos = myobj->pointList_;
	for (uint32 i = 0, danum = myobj->pointNum; i < danum; i++)
	{
		//尺度变换
		localpos[i].pos.x *= pkw;
		localpos[i].pos.y *= -pkh;//y坐标取反
		//从[-w2,w2]偏移到[0,w]
		localpos[i].pos.x += w_2;
		localpos[i].pos.y += h_2;
	}
}


//世界坐标变换到相机坐标
void YMGRE_Point_WorldToCamera(GRE_Fvector4d point, GRE_Fvector4d out, GRE_FMat4x4 camera)
{
	gre_log_explain((point == NULL) || (camera == NULL), GRE_LOG_PtrIO, "输入输出的点或相机不存在");

	//变换到相机坐标系
	YMGRE_Fvector4d_MatMultTo(camera, point, out);
}
//坐标点变换到 视平面
void YMGRE_Point_CameraToViewPlane(GRE_Fvector4d point, float32 viewPlaneDis)
{
	gre_log_explain(point == NULL, GRE_LOG_PtrI, "输入的点不存在");
	gre_log_explain(viewPlaneDis < 1e-9f, GRE_LOG_PtrI, "视平面距离需>0");
	//透视投影变换
	float32 pk = viewPlaneDis / point->z;
	point->x *= pk;
	point->y *= pk;
}
//点变换到窗口
void YMGRE_Point_ViewPlaneToWindows(GRE_Fvector4d point, GRE_Camera4d mycam)
{
	gre_log_explain(mycam == NULL, GRE_LOG_PtrI, "输入的相机不存在");
	float32 viewPlaneW = mycam->perspectPlane.pR - mycam->perspectPlane.pL;
	float32 viewPlaneH = mycam->perspectPlane.pU - mycam->perspectPlane.pD;
	float32 winW = mycam->img.width;
	float32 winH = mycam->img.height;
	gre_log_explain(point == NULL, GRE_LOG_PtrI, "输入的点不存在");
	//尺度变换
	float pkw = winW / viewPlaneW;
	float pkh = winH / viewPlaneH;
	float w_2 = winW / 2.0f;
	float h_2 = winH / 2.0f;

	//尺度变换
	point->x *= pkw;
	point->y *= -pkh;//y坐标取反
	//从[-w2,w2]偏移到[0,w]
	point->x += w_2;
	point->y += h_2;
}


