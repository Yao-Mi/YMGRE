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

void YMGRE_Object_LocalToWorld_wN(GRE_Object4d myobj)
{
	if (myobj == NULL) return;
	YMGRE_Object_LocalToWorld(myobj);
	if (myobj->pointList_wN == NULL || myobj->pointList_wN_ == NULL) return;
	for (uint32 i = 0; i < (uint32)myobj->pointNum; i++)
	{
		myobj->pointList_wN[i].base = myobj->pointList[i];
		myobj->pointList_wN_[i].base = myobj->pointList[i];
	}
}

//世界坐标变换到外部相机顶点缓存
void YMGRE_Object_WorldToCameraTo(GRE_Object4d myobj, GRE_FMat4x4 camera, GRE_Vertex4d out)
{
	gre_log_explain((myobj == NULL) || (camera == NULL) || (out == NULL), GRE_LOG_PtrIO, "输入的物体、相机或输出缓存不存在");
	for (uint32 i = 0, danum = myobj->pointNum; i < danum; i++)
	{
		YMGRE_Fvector4d_MatMultTo(camera, &myobj->pointList[i].pos, &out[i].pos);
		out[i].u = myobj->pointList[i].u;
		out[i].v = myobj->pointList[i].v;
	}
}

void YMGRE_Object_WorldToCameraTo_wN(GRE_Object4d myobj, GRE_FMat4x4 camera, GRE_Vertex4d_wN out)
{
	YMGRE_Object_WorldToCameraMaskedTo(myobj, camera, out, 1, NULL);
}

void YMGRE_Object_WorldToCameraMaskedTo(GRE_Object4d myobj, GRE_FMat4x4 camera,
	GRE_Vertex4d_wN out, uint8 transformTangent, const uint8* active)
{
	gre_log_explain((myobj == NULL) || (camera == NULL) || (out == NULL), GRE_LOG_PtrIO,
		"输入的物体、相机或高级顶点缓存不存在");
	if (myobj == NULL || camera == NULL || out == NULL || myobj->pointList_wN == NULL)
		return;
	uint8 diagonal = camera->val[3][0] == 0 && camera->val[3][1] == 0 &&
		camera->val[3][2] == 0 && camera->val[3][3] == 1;
	for (uint8 row = 0; row < 3; row++)
		for (uint8 col = 0; col < 3; col++)
			if (row != col && camera->val[row][col] != 0) diagonal = 0;
	if (diagonal) {
		float32 sx = camera->val[0][0], sy = camera->val[1][1], sz = camera->val[2][2];
		float32 tx = camera->val[0][3], ty = camera->val[1][3], tz = camera->val[2][3];
		for (uint32 i = 0; i < (uint32)myobj->pointNum; i++) {
			if (active && !active[i]) continue;
			out[i] = myobj->pointList_wN[i];
			out[i].base.pos.x = sx * out[i].base.pos.x + tx * out[i].base.pos.w;
			out[i].base.pos.y = sy * out[i].base.pos.y + ty * out[i].base.pos.w;
			out[i].base.pos.z = sz * out[i].base.pos.z + tz * out[i].base.pos.w;
			out[i].normal.x *= sx;
			out[i].normal.y *= sy;
			out[i].normal.z *= sz;
			out[i].normal.w = 0;
			if (transformTangent) {
				out[i].tangent.x *= sx;
				out[i].tangent.y *= sy;
				out[i].tangent.z *= sz;
			}
			out[i].tangent.w = 0;
		}
		return;
	}
	for (uint32 i = 0; i < (uint32)myobj->pointNum; i++)
	{
		if (active && !active[i]) continue;
		YMGRE_Fvector4d_MatMultTo(camera, &myobj->pointList_wN[i].base.pos, &out[i].base.pos);
		out[i].base.u = myobj->pointList_wN[i].base.u;
		out[i].base.v = myobj->pointList_wN[i].base.v;
		out[i].normal = myobj->pointList_wN[i].normal;
		out[i].normal.w = 0;
		out[i].tangent = myobj->pointList_wN[i].tangent;
		out[i].tangent.w = 0;
		out[i].tangentW = myobj->pointList_wN[i].tangentW;
		out[i].lightmapU = myobj->pointList_wN[i].lightmapU;
		out[i].lightmapV = myobj->pointList_wN[i].lightmapV;
		out[i].color = myobj->pointList_wN[i].color;
		out[i].vertexLighting = myobj->pointList_wN[i].vertexLighting;
		out[i].vertexSpecular = myobj->pointList_wN[i].vertexSpecular;
		YMGRE_Fvector4d_MatMultTo(camera, &out[i].normal, &out[i].normal);
		if (transformTangent)
			YMGRE_Fvector4d_MatMultTo(camera, &out[i].tangent, &out[i].tangent);
		out[i].normal.w = out[i].tangent.w = 0;
	}
}

void YMGRE_VertexList_CameraToViewPlane_wN(GRE_Vertex4d_wN points, uint32 pointNum, float32 viewPlaneDis)
{
	if (points == NULL || viewPlaneDis < 1e-9f) return;
	for (uint32 i = 0; i < pointNum; i++)
	{
		float32 pk = viewPlaneDis / points[i].base.pos.z;
		points[i].base.pos.x *= pk;
		points[i].base.pos.y *= pk;
	}
}

void YMGRE_VertexList_ViewPlaneToWindows_wN(GRE_Vertex4d_wN points, uint32 pointNum, GRE_Camera4d mycam)
{
	if (points == NULL || mycam == NULL) return;
	float32 pkw = mycam->img.width / (mycam->perspectPlane.pR - mycam->perspectPlane.pL);
	float32 pkh = mycam->img.height / (mycam->perspectPlane.pU - mycam->perspectPlane.pD);
	float32 w2 = mycam->img.width / 2.0f, h2 = mycam->img.height / 2.0f;
	for (uint32 i = 0; i < pointNum; i++)
	{
		points[i].base.pos.x = points[i].base.pos.x * pkw + w2;
		points[i].base.pos.y = points[i].base.pos.y * -pkh + h2;
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

//外部相机顶点缓存变换到视平面坐标
void YMGRE_VertexList_CameraToViewPlane(GRE_Vertex4d points, uint32 pointNum, float32 viewPlaneDis)
{
	gre_log_explain(points == NULL, GRE_LOG_PtrI, "输入的顶点缓存不存在");
	gre_log_explain(viewPlaneDis < 1e-9f, GRE_LOG_PtrI, "视平面距离需>0");
	for (uint32 i = 0; i < pointNum; i++)
	{
		float32 pk = viewPlaneDis / points[i].pos.z;
		points[i].pos.x *= pk;
		points[i].pos.y *= pk;
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

//外部视平面顶点缓存变换到窗口坐标
void YMGRE_VertexList_ViewPlaneToWindows(GRE_Vertex4d points, uint32 pointNum, GRE_Camera4d mycam)
{
	gre_log_explain((points == NULL) || (mycam == NULL), GRE_LOG_PtrIO, "输入的顶点缓存或相机不存在");
	float32 viewPlaneW = mycam->perspectPlane.pR - mycam->perspectPlane.pL;
	float32 viewPlaneH = mycam->perspectPlane.pU - mycam->perspectPlane.pD;
	float32 winW = mycam->img.width;
	float32 winH = mycam->img.height;
	float32 pkw = winW / viewPlaneW;
	float32 pkh = winH / viewPlaneH;
	float32 w_2 = winW / 2.0f;
	float32 h_2 = winH / 2.0f;

	for (uint32 i = 0; i < pointNum; i++)
	{
		points[i].pos.x *= pkw;
		points[i].pos.y *= -pkh;
		points[i].pos.x += w_2;
		points[i].pos.y += h_2;
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


