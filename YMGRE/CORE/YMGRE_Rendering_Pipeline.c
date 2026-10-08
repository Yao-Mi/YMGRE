#include "../CONFIG/YMGRE_Profile.h"
#include "YMGRE_Color.h"
#include "YMGRE_PBR.h"
#include "../CONFIG/YMGRE_Mem.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

static uint8 sphereInsideFrustum(GRE_Object4d object, GRE_Camera4d cam)
{
	if (object->boundType != GRE_Bounding_Sphere_R || object->BoundingSphereR < 0) return 0;
	float32 radius = YMGRE_Fabs(object->scale) * object->BoundingSphereR;
	gre_fvector4d center;
	YMGRE_Fvector4d_MatMultTo(&cam->move.TMat, &object->WorldCoordinate, &center);
	for (uint8 plane = 0; plane < 6; plane++) {
		float32 nx = 0, ny = 0, nz = 0;
		switch (plane) {
		case 0: nz = 1; break; case 1: nz = -1; break;
		case 2: nx = 1; nz = -cam->perspectPlane.kl; break;
		case 3: nx = -1; nz = cam->perspectPlane.kr; break;
		case 4: ny = 1; nz = -cam->perspectPlane.kd; break;
		default: ny = -1; nz = cam->perspectPlane.ku; break;
		}
		float32 x = nx*cam->move.TMat.val[0][0] + ny*cam->move.TMat.val[1][0] + nz*cam->move.TMat.val[2][0];
		float32 y = nx*cam->move.TMat.val[0][1] + ny*cam->move.TMat.val[1][1] + nz*cam->move.TMat.val[2][1];
		float32 z = nx*cam->move.TMat.val[0][2] + ny*cam->move.TMat.val[1][2] + nz*cam->move.TMat.val[2][2];
		if (linePlaneDistance(&center, cam, plane) < radius*YMGRE_Sqrt(x*x+y*y+z*z)+1e-4f) return 0;
	}
	return 1;
}

static void prepareProjectedVertices(GRE_RenderWorkspace workspace, uint32 count,
	GRE_Camera4d cam, uint8 allInside)
{
	float32 sx = cam->img.width/(cam->perspectPlane.pR-cam->perspectPlane.pL);
	float32 sy = cam->img.height/(cam->perspectPlane.pU-cam->perspectPlane.pD);
	float32 ox = cam->img.width/2.0f, oy = cam->img.height/2.0f;
	for (uint32 i = 0; i < count; i++) {
		if (!workspace->clipCodes[i]) { workspace->clipCodes[i] = 0x80; continue; }
		gre_fvector4d pos = workspace->pointList_wN[i].base.pos;
		uint8 code = 0;
		if (!allInside)
			for (uint8 plane = 0; plane < 6; plane++)
				if (linePlaneDistance(&pos, cam, plane) < 0) code |= (uint8)(1u << plane);
		workspace->clipCodes[i] = code;
		if (code == 0) {
			float32 scale = cam->perspectPlane.Dis/pos.z;
			pos.x *= scale;
			pos.y *= scale;
			pos.x = pos.x*sx + ox;
			pos.y = pos.y*-sy + oy;
			workspace->projectedPoints[i] = pos;
		}
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

/* Rasterize camera-space triangles only after clipping. A vertex behind the
 * camera must not make the visible portion of its face disappear. */
static void renderCameraTriangles(GRE_Object4d object, GRE_Vertex4d points,
	const uint8* hidden, const GRErgb24* colors, GRE_Material material, GRE_Camera4d camera)
{
	if (camera->wireFrame != GRE_Render_Wireframe)
	for (int pi = 0; pi < object->polygonNum; pi++)
	{
		GRE_Polygon4d source = &object->polygonList[pi];
		if ((hidden ? hidden[pi] : source->ishide) || source->num != 3) continue;
		gre_vertex4d input[3], clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
		for (int j = 0; j < 3; j++) input[j] = points[source->index[j]];
		uint16 count = YMGRE_Polygon_FrustumClip(input, 3, clipped,
			YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera);
		if (count < 3) continue;
		YMGRE_VertexList_CameraToViewPlane(clipped, count, camera->perspectPlane.Dis);
		YMGRE_VertexList_ViewPlaneToWindows(clipped, count, camera);
		/* Roundoff at an exact clipping boundary must not move a coverage edge
		 * a tiny positive distance inward (ceil would drop the first pixel). */
		for (uint16 j = 0; j < count; j++) {
			if (fabsf(clipped[j].pos.x) < .0001f) clipped[j].pos.x = 0;
			if (fabsf(clipped[j].pos.y) < .0001f) clipped[j].pos.y = 0;
			if (fabsf(clipped[j].pos.x-camera->img.width) < .0001f) clipped[j].pos.x = camera->img.width;
			if (fabsf(clipped[j].pos.y-camera->img.height) < .0001f) clipped[j].pos.y = camera->img.height;
		}
		GRErgb24 color = colors ? colors[pi] : source->planeColor_;
		if (material && material->unlit) color = (GRErgb24){DefaultPolygonClv,DefaultPolygonClv,DefaultPolygonClv};
		for (uint16 j = 1; j + 1 < count; j++)
		{
			GRE_Index indices[3] = {0, j, (GRE_Index)(j+1)};
			gre_polygon4d triangle = *source;
			triangle.index = indices;
			YMGRE_TriangleRaster_Fill(clipped, &triangle, color, material, camera);
		}
	}
	/* Draw only original mesh edges, after all fills. Clipping-generated fan
	 * diagonals and screen-border segments are not geometry edges. */
	if (camera->wireFrame == GRE_Render_Wireframe || object->wireFrame)
	for (int pi = 0; pi < object->polygonNum; pi++)
	{
		GRE_Polygon4d polygon = &object->polygonList[pi];
		if ((hidden ? hidden[pi] : polygon->ishide) || polygon->num != 3) continue;
		/* Integer line samples need the same face depth as filled pixels. Using
		 * interpolated endpoint depths after snapping x/y causes self-occlusion. */
		gre_vertex4d input[3], clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
		for (int j = 0; j < 3; j++) input[j] = points[polygon->index[j]];
		uint16 count = YMGRE_Polygon_FrustumClip(input, 3, clipped,
			YMGRE_FRUSTUM_CLIP_VERTEX_MAX, camera);
		if (count < 3) continue;
		YMGRE_VertexList_CameraToViewPlane(clipped, count, camera->perspectPlane.Dis);
		YMGRE_VertexList_ViewPlaneToWindows(clipped, count, camera);
		gre_fvector4d depthPlane;
		uint8 hasPlane = 0;
		for (uint16 j = 1; j + 1 < count && !hasPlane; j++) {
			GRE_Index indices[3] = {0, j, (GRE_Index)(j + 1)};
			gre_polygon4d triangle = *polygon;
			triangle.index = indices;
			hasPlane = YMGRE_Triangle_DepthPlane(clipped, &triangle, &depthPlane);
		}
		if (!hasPlane) continue;
		for (int j = 0; j < 3; j++)
		{
			gre_fvector4d a = points[polygon->index[j]].pos;
			gre_fvector4d b = points[polygon->index[(j+1)%3]].pos;
			if (!clipCameraLine(&a, &b, camera)) continue;
			YMGRE_Point_CameraToViewPlane(&a, camera->perspectPlane.Dis);
			YMGRE_Point_CameraToViewPlane(&b, camera->perspectPlane.Dis);
			YMGRE_Point_ViewPlaneToWindows(&a, camera);
			YMGRE_Point_ViewPlaneToWindows(&b, camera);
			int x0=GREMax(0,GREMin(camera->img.width-1,(int)(a.x+.5f)));
			int y0=GREMax(0,GREMin(camera->img.height-1,(int)(a.y+.5f)));
			int x1=GREMax(0,GREMin(camera->img.width-1,(int)(b.x+.5f)));
			int y1=GREMax(0,GREMin(camera->img.height-1,(int)(b.y+.5f)));
			YMGRE_Img_LineDepthPlane(camera->img.data, camera->img.width, camera->img.zbuff,
				&depthPlane, x0, y0, x1, y1);
		}
	}
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

				GRE_Material material = YMGRE_Material_Find(MaterialList, thisobj->materiaName);
				renderCameraTriangles(thisobj, thisobj->pointList_, NULL, NULL, material, thiscam);
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
				GRE_Material material = YMGRE_Material_Find(MaterialList, thisobj->materiaName);
				renderCameraTriangles(thisobj, workspace->pointList, workspace->polygonHide,
					workspace->polygonColor, material, mycam);
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

#if YMGRE_ENABLE_TRANSPARENCY
typedef struct {
	gre_vertex4d_wN vertices[3];
	gre_polygon4d polygon;
	GRE_Material material;
	GRE_Lightmap lightmap;
	float32 depth, mirrorKs;
	int renderMode;
 int minY,maxY;
	size_t sequence;
} GRE_TransparentTriangle;

static int compareTransparentTriangles(const void* lhs, const void* rhs)
{
	const GRE_TransparentTriangle* a = lhs;
	const GRE_TransparentTriangle* b = rhs;
	if (a->depth > b->depth) return -1;
	if (a->depth < b->depth) return 1;
	return (a->sequence > b->sequence) - (a->sequence < b->sequence);
}

/* Sort compact indices in reserved workspace storage, never whole triangles.
   No hidden allocation; the sequence key preserves deterministic equal depths. */
static void siftTransparent(const GRE_TransparentTriangle* packets,size_t* order,size_t count,size_t root)
{
 size_t value=order[root];
 while(root<count/2){
  size_t child=root*2+1;
  if(child+1<count&&compareTransparentTriangles(&packets[order[child]],&packets[order[child+1]])<0)child++;
  if(compareTransparentTriangles(&packets[value],&packets[order[child]])>=0)break;
  order[root]=order[child];root=child;
 }
 order[root]=value;
}
static void sortTransparent(const GRE_TransparentTriangle* packets,size_t* order,size_t count)
{
 for(size_t i=0;i<count;i++)order[i]=i;
 if(count<2)return;
 for(size_t root=count/2;root>0;)siftTransparent(packets,order,count,--root);
 for(size_t end=count-1;end>0;end--){
  size_t swap=order[0];order[0]=order[end];order[end]=swap;
  siftTransparent(packets,order,end,0);
 }
}

static void drawOpacityPacket(GRE_TransparentTriangle* packet, GRE_Camera4d cam,
    GRE_List LightList, GRE_RenderWorkspace workspace)
{
	GRE_Index indices[3] = {0, 1, 2};
	packet->polygon.index = indices;
	#if YMGRE_ENABLE_PBR
	if (cam->pbrEnabled && !packet->material->unlit && !packet->lightmap && packet->material->advanced && packet->material->advanced->pbrEnabled)
		YMGRE_TriangleRaster_Fill_wN(packet->vertices, &packet->polygon,
			packet->material, LightList, workspace->lightPos, &cam->move.TMat,
			packet->mirrorKs, cam);
	else
#endif
	if (packet->lightmap)
		YMGRE_TriangleRaster_FillLightmap_wN(packet->vertices, &packet->polygon,
			packet->material, packet->lightmap, cam);
	else if (packet->renderMode == GRE_RenderMode_Vertex && !packet->material->unlit)
		YMGRE_TriangleRaster_FillVertexLit_wN(packet->vertices, &packet->polygon,
			packet->material, cam);
	else
		YMGRE_TriangleRaster_Fill_wN(packet->vertices, &packet->polygon,
			packet->material, LightList, workspace->lightPos, &cam->move.TMat,
			packet->mirrorKs, cam);
}

#if YMGRE_ENABLE_RASTER_DISPATCH
/* Each job owns disjoint rows. Geometry, lights and packets are read-only;
   depth and transparent blending retain exactly the serial submission order. */
typedef struct {
 GRE_TransparentTriangle* packets;size_t count;
 GRE_Camera4d camera;GRE_List lights;GRE_RenderWorkspace workspace;
 int pass;
 const size_t* order;
} GRE_RasterBatch;
static void drawPBRBand(void* context,uint32 jobIndex)
{
 GRE_RasterBatch* batch=context;
 gre_camera4d camera=*batch->camera;
 int begin=(int)jobIndex*16,end=GREMin(begin+16,camera.img.height);
 camera.opacityPass=batch->pass==2?2:1;
 GRE_Index indices[3]={0,1,2};
 for(size_t i=0;i<batch->count;i++){
  GRE_TransparentTriangle* packet=&batch->packets[batch->order?batch->order[i]:i];
  int alpha=packet->material->advanced&&packet->material->advanced->opacityPixel;
  if((batch->pass==0?alpha:!alpha)||packet->maxY<begin||packet->minY>=end)continue;
  gre_polygon4d polygon=packet->polygon;polygon.index=indices;
  YMGRE_PBR_FillRows(packet->vertices,&polygon,packet->material,batch->lights,
   batch->workspace->lightPos,&camera.move.TMat,&camera,begin,end);
 }
}
static int canDispatchPBR(GRE_Camera4d cam,GRE_List objects,GRE_List materials,GRE_RenderWorkspace ws)
{
 if(!ws->rasterDispatch||!cam->pbrEnabled||cam->wireFrame==GRE_Render_Wireframe)return 0;
 for(GRE_ListNode node=objects->listhead;node;node=node->next)
  for(GRE_Object4d obj=node->data;obj;obj=obj->nextObject){
   if(!obj->isVisible||!obj->pointList_wN)continue;
   GRE_Material mat=YMGRE_Material_Find(materials,obj->materiaName);
   if(obj->wireFrame||obj->lightmap||!mat||mat->unlit||!mat->advanced||!mat->advanced->pbrEnabled)return 0;
  }
 return 1;
}
#endif

#endif

// Advanced material pipeline; legacy entry points remain unchanged.
#if YMGRE_PROFILE_RENDER_STAGES
#define renderAdvancedBatch renderAdvancedBatch_Measured
#endif
static void renderAdvancedBatch(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList,
	GRE_RenderWorkspace workspace,uint8 clearTarget)
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
	workspace->materialStatus=0;
#if YMGRE_ENABLE_LINEAR_COLOR
 if(cam->wireFrame==GRE_Render_Wireframe)cam->linearColorEnabled=0;
 if(!YMGRE_Color_BeginFrame(cam,workspace,(GRErgb24){50,50,50})){workspace->materialStatus=-1;return;}
#endif
 if(clearTarget)YMGRE_CameraImage_Init(cam, (GRErgb24){50,50,50});
#if YMGRE_ENABLE_TRANSPARENCY
 cam->opacityPass=1;
 GRE_TransparentTriangle* transparent=workspace->transparentTriangles;
 size_t opaqueCount=0;
 size_t packetStride=sizeof(*transparent)+sizeof(size_t);
 size_t transparentCount=0,transparentCapacity=workspace->transparentCapacity/packetStride;
 int parallelPBR=0;
#if YMGRE_ENABLE_RASTER_DISPATCH
 parallelPBR=canDispatchPBR(cam,ObjList,MaterialList,workspace);
#endif
#endif
 int deferWires=0;
#if YMGRE_ENABLE_LINEAR_COLOR
 deferWires=cam->linearColorEnabled;
#endif
#if YMGRE_ENABLE_TRANSPARENCY
 for(GRE_ListNode n=MaterialList?MaterialList->listhead:NULL;n;n=n->next){
  GRE_Material m=n->data;if(m&&m->advanced&&m->advanced->opacityPixel){deferWires=1;break;}
 }
#endif
	uint32 lightNum = 0;
	for(GRE_ListNode n=LightList->listhead;n!=NULL;n=n->next) lightNum++;
	YMGRE_RenderWorkspace_Reserve(workspace, 0, 0, lightNum);
	uint32 li = 0;
	for(GRE_ListNode n=LightList->listhead;n!=NULL;n=n->next)
	{
		GRE_Light4d light = n->data;
		YMGRE_Point_WorldToCamera(&light->pos, &workspace->lightPos[li++], &cam->move.TMat);
	}
 int materialGroups=1;
#if YMGRE_ENABLE_TRANSPARENCY
 if(parallelPBR)materialGroups=2;
#endif
 for(int materialGroup=0;materialGroup<materialGroups;materialGroup++){
	for (GRE_ListNode node = ObjList->listhead; node != NULL; node = node->next)
	{
		GRE_Object4d object = node->data;
		for (; object != NULL; object = object->nextObject)
		{
			if (!object->isVisible || object->pointList_wN == NULL) continue;
            GRE_Material material = YMGRE_Material_Find(MaterialList, object->materiaName);
#if YMGRE_ENABLE_TRANSPARENCY
            if(parallelPBR){int alpha=material&&material->advanced&&material->advanced->opacityPixel;if(alpha!=materialGroup)continue;}
#endif
			YMGRE_RenderWorkspace_Reserve(workspace, object->pointNum, object->polygonNum, lightNum);
			if (!YMGRE_RenderWorkspace_EnableVertexAttributes(workspace, object->pointNum)) continue;
			if (YMGRE_Object_FrustumCullingCal(object, cam)) continue;
			uint8 projectedReady = YMGRE_RenderWorkspace_EnableProjectionCache(workspace, object->pointNum);
			uint8* active = projectedReady ? workspace->clipCodes : NULL;
			if (material && material->doubleSided) {
				memset(workspace->polygonHide, 0, object->polygonNum);
				if (active) memset(active, 1, object->pointNum);
			} else YMGRE_Backface_RemoveAndMarkTo(object, &cam->pos,
				workspace->polygonHide, active, object->wireFrame || cam->wireFrame == GRE_Render_Wireframe, 0);
			YMGRE_Object_WorldToCameraMaskedTo(object, &cam->move.TMat,
				workspace->pointList_wN, 1, active);
			/* Camera wireframe is exclusive; object wireFrame is an overlay on solid shading. */
			if (cam->wireFrame == GRE_Render_Wireframe)
			{
				YMGRE_VertexList_CameraToViewPlane_wN(workspace->pointList_wN, object->pointNum, cam->perspectPlane.Dis);
				YMGRE_VertexList_ViewPlaneToWindows_wN(workspace->pointList_wN, object->pointNum, cam);
				YMGRE_TrangleObject_Wires_wN(object, workspace->pointList_wN, workspace->polygonHide, cam);
			}
			else
			{
				GRE_Lightmap lightmap=object->lightmap;
				if (lightmap && (!lightmap->enabled || !lightmap->pixels || !lightmap->uv1 ||
					lightmap->triangleCount != (uint32)object->polygonNum)) lightmap=NULL;
				/* Single-sided vertex lighting is independent of the incident triangle.
				   Double-sided faces can flip normals, and UV1/lightmaps are per face. */
				uint8 vertexLightingReady = object->renderMode == GRE_RenderMode_Vertex &&
					material && !material->doubleSided && !material->unlit && !lightmap &&
					object->pointNum > 0 && object->pointNum <= UINT16_MAX && object->polygonNum > 0;
				if (vertexLightingReady) {
					uint32 visible = 0;
					for (int pi = 0; pi < object->polygonNum; pi++)
						if (!workspace->polygonHide[pi] && object->polygonList[pi].num == 3) visible++;
					vertexLightingReady = visible >= ((uint32)object->pointNum + 2) / 3;
					if (vertexLightingReady)
						YMGRE_TriangleRaster_ComputeVertexLightingMasked_wN(workspace->pointList_wN,
							(uint16)object->pointNum, &object->polygonList[0], material,
							LightList, workspace->lightPos, &cam->move.TMat, object->mirrorKs, active);
				}
				if (projectedReady)
					prepareProjectedVertices(workspace, object->pointNum, cam, sphereInsideFrustum(object, cam));
				for (int pi = 0; pi < object->polygonNum; pi++)
				{
					GRE_Polygon4d source = &object->polygonList[pi];
					if (workspace->polygonHide[pi] || source->num != 3) continue;
					uint8 backFacing = 0;
					if (material && material->doubleSided) {
						gre_fvector4d viewVector;
						YMGRE_Fvector4d_SubToResult(&cam->pos,
							&object->pointList[source->index[0]].pos, &viewVector);
						backFacing = YMGRE_Fvector4d_Dot(&source->pN, &viewVector) <= 0.0f;
					}
					gre_vertex4d_wN input[3], clipped[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
					for (int vi = 0; vi < 3; vi++) {
						input[vi] = workspace->pointList_wN[source->index[vi]];
						if (backFacing) {
							input[vi].normal.x = -input[vi].normal.x;
							input[vi].normal.y = -input[vi].normal.y;
							input[vi].normal.z = -input[vi].normal.z;
						}
						if (lightmap) {
							input[vi].lightmapU=lightmap->uv1[pi*6+vi*2];
							input[vi].lightmapV=lightmap->uv1[pi*6+vi*2+1];
						}
					}
					if (object->renderMode == GRE_RenderMode_Face)
					{
						gre_fvector4d faceNormal = source->pN;
						faceNormal.w = 0;
                        if(backFacing){faceNormal.x=-faceNormal.x;faceNormal.y=-faceNormal.y;faceNormal.z=-faceNormal.z;}
						YMGRE_Fvector4d_MatMultTo(&cam->move.TMat, &faceNormal, &faceNormal);
						YMGRE_Fvector4d_Normalize(&faceNormal);
						for (int vi = 0; vi < 3; vi++) input[vi].normal = faceNormal;
					}
					else if (object->renderMode == GRE_RenderMode_Vertex && !vertexLightingReady)
					{
						YMGRE_TriangleRaster_ComputeVertexLighting_wN(input, 3, source,
							material, LightList, workspace->lightPos, &cam->move.TMat,
							object->mirrorKs);
					}
					uint16 count;
					uint8 codes = 0xff;
					if (projectedReady) {
						uint8 a = workspace->clipCodes[source->index[0]];
						uint8 b = workspace->clipCodes[source->index[1]];
						uint8 c = workspace->clipCodes[source->index[2]];
						if (a & b & c) continue;
						codes = a | b | c;
					}
					if (codes == 0) {
						count = 3;
						for (int vi = 0; vi < 3; vi++) {
							clipped[vi] = input[vi];
							clipped[vi].base.pos = workspace->projectedPoints[source->index[vi]];
						}
					} else {
						count = YMGRE_Polygon_FrustumClip_wN(input, 3, clipped,
							YMGRE_FRUSTUM_CLIP_VERTEX_MAX, cam);
						if (count < 3) continue;
						YMGRE_VertexList_CameraToViewPlane_wN(clipped, count, cam->perspectPlane.Dis);
						YMGRE_VertexList_ViewPlaneToWindows_wN(clipped, count, cam);
					}
					for (uint16 vi = 1; vi + 1 < count; vi++)
					{
						GRE_Index indices[3] = { 0, vi, (GRE_Index)(vi + 1) };
						gre_polygon4d triangle = *source;
						triangle.num = 3;
						triangle.index = indices;
#if YMGRE_ENABLE_TRANSPARENCY
						if (parallelPBR || (material && material->advanced && material->advanced->opacityPixel)) {
							if (transparentCount == transparentCapacity) {
								size_t capacity = transparentCapacity ? transparentCapacity * 2 : 1024;
								if(!workspace->ownsMemory || capacity>SIZE_MAX/packetStride){workspace->materialStatus=-1;return;}
                                void* grown=GRE_RenderBuff_Malloc(capacity*packetStride);
                                if(!grown){workspace->materialStatus=-1;return;}
                                if(transparentCount)memcpy(grown,transparent,transparentCount*sizeof(*transparent));
                                GRE_RenderBuff_Free(transparent);
                                workspace->transparentTriangles=grown;workspace->transparentCapacity=capacity*packetStride;
								transparent = grown;
								transparentCapacity = capacity;
							}
							GRE_TransparentTriangle* packet = &transparent[transparentCount];
							packet->vertices[0] = clipped[0];
							packet->vertices[1] = clipped[vi];
							packet->vertices[2] = clipped[vi+1];
							packet->polygon = triangle;
							packet->polygon.index = NULL;
							packet->material = material;
							packet->lightmap = lightmap;
							packet->mirrorKs = object->mirrorKs;
							packet->renderMode = object->renderMode;
                            packet->minY=(int)floorf(fminf(clipped[0].base.pos.y,fminf(clipped[vi].base.pos.y,clipped[vi+1].base.pos.y)));
                            packet->maxY=(int)ceilf(fmaxf(clipped[0].base.pos.y,fmaxf(clipped[vi].base.pos.y,clipped[vi+1].base.pos.y)));
							packet->depth = (clipped[0].base.pos.z + clipped[vi].base.pos.z +
								clipped[vi+1].base.pos.z) / 3.0f;
							packet->sequence = transparentCount++;
                            if(parallelPBR&&!materialGroup)opaqueCount++;
							/* Group 2 is deferred until all no-opacity-map geometry has drawn. */
							continue;
						}
#endif
						#if YMGRE_ENABLE_PBR
						if (cam->pbrEnabled && material && !material->unlit && !lightmap && material->advanced && material->advanced->pbrEnabled)
							YMGRE_TriangleRaster_Fill_wN(clipped, &triangle, material, LightList,
								workspace->lightPos, &cam->move.TMat, object->mirrorKs, cam);
						else
#endif
						if (lightmap)
							YMGRE_TriangleRaster_FillLightmap_wN(clipped, &triangle, material, lightmap, cam);
						else if (object->renderMode == GRE_RenderMode_Vertex && !(material && material->unlit))
							YMGRE_TriangleRaster_FillVertexLit_wN(clipped, &triangle, material, cam);
						else
							YMGRE_TriangleRaster_Fill_wN(clipped, &triangle, material, LightList,
								workspace->lightPos, &cam->move.TMat, object->mirrorKs, cam);
					}
				}
			}
            if(!deferWires)
			if (object->wireFrame && cam->wireFrame != GRE_Render_Wireframe)
			{
				YMGRE_VertexList_CameraToViewPlane_wN(workspace->pointList_wN,
					object->pointNum, cam->perspectPlane.Dis);
				YMGRE_VertexList_ViewPlaneToWindows_wN(workspace->pointList_wN,
					object->pointNum, cam);
				YMGRE_TrangleObject_Wires_wN(object, workspace->pointList_wN,
					workspace->polygonHide, cam);
			}
		}
	}
 }
#if YMGRE_ENABLE_TRANSPARENCY
 size_t* order=transparent?(size_t*)(transparent+transparentCapacity):NULL;
#if YMGRE_ENABLE_RASTER_DISPATCH
 if(parallelPBR){
  GRE_RasterBatch batch={transparent,opaqueCount,cam,LightList,workspace,0,NULL};
  uint32 jobs=(cam->img.height+15)/16;
  workspace->rasterDispatch(workspace->rasterDispatchUser,drawPBRBand,&batch,jobs);
  batch.pass=1;batch.packets=transparent?transparent+opaqueCount:NULL;batch.count=transparentCount-opaqueCount;
  workspace->rasterDispatch(workspace->rasterDispatchUser,drawPBRBand,&batch,jobs);
  sortTransparent(batch.packets,order,batch.count);
  batch.pass=2;batch.order=order;
  workspace->rasterDispatch(workspace->rasterDispatchUser,drawPBRBand,&batch,jobs);
 }else
#endif
 {
	/* Group 1 (materials without an opacity map) is complete. Group 2 keeps
	   alpha==255 depth establishment separate from fractional-alpha blending,
	   so intersecting hair cards retain the previous occlusion semantics.
	   Preserve submission order for this depth subpass, including equal depths. */
		for (size_t i = 0; i < transparentCount; i++)
			drawOpacityPacket(&transparent[i], cam, LightList, workspace);
	/* All opaque depth is now established. Blend fractional coverage globally,
	   back to front, without writing opaque depth. */
	sortTransparent(transparent, order, transparentCount);
	cam->opacityPass = 2;
	for (size_t i = 0; i < transparentCount; i++)
		drawOpacityPacket(&transparent[order[i]], cam, LightList, workspace);
 }
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 YMGRE_Color_EndFrame(cam);
#endif
 if(deferWires){
  /* Draw wire overlays after transparency and optional HDR resolution. */
  for(GRE_ListNode node=ObjList->listhead;node;node=node->next)for(GRE_Object4d object=node->data;object;object=object->nextObject){
   if(!object->isVisible||!object->wireFrame||!object->pointList_wN)continue;
   YMGRE_RenderWorkspace_Reserve(workspace,object->pointNum,object->polygonNum,0);
   if(!YMGRE_RenderWorkspace_EnableVertexAttributes(workspace,object->pointNum))continue;
   if(YMGRE_Object_FrustumCullingCal(object,cam))continue;
   YMGRE_Object_WorldToCameraTo_wN(object,&cam->move.TMat,workspace->pointList_wN);
   GRE_Material material=YMGRE_Material_Find(MaterialList,object->materiaName);
   if(material&&material->doubleSided)memset(workspace->polygonHide,0,object->polygonNum);
   else YMGRE_Backface_RemoveTo(object,&cam->pos,workspace->polygonHide);
   YMGRE_VertexList_CameraToViewPlane_wN(workspace->pointList_wN,object->pointNum,cam->perspectPlane.Dis);
   YMGRE_VertexList_ViewPlaneToWindows_wN(workspace->pointList_wN,object->pointNum,cam);
   YMGRE_TrangleObject_Wires_wN(object,workspace->pointList_wN,workspace->polygonHide,cam);
  }
 }
}
#if YMGRE_PROFILE_RENDER_STAGES
#undef renderAdvancedBatch
static void renderAdvancedBatch(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList,
	GRE_RenderWorkspace workspace,uint8 clearTarget)
{
 uint32 start=YMGRE_ProfileNow();
 renderAdvancedBatch_Measured(thiscam,LightList,ObjList,MaterialList,workspace,clearTarget);
 YMGRE_ProfileCycles[6]+=YMGRE_ProfileNow()-start;
}
#endif


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

size_t YMGRE_Material_TransparentPacketSize(void)
{
#if YMGRE_ENABLE_TRANSPARENCY
 return sizeof(GRE_TransparentTriangle)+sizeof(size_t);
#else
 return 0;
#endif
}

void YMGRE_Camera_TanglePipline_wN(GRE_Camera4d camera,GRE_List lights,
 GRE_List objects,GRE_List materials,GRE_RenderWorkspace workspace)
{
 renderAdvancedBatch(camera,lights,objects,materials,workspace,1);
}
int YMGRE_Camera_AppendOpaqueBatch_wN(GRE_Camera4d camera,GRE_List lights,
 GRE_List objects,GRE_List materials,GRE_RenderWorkspace workspace)
{
 if(!camera||!lights||!objects||!materials||!workspace||!YMGRE_Camera_GetRenderTarget(camera))return 0;
#if YMGRE_ENABLE_LINEAR_COLOR
 if(camera->linearColorEnabled)return 0;
#endif
#if YMGRE_ENABLE_TRANSPARENCY
 for(GRE_ListNode n=objects->listhead;n;n=n->next)
  for(GRE_Object4d o=n->data;o;o=o->nextObject){
   GRE_Material m=YMGRE_Material_Find(materials,o->materiaName);
   if(m&&m->advanced&&m->advanced->opacityPixel)return 0;
  }
#endif
 renderAdvancedBatch(camera,lights,objects,materials,workspace,0);
 return workspace->materialStatus==0;
}
