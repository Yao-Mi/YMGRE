#include "./YMGRE_Rendering_Pipeline.h"
#include "./YMGRE_CullingAndClipping.h"

#include "../PORT/YMGRE_YMGUI_LCD.h"
#include "./YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "./YMGRE_Camera.h"
#include "./YMGRE_Rasterization.h"

#include "./YMGRE_Light.h"
#include"./YMGRE_ScenceManager.h"

//  世界坐标系
//     ^ y
//     |
//      ---> x
//   / z 
// 
//在齐次坐标下，向量和点的区别在于是否会受到平移变换的影响，不受到影响的为向量，受到影响的为点
// 如：
//     向量： [x,y,0]   点： [x,y,1]


//每个相机都拥有一条独立的渲染管线 ,多边形
void YMGRE_Camera_PolygonPipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList)
{
	//相机初始化
	GRErgb24 background = { .R = 50,.G = 50,.B = 50 };//默认背景色
	YMGRE_CameraImage_Init(thiscam, background); //zbuff清除和图像清除

	//遍历灯光
	for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
	{
		GRE_Light4d thisLight = curLightlist->data;
		//从世界空间变换到相机空间
		YMGRE_Point_WorldToCamera(&thisLight->pos, &thisLight->proper.pos_, &thiscam->move.TMat);
	}

	//遍历物体
	for (GRE_ListNode curObjlist = ObjList->listhead; curObjlist != NULL; curObjlist = curObjlist->next)
	{
		GRE_Object4d thisobj = curObjlist->data;
		//遍历该物体所有的subMesh
		do
		{
			//所有物体从世界空间变换到相机空间
			YMGRE_Object_WorldToCamera(thisobj, &thiscam->move.TMat);
			//利用视景体将完全位于框外的物体剔除
			YMGRE_Object_FrustumCulling(thisobj, thiscam);

			if (!thisobj->isDelete) //未被剔除
			{
				//相机背面消隐，剔除背面
				YMGRE_Backface_Remove(thisobj, &thiscam->pos);

				//进行光照渲染，采用平面着色器，
				YMGRE_ObjectLighting_Color(thisobj, LightList, thiscam);

				//投影变换到视平面
				YMGRE_Object_CameraToViewPlane(thisobj, thiscam->perspectPlane.Dis);

				//将完全位于视景体外的面剔除
				YMGRE_ObjectPoly_FrustumCulling(thisobj, thiscam);

				//透视坐标变换到窗口坐标
				YMGRE_Object_ViewPlaneToWindows(thisobj, thiscam);

				//图元光栅化显示，消除被遮挡的隐面
				YMGRE_PolygonObject_Primitive_Rasterization(thisobj, thiscam, 0);
			}
		} while ((thisobj = thisobj->nextObject) != NULL);
	}

	////绘制灯光
	//for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
	//{
	//	GRE_Light4d thisLight = curLightlist->data;
	//	//非全局光照才绘制
	//	if (thisLight->type != GRE_GlobalLight)
	//	{
	//		//投影变换到视平面
	//		YMGRE_Point_CameraToViewPlane(&thisLight->proper.pos_, thiscam->perspectPlane.Dis);
	//		//透视坐标变换到窗口坐标
	//		YMGRE_Point_ViewPlaneToWindows(&thisLight->proper.pos_, thiscam);
	//		//光栅化绘制
	//		YMGRE_Light_Primitive_Rasterization(thisLight, thiscam, 2);//显示大小为2倍
	//	}
	//}
}

//使用外部工作区渲染多边形，避免把相机相关临时结果写回共享物体
void YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList,
	GRE_List MaterialList, GRE_RenderWorkspace workspace)
{
	(void)MaterialList;
	gre_log_explain(thiscam == NULL, GRE_LOG_PtrI, "输入的相机不存在");
	if (workspace == NULL)
		workspace = thiscam->workspace;
	gre_log_explain(workspace == NULL, GRE_LOG_PtrI, "相机未绑定渲染工作区");

	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(thiscam);
	//临时相机头只替换输出图像，镜头和变换参数继续复用原相机
	gre_camera4d renderCamera = *thiscam;
	renderCamera.img = *target;
	renderCamera.target = target;
	GRE_Camera4d mycam = &renderCamera;
	GRErgb24 background = { .R = 50,.G = 50,.B = 50 };
	YMGRE_CameraImage_Init(mycam, background);

	//灯光位置属于相机上下文，写入工作区而不是共享灯光对象
	uint32 lightNum = 0;
	for (GRE_ListNode node = LightList->listhead; node != NULL; node = node->next)
		lightNum++;
	YMGRE_RenderWorkspace_Reserve(workspace, 0, 0, lightNum);
	uint32 lightIndex = 0;
	for (GRE_ListNode node = LightList->listhead; node != NULL; node = node->next)
	{
		GRE_Light4d light = node->data;
		YMGRE_Point_WorldToCamera(&light->pos, &workspace->lightPos[lightIndex++], &mycam->move.TMat);
	}

	for (GRE_ListNode curObjlist = ObjList->listhead; curObjlist != NULL; curObjlist = curObjlist->next)
	{
		GRE_Object4d thisobj = curObjlist->data;
		//依次处理同一模型文件中的所有 subMesh
		do
		{
			//工作区只需扩展到当前物体规模，后续物体继续复用同一块缓存
			YMGRE_RenderWorkspace_Reserve(workspace, thisobj->pointNum, thisobj->polygonNum, lightNum);
			//世界坐标变换结果写入工作区，不修改共享物体的 pointList_
			YMGRE_Object_WorldToCameraTo(thisobj, &mycam->move.TMat, workspace->pointList);
			if (!YMGRE_Object_FrustumCullingCal(thisobj, mycam))
			{
				//逐面剔除和光照结果均按 polygon 索引写入工作区
				YMGRE_Backface_RemoveTo(thisobj, &mycam->pos, workspace->polygonHide);
				YMGRE_ObjectLighting_ColorTo(thisobj, workspace->pointList, LightList, workspace->lightPos, workspace->polygonColor);
				//相机空间依次投影到视平面，再变换到 framebuffer 窗口坐标
				YMGRE_VertexList_CameraToViewPlane(workspace->pointList, thisobj->pointNum, mycam->perspectPlane.Dis);
				YMGRE_ObjectPoly_FrustumCullingTo(thisobj, workspace->pointList, mycam, workspace->polygonHide);
				YMGRE_VertexList_ViewPlaneToWindows(workspace->pointList, thisobj->pointNum, mycam);
				//普通多边形使用扫描线填充，外部工作区版本不回写 Polygon 状态
				YMGRE_PolygonObject_Primitive_RasterizationTo(thisobj, workspace->pointList, workspace->polygonHide,
					workspace->polygonColor, mycam, 0);
			}
		} while ((thisobj = thisobj->nextObject) != NULL);
	}
}

//每个相机都拥有一条独立的渲染管线 ，三角形
void YMGRE_Camera_TanglePipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList)
{
	//相机初始化
	GRErgb24 background = { .R = 50,.G = 50,.B = 50 };//默认背景色
	YMGRE_CameraImage_Init(thiscam, background); //zbuff清除和图像清除

	//遍历灯光
	for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
	{
		GRE_Light4d thisLight = curLightlist->data;
		//从世界空间变换到相机空间
		YMGRE_Point_WorldToCamera(&thisLight->pos, &thisLight->proper.pos_, &thiscam->move.TMat);
	}

	//遍历物体
	for (GRE_ListNode curObjlist = ObjList->listhead; curObjlist != NULL; curObjlist = curObjlist->next)
	{
		GRE_Object4d thisobj = curObjlist->data;
		//遍历该物体所有的subMesh
		do
		{
			//所有物体从世界空间变换到相机空间
			YMGRE_Object_WorldToCamera(thisobj, &thiscam->move.TMat);
			//利用视景体将完全位于框外的物体剔除
			YMGRE_Object_FrustumCulling(thisobj, thiscam);

			if (!thisobj->isDelete) //未被剔除
			{
				//相机背面消隐，剔除背面
				YMGRE_Backface_Remove(thisobj, &thiscam->pos);

				//进行光照渲染，采用平面着色器，
				YMGRE_ObjectLighting_Color(thisobj, LightList, thiscam);

				//投影变换到视平面
				YMGRE_Object_CameraToViewPlane(thisobj, thiscam->perspectPlane.Dis);

				//将完全位于视景体外的面剔除
				YMGRE_ObjectPoly_FrustumCulling(thisobj, thiscam);

				//透视坐标变换到窗口坐标
				YMGRE_Object_ViewPlaneToWindows(thisobj, thiscam);

				//线框模型
				if (thiscam->wireFrame == GRE_Render_Wireframe)
				{
					YMGRE_TrangleObject_Wires(thisobj, thiscam);
				}
				//实体模型
				else
				{
					//找到材质
					GRE_Material myMater = YMGRE_Material_Find(MaterialList, thisobj->materiaName);

					//三角图元光栅化显示，Zbuff算法消除被遮挡的隐面
					YMGRE_TrangleObject_Primitive_Rasterization(thisobj, myMater, thiscam);

				}
			}
		} while ((thisobj = thisobj->nextObject) != NULL);
	}

	////绘制灯光
	//for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)
	//{
	//	GRE_Light4d thisLight = curLightlist->data;
	//	//非全局光照才绘制
	//	if (thisLight->type != GRE_GlobalLight)
	//	{
	//		//投影变换到视平面
	//		YMGRE_Point_CameraToViewPlane(&thisLight->proper.pos_, thiscam->perspectPlane.Dis);
	//		//透视坐标变换到窗口坐标
	//		YMGRE_Point_ViewPlaneToWindows(&thisLight->proper.pos_, thiscam);
	//		//光栅化绘制
	//		YMGRE_Light_Primitive_Rasterization(thisLight, thiscam, 2);//显示大小为2倍
	//	}
	//}
}

//使用外部工作区渲染三角网格，支持顺序多相机共享只读场景
void YMGRE_Camera_TanglePipline_RenderingWithWorkspace(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList,
	GRE_List MaterialList, GRE_RenderWorkspace workspace)
{
	gre_log_explain(thiscam == NULL, GRE_LOG_PtrI, "输入的相机不存在");
	if (workspace == NULL)
		workspace = thiscam->workspace;
	gre_log_explain(workspace == NULL, GRE_LOG_PtrI, "相机未绑定渲染工作区");

	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(thiscam);
	//渲染期间不改相机的兼容 img，端口仍可按旧接口读取原图像字段
	gre_camera4d renderCamera = *thiscam;
	renderCamera.img = *target;
	renderCamera.target = target;
	GRE_Camera4d mycam = &renderCamera;
	GRErgb24 background = { .R = 50,.G = 50,.B = 50 };
	YMGRE_CameraImage_Init(mycam, background);

	//灯光的相机空间位置按当前相机重新计算，避免多相机互相覆盖
	uint32 lightNum = 0;
	for (GRE_ListNode node = LightList->listhead; node != NULL; node = node->next)
		lightNum++;
	YMGRE_RenderWorkspace_Reserve(workspace, 0, 0, lightNum);
	uint32 lightIndex = 0;
	for (GRE_ListNode node = LightList->listhead; node != NULL; node = node->next)
	{
		GRE_Light4d light = node->data;
		YMGRE_Point_WorldToCamera(&light->pos, &workspace->lightPos[lightIndex++], &mycam->move.TMat);
	}

	for (GRE_ListNode curObjlist = ObjList->listhead; curObjlist != NULL; curObjlist = curObjlist->next)
	{
		GRE_Object4d thisobj = curObjlist->data;
		//依次处理同一模型文件中的所有 subMesh
		do
		{
			//每次只保留当前物体的临时结果，工作区容量按最大物体复用
			YMGRE_RenderWorkspace_Reserve(workspace, thisobj->pointNum, thisobj->polygonNum, lightNum);
			//世界坐标变换结果写入工作区，不修改共享物体的 pointList_
			YMGRE_Object_WorldToCameraTo(thisobj, &mycam->move.TMat, workspace->pointList);
			if (!YMGRE_Object_FrustumCullingCal(thisobj, mycam))
			{
				//背面状态和逐面光照颜色均保存在当前工作区
				YMGRE_Backface_RemoveTo(thisobj, &mycam->pos, workspace->polygonHide);
				YMGRE_ObjectLighting_ColorTo(thisobj, workspace->pointList, LightList, workspace->lightPos, workspace->polygonColor);
				//投影后完成逐面视景体剔除，再变换到窗口坐标进行光栅化
				YMGRE_VertexList_CameraToViewPlane(workspace->pointList, thisobj->pointNum, mycam->perspectPlane.Dis);
				YMGRE_ObjectPoly_FrustumCullingTo(thisobj, workspace->pointList, mycam, workspace->polygonHide);
				YMGRE_VertexList_ViewPlaneToWindows(workspace->pointList, thisobj->pointNum, mycam);

				//相机线框模式只画边；实体模式下由模型 wireFrame 决定是否后画网格线
				if (mycam->wireFrame == GRE_Render_Wireframe)
					YMGRE_TrangleObject_WiresTo(thisobj, workspace->pointList, workspace->polygonHide, mycam);
				else
				{
					GRE_Material myMater = YMGRE_Material_Find(MaterialList, thisobj->materiaName);
					YMGRE_TrangleObject_Primitive_RasterizationTo(thisobj, workspace->pointList, workspace->polygonHide,
						workspace->polygonColor, myMater, mycam);
				}
			}
		} while ((thisobj = thisobj->nextObject) != NULL);
	}
}



////绘制线框
//for (int j = 0; j < myobj.polygonNum; j++)
//{
//	GRE_Polygon4d thispoly = &myobj.polygonList[j];
//	if (thispoly->ishide)
//	{
//		continue;
//	}
//	for (int i = 0; i < thispoly->num; i++)
//	{
//		int i1 = thispoly->index[i];
//		int i2;
//		if (i == (thispoly->num - 1))
//			i2 = thispoly->index[0];
//		else
//		{
//			i2 = thispoly->index[i + 1]; //完成闭环
//		}
//
//		LCD_Draw_Line(myobj.pointList_[i1].pos.x, myobj.pointList_[i1].pos.y,
//			myobj.pointList_[i2].pos.x, myobj.pointList_[i2].pos.y);
//	}
//}



#if 0
#include "../../Demo/legacy/worldmap.h"

//平面着色指一个多边形一个颜色，平滑着色是一个顶点一个颜色然后使用双线性插值得到
gre_object4d myobj;

void objInit()
{
	int r = 100;
	int galpha = 20, gbeta = 20;//面片的夹角
	int N1 = 180 / galpha;//维度划分
	int N2 = 360 / gbeta;//经度划分
	//顶点列表
	myobj.pointNum = (N1 - 1) * N2 + 2; //两个南北极点 + N-1个中间点
	myobj.pointList = (gre_vertex4d*)GRE_malloc1(myobj.pointNum * sizeof(gre_vertex4d));
	myobj.pointList_ = (gre_vertex4d*)GRE_malloc1(myobj.pointNum * sizeof(gre_vertex4d));
	//计算顶点坐标
	//北极点
	myobj.pointList[0].pos.x = 0; myobj.pointList[0].pos.y = r; myobj.pointList[0].pos.z = 0;
	myobj.pointList[0].pos.w = 1;
	//南极点
	int indx = myobj.pointNum - 1;
	myobj.pointList[indx].pos.x = 0; myobj.pointList[indx].pos.y = -r; myobj.pointList[indx].pos.z = 0;
	myobj.pointList[indx].pos.w = 1;

	//中间点
	for (int i = 0; i < N1 - 1; i++)
	{
		float alp = (i + 1) * galpha * YMGRE_Deg2Rad;
		for (int j = 0; j < N2; j++)
		{
			float bet = j * gbeta * YMGRE_Deg2Rad;
			int indx = i * N2 + j + 1;
			myobj.pointList[indx].pos.x = r * sin(alp) * sin(bet);
			myobj.pointList[indx].pos.y = r * cos(alp);
			myobj.pointList[indx].pos.z = r * sin(alp) * cos(bet);
			myobj.pointList[indx].pos.w = 1;
		}
	}

	//计算平面数
	myobj.polygonNum = N1 * N2;
	myobj.polygonList = (gre_polygon4d*)GRE_malloc1(myobj.polygonNum * sizeof(gre_polygon4d));

	// 构建南北极三角形面片
	indx = (N1 - 1) * N2;
	for (int i = 0; i < N2; i++)
	{
		int j = i + 1;
		if (j == N2)j = 0; //首尾相连
		//        0
		//      / | \
		// 1 2 3 ... N2-1
		int polynum = 3;//三角形
		myobj.polygonList[i].num = polynum;
		myobj.polygonList[i].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj.polygonList[i].index[0] = 0;
		myobj.polygonList[i].index[1] = i + 1; //北极点偏移量为 1
		myobj.polygonList[i].index[2] = j + 1;
		myobj.polygonList[i].ishide = 0;//未被隐藏

		// 1 2 3 ... N2-1
		//      \ | /
		//        0

		//indx = (N1 - 1) * N2;
		int ii = indx + i;
		myobj.polygonList[ii].num = polynum;//三角形
		myobj.polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj.polygonList[ii].index[0] = indx - N2 + i + 1;
		myobj.polygonList[ii].index[1] = indx + 1;//北极点
		myobj.polygonList[ii].index[2] = indx - N2 + j + 1;

		myobj.polygonList[ii].ishide = 0;//未被隐藏
	}
	//中间四边形构建
	for (int i = 1; i < N1 - 1; i++)
	{
		indx = i * N2;
		for (int j = 0; j < N2; j++)
		{
			int tj = j + 1;
			if (tj == N2)tj = 0; //首尾相连
			//    2  3
			//    /  \
			// 1  2  3 ... N2-1
			int ii = indx + j;
			//indx = i * N2
			int polynum = 4;//四边形
			myobj.polygonList[ii].num = polynum;
			myobj.polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

			myobj.polygonList[ii].index[0] = indx - N2 + j + 1; //+1代表北极点偏移为1
			myobj.polygonList[ii].index[1] = indx + j + 1;
			myobj.polygonList[ii].index[2] = indx + tj + 1;
			myobj.polygonList[ii].index[3] = indx - N2 + tj + 1;
			myobj.polygonList[ii].ishide = 0;//未被隐藏
		}
	}

	//计算平面法向量
	for (int i = 0; i < myobj.polygonNum; i++)
	{
		//计算平面法向量
		// 0   3
		// 1   2
		//u= p1->p2  v= p1->p3 ，边按逆时针排布
		GRE_Polygon4d thispoly = &myobj.polygonList[i];
		GRE_Vertex4d points = myobj.pointList;
		//计算平面法向量
		int i1 = thispoly->index[0];
		int i2 = thispoly->index[1];
		int i3 = thispoly->index[2];

		GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&points[i2].pos, &points[i1].pos);
		GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&points[i3].pos, &points[i1].pos);
		GRE_Fvector4d n = YMGRE_Fvector4d_Cross(u, v);// n=u×v

		YMGRE_Fvector4d_AssTo(&thispoly->pN, n);// pn = n
		//释放内存
		YMGRE_Free_VectorF4d(u);
		YMGRE_Free_VectorF4d(v);
		YMGRE_Free_VectorF4d(n);

		//设置平面的颜色
		GRErgb24 clor;
		int index_y = i / N2; //每行N2个数据，一共N1行
		int index_x = i % N2; // 
		uint8* rgb16 = YMGRE_World_data; //w = 640  h=320
		float32 kx = (640.0f - 1) / (N2 - 1);
		float32 ky = (320.0f - 1) / (N1 - 1);

		//转为图形索引
		int i_x = index_x * kx;
		int i_y = index_y * ky;
		int rgb_index = (i_y * 640 + i_x) * 2;
		_color16_t thiscolor = rgb16[rgb_index] | ((uint16)rgb16[rgb_index + 1] << 8);
		//clor.R = (i / N2) * 255 / N1;
		clor.R = (thiscolor & 0xF800) >> 8;
		clor.G = (thiscolor & 0x07E0) >> 3;
		clor.B = (thiscolor & 0x001F) << 3;
		thispoly->planeColor = clor;
	}


	myobj.mirrorKs = 0.8;
	myobj.WorldCoordinate.x = 0;
	myobj.WorldCoordinate.y = 0;
	myobj.WorldCoordinate.z = 0;
	myobj.WorldCoordinate.w = 1; //作为位置，是一个点

	myobj.scale = 0.8f;//放缩尺度
	myobj.isDelete = 0;//不被剔除
	myobj.boundType = GRE_Bounding_Sphere_R;//使用球体进行包围
	myobj.BoundingSphereR = r;
}


gre_light4d mylight[2];

void lightInit()
{
	//全局光照

	mylight[0].ID = 1; //确定ID编号
	mylight[0].ishide = 0;//启用灯光
	mylight[0].type = GRE_GlobalLight;//设置为全局光源

	//设置属性
	mylight[0].proper.lightcolor.R = 255; //光的颜色强度
	mylight[0].proper.lightcolor.G = 255;
	mylight[0].proper.lightcolor.B = 255;

	//点光源

	mylight[1].ID = 2; //确定ID编号
	mylight[1].ishide = 0;//启用灯光
	mylight[1].type = GRE_PointLight;//设置为全局光源

	//设置属性
	mylight[1].proper.lightcolor.R = 255; //光的颜色强度
	mylight[1].proper.lightcolor.G = 255;
	mylight[1].proper.lightcolor.B = 255;

	mylight[1].proper.kc0 = 1.0f; //确保衰减系数:  1 /（c0+c1*d +c2*d^2）<= 1
	mylight[1].proper.kc1 = 0.01f;
	mylight[1].proper.kc2 = 0.0f;

	mylight[1].pos.x = -50; //位置
	mylight[1].pos.y = -50;
	mylight[1].pos.z = 90;
	mylight[1].pos.w = 1;
}

void YMGRE_Pipline_Rendering0()
{
	objInit();
	//左右开角
	float32 angL = 30;
	float32 angR = 30;
	//上下开角
	float32 angU = 30;
	float32 angD = 30;
	GRE_Camera4d mycam = YMGRE_Creat_Camera(0,500, 500, angL, angR, angU, angD);//创建摄像机，根据定义自动创建视平面
	YMGRE_Camera_Frustum_Init(mycam, 50, 800);

	//初始位置点和注视点
	gre_fvector4d campos, targetpos;
	campos.x = 0; campos.y = 0; campos.z = 300;
	campos.w = 1;
	targetpos.x = 0; targetpos.y = 0; targetpos.z = 0;
	targetpos.w = 1;
	//自旋角
	float32 theta = 0;

	lightInit();//开启光照
	gre_list LightList = { 0 };
	YMGRE_List_Append(&LightList, sizeof(gre_light4d), &mylight[0]);
	YMGRE_List_Append(&LightList, sizeof(gre_light4d), &mylight[1]);
	//添加物体
	gre_list ObjList = { 0 };
	YMGRE_List_Append(&ObjList, sizeof(gre_object4d), &myobj);

	gre_list MaterialList = { 0 };

	//物理空间变换到世界空间
	YMGRE_Object_LocalToWorld(&myobj);

	while (LCD_Update(30))
	{
		char a;
		if (LCD_GetChar(&a))
		{
			if (a == 'w')campos.z += 10;
			if (a == 's')campos.z -= 10;
			if (a == 'd')theta += 10;
			if (a == 'a')theta -= 10;

			printf("theta=%.5f  ", theta);
			printf("z=%.5f  \n", campos.z);
		}

		int x, y;
		if (LCD_GetXY(0, 0, mycam->img.width, mycam->img.height, &x, &y))
		{
			campos.x = x - mycam->img.width / 2;
			campos.y = mycam->img.height / 2 - y;//反向
			//mylight[1].pos.x = x - 100; //位置
			//mylight[1].pos.y = 100 - y;
			printf("x=%.5f y=%.5f \n", campos.x, campos.y);
		}
		//空间位置初始化,使用默认的v参考向量
		YMGRE_UVNCamera_PositionInit(mycam, &campos, &targetpos, NULL, theta);

		//对相机所在管线进行渲染
		YMGRE_Camera_PolygonPipline_Rendering(mycam, &LightList, &ObjList, &MaterialList);

		//显示摄像头采集到的图像
		LCD_Fill_RgbRect(0, 0, mycam->img.width, mycam->img.height, mycam->img.data);
	}
	//释放相机内存
	YMGRE_Free_Camera(mycam);
}
#endif
