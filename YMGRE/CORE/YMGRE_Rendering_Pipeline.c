#include "./YMGRE_Rendering_Pipeline.h"
#include "./YMGRE_CullingAndClipping.h"

#include "./YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "./YMGRE_Camera.h"
#include "./YMGRE_Rasterization.h"
#include "./YMGRE_TriangleRaster.h"

#include "./YMGRE_Light.h"

static float32 linePlaneDistance(GRE_Fvector4d point, GRE_Camera4d camera, uint8 plane)
{
	switch (plane) {
	case 0: return point->z - camera->frustum.Znear;
	case 1: return camera->frustum.Zfar - point->z;
	case 2: return point->x - camera->perspectPlane.kl * point->z;
	case 3: return camera->perspectPlane.kr * point->z - point->x;
	case 4: return point->y - camera->perspectPlane.kd * point->z;
	default: return camera->perspectPlane.ku * point->z - point->y;
	}
}

static uint8 clipCameraLine(GRE_Fvector4d start, GRE_Fvector4d end, GRE_Camera4d camera)
{
	for (uint8 plane = 0; plane < 6; plane++) {
		float32 d0 = linePlaneDistance(start, camera, plane);
		float32 d1 = linePlaneDistance(end, camera, plane);
		if (d0 < 0 && d1 < 0) return 0;
		if ((d0 < 0) != (d1 < 0)) {
			float32 t = d0 / (d0 - d1);
			gre_fvector4d point = {
				start->x + (end->x - start->x) * t,
				start->y + (end->y - start->y) * t,
				start->z + (end->z - start->z) * t, 1
			};
			if (d0 < 0) *start = point; else *end = point;
		}
	}
	return 1;
}

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
			if (!thisobj->isVisible) continue;
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
			if (!thisobj->isVisible) continue;
			//工作区只需扩展到当前物体规模，后续物体继续复用同一块缓存
			YMGRE_RenderWorkspace_Reserve(workspace, thisobj->pointNum, thisobj->polygonNum, lightNum);
			//世界坐标变换结果写入工作区，不修改共享物体的 pointList_
			YMGRE_Object_WorldToCameraTo(thisobj, &mycam->move.TMat, workspace->pointList);
			if (!YMGRE_Object_FrustumCullingCal(thisobj, mycam))
			{
				//逐面剔除和光照结果均按 polygon 索引写入工作区
				YMGRE_Backface_RemoveTo(thisobj, &mycam->pos, workspace->polygonHide);
				YMGRE_ObjectLighting_ColorTo(thisobj, workspace->pointList, LightList,
					workspace->lightPos, &mycam->move.TMat, workspace->polygonColor);
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
			if (!thisobj->isVisible) continue;
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
			if (!thisobj->isVisible) continue;
			//每次只保留当前物体的临时结果，工作区容量按最大物体复用
			YMGRE_RenderWorkspace_Reserve(workspace, thisobj->pointNum, thisobj->polygonNum, lightNum);
			//世界坐标变换结果写入工作区，不修改共享物体的 pointList_
			YMGRE_Object_WorldToCameraTo(thisobj, &mycam->move.TMat, workspace->pointList);
			if (!YMGRE_Object_FrustumCullingCal(thisobj, mycam))
			{
				//背面状态和逐面光照颜色均保存在当前工作区
				YMGRE_Backface_RemoveTo(thisobj, &mycam->pos, workspace->polygonHide);
				YMGRE_ObjectLighting_ColorTo(thisobj, workspace->pointList, LightList,
					workspace->lightPos, &mycam->move.TMat, workspace->polygonColor);
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

void YMGRE_Camera_TanglePipline_VertexColor_wN(GRE_Camera4d thiscam,
	GRE_List ObjList,GRE_RenderWorkspace workspace)
{
	gre_log_explain((thiscam==NULL)||(ObjList==NULL),GRE_LOG_PtrI,"顶点色管线输入不存在");
	if(thiscam==NULL||ObjList==NULL) return;
	if(workspace==NULL) workspace=thiscam->workspace;
	if(workspace==NULL) return;
	GRE_RenderTarget target=YMGRE_Camera_GetRenderTarget(thiscam);
	if(target==NULL) return;
	gre_camera4d renderCamera=*thiscam;
	renderCamera.img=*target;
	renderCamera.target=target;
	GRE_Camera4d cam=&renderCamera;
	YMGRE_CameraImage_Init(cam,(GRErgb24){50,50,50});
	for(GRE_ListNode node=ObjList->listhead;node!=NULL;node=node->next)
	{
		GRE_Object4d object=node->data;
		for(;object!=NULL;object=object->nextObject)
		{
			if(!object->isVisible||object->pointList_wN==NULL) continue;
			YMGRE_RenderWorkspace_Reserve(workspace,object->pointNum,object->polygonNum,0);
			if(!YMGRE_RenderWorkspace_EnableVertexAttributes(workspace,object->pointNum)) continue;
			YMGRE_Object_WorldToCameraTo_wN(object,&cam->move.TMat,workspace->pointList_wN);
			if(YMGRE_Object_FrustumCullingCal(object,cam)) continue;
			YMGRE_Backface_RemoveTo(object,&cam->pos,workspace->polygonHide);
			YMGRE_VertexList_CameraToViewPlane_wN(workspace->pointList_wN,
				object->pointNum,cam->perspectPlane.Dis);
			YMGRE_ObjectPoly_FrustumCullingTo_wN(object,workspace->pointList_wN,
				cam,workspace->polygonHide);
			YMGRE_VertexList_ViewPlaneToWindows_wN(workspace->pointList_wN,
				object->pointNum,cam);
			if(cam->wireFrame==GRE_Render_Wireframe)
				YMGRE_TrangleObject_Wires_wN(object,workspace->pointList_wN,
					workspace->polygonHide,cam);
			else
				YMGRE_TrangleObject_Primitive_Rasterization_VertexColor_wN(object,
					workspace->pointList_wN,workspace->polygonHide,cam);
		}
	}
}

// Advanced material pipeline. Legacy pipeline entry points intentionally remain untouched.
void YMGRE_Camera_TanglePipline_wN(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList,
	GRE_RenderWorkspace workspace)
{
	gre_log_explain((thiscam==NULL)||(LightList==NULL)||(ObjList==NULL)||(MaterialList==NULL),
		GRE_LOG_PtrI,"高级材质管线输入不存在");
	if(thiscam==NULL||LightList==NULL||ObjList==NULL||MaterialList==NULL) return;
	if (workspace == NULL) workspace = thiscam->workspace;
	if (workspace == NULL) return;
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(thiscam);
	if (target == NULL) return;
	gre_camera4d renderCamera = *thiscam;
	renderCamera.img = *target;
	renderCamera.target = target;
	GRE_Camera4d cam = &renderCamera;
	YMGRE_CameraImage_Init(cam, (GRErgb24){ 50, 50, 50 });
	uint32 lightNum = 0;
	for(GRE_ListNode n=LightList->listhead;n!=NULL;n=n->next) lightNum++;
	YMGRE_RenderWorkspace_Reserve(workspace, 0, 0, lightNum);
	uint32 li = 0;
	for(GRE_ListNode n=LightList->listhead;n!=NULL;n=n->next)
	{
		GRE_Light4d light = n->data;
		YMGRE_Point_WorldToCamera(&light->pos, &workspace->lightPos[li++], &cam->move.TMat);
	}
	for (GRE_ListNode node = ObjList->listhead; node != NULL; node = node->next)
	{
		GRE_Object4d object = node->data;
		for (; object != NULL; object = object->nextObject)
		{
			if (!object->isVisible || object->pointList_wN == NULL) continue;
			YMGRE_RenderWorkspace_Reserve(workspace, object->pointNum, object->polygonNum, lightNum);
			if (!YMGRE_RenderWorkspace_EnableVertexAttributes(workspace, object->pointNum)) continue;
			YMGRE_Object_WorldToCameraTo_wN(object, &cam->move.TMat, workspace->pointList_wN);
			if (YMGRE_Object_FrustumCullingCal(object, cam)) continue;
			YMGRE_Backface_RemoveTo(object, &cam->pos, workspace->polygonHide);
			if (cam->wireFrame == GRE_Render_Wireframe)
			{
				YMGRE_VertexList_CameraToViewPlane_wN(workspace->pointList_wN, object->pointNum, cam->perspectPlane.Dis);
				YMGRE_VertexList_ViewPlaneToWindows_wN(workspace->pointList_wN, object->pointNum, cam);
				YMGRE_TrangleObject_Wires_wN(object, workspace->pointList_wN, workspace->polygonHide, cam);
			}
			else
			{
				GRE_Material material = YMGRE_Material_Find(MaterialList, object->materiaName);
				for (int pi = 0; pi < object->polygonNum; pi++)
				{
					GRE_Polygon4d source = &object->polygonList[pi];
					if (workspace->polygonHide[pi] || source->num != 3) continue;
					gre_vertex4d_wN input[3], clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
					for (int vi = 0; vi < 3; vi++) input[vi] = workspace->pointList_wN[source->index[vi]];
					if (object->renderMode == GRE_RenderMode_Face)
					{
						gre_fvector4d faceNormal = source->pN;
						faceNormal.w = 0;
						YMGRE_Fvector4d_MatMultTo(&cam->move.TMat, &faceNormal, &faceNormal);
						YMGRE_Fvector4d_Normalize(&faceNormal);
						for (int vi = 0; vi < 3; vi++) input[vi].normal = faceNormal;
					}
					else if (object->renderMode == GRE_RenderMode_Vertex)
					{
						YMGRE_TriangleRaster_ComputeVertexLighting_wN(input, 3, source,
							material, LightList, workspace->lightPos, &cam->move.TMat,
							object->mirrorKs);
					}
					uint16 count = YMGRE_Polygon_FrustumClip_wN(input, 3, clipped,
						YMGRE_FRUSTUM_CLIP_VERTEX_MAX, cam);
					if (count < 3) continue;
					YMGRE_VertexList_CameraToViewPlane_wN(clipped, count, cam->perspectPlane.Dis);
					YMGRE_VertexList_ViewPlaneToWindows_wN(clipped, count, cam);
					for (uint16 vi = 1; vi + 1 < count; vi++)
					{
						uint16 indices[3] = { 0, vi, (uint16)(vi + 1) };
						gre_polygon4d triangle = *source;
						triangle.num = 3;
						triangle.index = indices;
						if (object->renderMode == GRE_RenderMode_Vertex)
							YMGRE_TriangleRaster_FillVertexLit_wN(clipped, &triangle, material, cam);
						else
							YMGRE_TriangleRaster_Fill_wN(clipped, &triangle, material, LightList,
								workspace->lightPos, &cam->move.TMat, object->mirrorKs, cam);
					}
				}
			}
		}
	}
}

//独立3D线段在场景之后叠加到同一目标，不参与三角形背面剔除
void YMGRE_Camera_LineList_Rendering(GRE_Camera4d camera, const gre_line3d* lines,
	uint32 lineNum, uint8 depthTest)
{
	gre_log_explain((camera == NULL) || ((lineNum > 0) && (lines == NULL)), GRE_LOG_PtrIO,
		"线段相机或列表不存在");
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(camera);
	gre_camera4d renderCamera = *camera;
	renderCamera.img = *target;
	renderCamera.target = target;
	for (uint32 i = 0; i < lineNum; i++) {
		gre_fvector4d start, end;
		YMGRE_Fvector4d_MatMultTo(&camera->move.TMat, (GRE_Fvector4d)&lines[i].start, &start);
		YMGRE_Fvector4d_MatMultTo(&camera->move.TMat, (GRE_Fvector4d)&lines[i].end, &end);
		if (!clipCameraLine(&start, &end, &renderCamera)) continue;
		float32 z1 = start.z, z2 = end.z;
		YMGRE_Point_CameraToViewPlane(&start, renderCamera.perspectPlane.Dis);
		YMGRE_Point_CameraToViewPlane(&end, renderCamera.perspectPlane.Dis);
		YMGRE_Point_ViewPlaneToWindows(&start, &renderCamera);
		YMGRE_Point_ViewPlaneToWindows(&end, &renderCamera);
		int32 x1 = (int32)(start.x + 0.5f), y1 = (int32)(start.y + 0.5f);
		int32 x2 = (int32)(end.x + 0.5f), y2 = (int32)(end.y + 0.5f);
		if (x1 < 0) x1 = 0; if (x1 >= target->width) x1 = target->width - 1;
		if (x2 < 0) x2 = 0; if (x2 >= target->width) x2 = target->width - 1;
		if (y1 < 0) y1 = 0; if (y1 >= target->height) y1 = target->height - 1;
		if (y2 < 0) y2 = 0; if (y2 >= target->height) y2 = target->height - 1;
		uint8 thickness=lines[i].thickness?lines[i].thickness:1;
		int half=(int)thickness/2; int32 dx=x2-x1,dy=y2-y1; float32 len=sqrtf((float32)dx*dx+(float32)dy*dy);
		float32 nx=len>0.0f?-(float32)dy/len:0.0f, ny=len>0.0f?(float32)dx/len:1.0f;
		for(int offset=-half;offset<=half;++offset){
			int16 ox1=(int16)(x1+nx*offset),oy1=(int16)(y1+ny*offset),ox2=(int16)(x2+nx*offset),oy2=(int16)(y2+ny*offset);
			YMGRE_Img_LineDepth(target->data, target->zbuff, target->width, target->height,
				ox1, oy1, z1, ox2, oy2, z2, lines[i].color, depthTest);
		}
	}
}
