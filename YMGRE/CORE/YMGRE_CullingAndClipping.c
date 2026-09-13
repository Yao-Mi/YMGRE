#include "./YMGRE_CullingAndClipping.h"
#include "./YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"

/*----------------------------------------  视景体裁剪 ----------------------------------------------*/
//对物体进行剔除
uint8 YMGRE_Object_FrustumCullingCal(GRE_Object4d myobj, GRE_Camera4d mycam)
{
	gre_fvector4d localpos;
	//物体绝对位置（center 0,0,0,1 + 世界偏移 = 世界坐标） 
	//变换到相机坐标系下
	YMGRE_Fvector4d_MatMultTo(&mycam->move.TMat, &myobj->WorldCoordinate, &localpos);

	float32 srx = 0, sry = 0, srz = 0;
	//选择剔除方式
	switch (myobj->boundType)
	{
	case GRE_Bounding_Box_AABB:	//AABB包围盒剔除
	{
		//AABB - box简单点说就是垂直于坐标轴的包围盒，这种包围盒不会旋转（当物体旋转的时候，就用更大的包围盒去包围它）。
		//参考：https://zhuanlan.zhihu.com/p/163590893
		
		srx = myobj->scale * (myobj->BoundingBoxMax.x - myobj->BoundingBoxMin.x) / 2; // r = d / 2
		sry = myobj->scale * (myobj->BoundingBoxMax.y - myobj->BoundingBoxMin.y) / 2;
		srz = myobj->scale * (myobj->BoundingBoxMax.z - myobj->BoundingBoxMin.z) / 2;

		srx = (srx > sry) ? ((srx > srz) ? srx : srz) : sry;//取max,由于物体会发生各种旋转，所以取最大值
		sry = srx;
		srz = srx;
		break;
	}
	case GRE_Bounding_Sphere_R://使用球体半径剔除
	{
		srx = myobj->scale * myobj->BoundingSphereR;
		sry = srx;
		srz = srx;
		break;
	}
	default:
		gre_log_explain(1, GRE_LOG_TypeI, "物体包围体类型错误");
		break;
	}

	uint8 dlflg = 0;
	//远近面剔除
	//    /    |      |
	//   *   #-|      |-#
	//    \    |      |
	if (((localpos.z + srz) < mycam->frustum.Znear) || \
		((localpos.z - srz) > mycam->frustum.Zfar))
		dlflg = 1;
	
	//左右面(x)剔除：基于xz平面投影进行
	//           |    
	//      - - -|    /
	//       \   |   /-#
	//      #-\  |  /
	//         \_| z
	//          x          tan(θ) = x/z   其中   x=w/2 ，可得 x(t) = z(t)*tan(θ)
	float32 x_tl = localpos.z * mycam->perspectPlane.kl; // pL是负数
	float32 x_tr = localpos.z * mycam->perspectPlane.kr;
	if (((localpos.x + srx) < x_tl) || \
		((localpos.x - srx) > x_tr))
		dlflg = 1;

	//上下面剔除，原理同上
	float32 y_tu = localpos.z * mycam->perspectPlane.ku;
	float32 y_td = localpos.z * mycam->perspectPlane.kd;//pD是负数
	if (((localpos.y + sry) < y_td) || \
		((localpos.y - sry) > y_tu))
		dlflg = 1;
	
	return dlflg;
}

void YMGRE_Object_FrustumCulling(GRE_Object4d myobj, GRE_Camera4d mycam)
{
	myobj->isDelete = YMGRE_Object_FrustumCullingCal(myobj, mycam);
}

/* These entry points receive projected x/y and camera-space z. Reject only
 * when every vertex is outside the same plane; an offscreen vertex alone says
 * nothing about the visible area of a polygon. Boundary points are inside. */
static uint8 YMGRE_ProjectedOutcode(GRE_Fvector4d point, GRE_Camera4d camera)
{
	uint8 code = 0;
	if (point->z < camera->frustum.Znear) code |= 1;
	if (point->z > camera->frustum.Zfar) code |= 2;
	/* Behind-camera projection reverses x/y. Keep side-plane rejection
	 * conservative for faces crossing the near plane. */
	if (point->z <= 0) return code;
	if (point->x < camera->perspectPlane.pL) code |= 4;
	if (point->x > camera->perspectPlane.pR) code |= 8;
	if (point->y < camera->perspectPlane.pD) code |= 16;
	if (point->y > camera->perspectPlane.pU) code |= 32;
	return code;
}

void YMGRE_ObjectPoly_FrustumCulling(GRE_Object4d object, GRE_Camera4d camera)
{
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		uint8 outside = 63;
		for (int j = 0; j < polygon->num && outside; j++)
			outside &= YMGRE_ProjectedOutcode(&object->pointList_[polygon->index[j]].pos, camera);
		if (outside) polygon->ishide = 1;
	}
}

void YMGRE_ObjectPoly_FrustumCullingTo(GRE_Object4d object, GRE_Vertex4d points,
	GRE_Camera4d camera, uint8* polygonHide)
{
	gre_log_explain((object == NULL) || (points == NULL) || (polygonHide == NULL), GRE_LOG_PtrIO, "输入的物体、顶点或多边形状态不存在");
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		uint8 outside = 63;
		for (int j = 0; j < polygon->num && outside; j++)
			outside &= YMGRE_ProjectedOutcode(&points[polygon->index[j]].pos, camera);
		if (outside) polygonHide[i] = 1;
	}
}

void YMGRE_ObjectPoly_FrustumCullingTo_wN(GRE_Object4d object, GRE_Vertex4d_wN points,
	GRE_Camera4d camera, uint8* polygonHide)
{
	if (object == NULL || points == NULL || camera == NULL || polygonHide == NULL) return;
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		uint8 outside = 63;
		for (int j = 0; j < polygon->num && outside; j++)
			outside &= YMGRE_ProjectedOutcode(&points[polygon->index[j]].base.pos, camera);
		if (outside) polygonHide[i] = 1;
	}
}

//计算相机空间顶点到指定视景体平面的有向距离，非负表示位于内部
static float32 YMGRE_FrustumPlaneDistance(GRE_Vertex4d vertex, GRE_Camera4d camera, uint8 plane)
{
	switch (plane)
	{
	case 0:
		return vertex->pos.z - camera->frustum.Znear;
	case 1:
		return camera->frustum.Zfar - vertex->pos.z;
	case 2:
		return vertex->pos.x - camera->perspectPlane.kl * vertex->pos.z;
	case 3:
		return camera->perspectPlane.kr * vertex->pos.z - vertex->pos.x;
	case 4:
		return vertex->pos.y - camera->perspectPlane.kd * vertex->pos.z;
	default:
		return camera->perspectPlane.ku * vertex->pos.z - vertex->pos.y;
	}
}

//根据端点到裁剪面的距离插值生成交点，同时保留纹理坐标
static gre_vertex4d YMGRE_FrustumClipIntersect(GRE_Vertex4d start, GRE_Vertex4d end,
	float32 startDistance, float32 endDistance)
{
	float32 t = startDistance / (startDistance - endDistance);
	gre_vertex4d result;
	result.pos.x = start->pos.x + (end->pos.x - start->pos.x) * t;
	result.pos.y = start->pos.y + (end->pos.y - start->pos.y) * t;
	result.pos.z = start->pos.z + (end->pos.z - start->pos.z) * t;
	result.pos.w = start->pos.w + (end->pos.w - start->pos.w) * t;
	result.u = start->u + (end->u - start->u) * t;
	result.v = start->v + (end->v - start->v) * t;
	return result;
}

uint16 YMGRE_Polygon_FrustumClip(GRE_Vertex4d input, uint16 inputNum, GRE_Vertex4d output,
	uint16 outputMax, GRE_Camera4d camera)
{
	gre_log_explain((input == NULL) || (output == NULL) || (camera == NULL), GRE_LOG_PtrIO,
		"输入多边形、输出缓存或相机不存在");
	gre_log_explain((inputNum < 3) || (inputNum > YMGRE_FRUSTUM_CLIP_VERTEX_MAX), GRE_LOG_ParamI,
		"视景体裁剪输入顶点数无效");

	gre_vertex4d buffer0[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	gre_vertex4d buffer1[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	GRE_Vertex4d source = buffer0;
	GRE_Vertex4d target = buffer1;
	uint16 sourceNum = inputNum;
	for (uint16 i = 0; i < inputNum; i++)
		buffer0[i] = input[i];

	for (uint8 plane = 0; plane < 6; plane++)
	{
		uint16 targetNum = 0;
		GRE_Vertex4d start = &source[sourceNum - 1];
		float32 startDistance = YMGRE_FrustumPlaneDistance(start, camera, plane);
		for (uint16 i = 0; i < sourceNum; i++)
		{
			GRE_Vertex4d end = &source[i];
			float32 endDistance = YMGRE_FrustumPlaneDistance(end, camera, plane);
			uint8 startInside = startDistance >= 0.0f;
			uint8 endInside = endDistance >= 0.0f;

			if (startInside != endInside)
			{
				gre_log_explain(targetNum >= YMGRE_FRUSTUM_CLIP_VERTEX_MAX, GRE_LOG_Mem1,
					"视景体裁剪输出顶点超过固定缓存");
				target[targetNum++] = YMGRE_FrustumClipIntersect(start, end,
					startDistance, endDistance);
			}
			if (endInside)
			{
				gre_log_explain(targetNum >= YMGRE_FRUSTUM_CLIP_VERTEX_MAX, GRE_LOG_Mem1,
					"视景体裁剪输出顶点超过固定缓存");
				target[targetNum++] = *end;
			}
			start = end;
			startDistance = endDistance;
		}

		if (targetNum == 0)
			return 0;
		GRE_Vertex4d swap = source;
		source = target;
		target = swap;
		sourceNum = targetNum;
	}

	gre_log_explain(sourceNum > outputMax, GRE_LOG_Mem1, "视景体裁剪输出缓存不足");
	for (uint16 i = 0; i < sourceNum; i++)
		output[i] = source[i];
	return sourceNum;
}

/*----------------------------------------  背面剔除 ----------------------------------------------*/
//可以将面提取为一致的逆时针或顺时针边序列，那么这是一个经过充分研究的问题。
// 经典解决方案是Newells算法：khronos.org / opengl / wiki / Calculating_a_Surface_Normal

void YMGRE_Backface_Remove(GRE_Object4d myobj, GRE_Fvector4d camPos)
{
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];//取该四边形
		//视矢量：由p[0]指向视点的向量：campos - p[0]= - p[0]
		int i1 = thispoly->index[0];

		//注意：必须使用 未经过透视变换的坐标点
		gre_fvector4d viewv;
		YMGRE_Fvector4d_SubToResult(camPos, &myobj->pointList[i1].pos, &viewv);//不能用 pointList_
		//		//     |
		//		//     |-----> pN
		//		//     |\
		//		//     | \ outside   cos(val)>0
		//与视矢量p[0] 与平面法向量pn的点积 < 0时，为背面， =0时为侧面(垂直)， 
		float32 dotv = YMGRE_Fvector4d_Dot(&thispoly->pN, &viewv);

		if (dotv> 0.0f)//则dot > 0时，等价于cos(-90,90)范围，是可见的。
		{
			thispoly->ishide = 0; //可见，未被隐藏
		}
		else
		{
			thispoly->ishide = 1;//被隐藏
		}
	}
}

void YMGRE_Backface_RemoveTo(GRE_Object4d myobj, GRE_Fvector4d camPos, uint8* polygonHide)
{
	gre_log_explain((myobj == NULL) || (camPos == NULL) || (polygonHide == NULL), GRE_LOG_PtrIO, "输入的物体、相机或多边形状态不存在");
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		int i1 = thispoly->index[0];
		gre_fvector4d viewv;
		YMGRE_Fvector4d_SubToResult(camPos, &myobj->pointList[i1].pos, &viewv);
		polygonHide[i] = (YMGRE_Fvector4d_Dot(&thispoly->pN, &viewv) > 0.0f) ? 0 : 1;
	}
}

/*----------------------------------------  边框裁剪 ----------------------------------------------*/

//求多边形的一条边sp和裁剪边point0 point1的交点
static inline void YMGRE_Clip_Intersect_(float x0, float y0, float x1, float y1, GRE_FRECT myroi, uint8 flg, float* x, float* y)// 0上 1下 2左 3右
{
	switch (flg)
	{
		//水平裁剪边
	case 0://(y == ymax)
		*y = myroi->y1;
		*x = x0 + (*y - y0) * (x1 - x0) / (y1 - y0);
		break;
	case 1://(y == ymin)
		*y = myroi->y0;
		*x = x0 + (*y - y0) * (x1 - x0) / (y1 - y0);
		break;
		//竖直裁剪边
	case 2://(x == xmin)
		*x = myroi->x0;
		*y = y0 + (*x - x0) * (y1 - y0) / (x1 - x0);
		break;
	case 3://(x == xmax)
		*x = myroi->x1;
		*y = y0 + (*x - x0) * (y1 - y0) / (x1 - x0);
		break;
	default:
		break;
	}
}
static inline int YMGRE_Clip_isInside(float x0, float y0, GRE_FRECT myroi, uint8 flg)// 0上 1下 2左 3右
{
	switch (flg)
	{
	case 0:
		if (y0 <= myroi->y1)//(y0 <= ymax)
			return 1;
		break;
	case 1:
		if (y0 >= myroi->y0)//(y0 >= ymin)
			return 1;
		break;
	case 2:
		if (x0 >= myroi->x0)//(x0 >= xmin)
			return 1;
		break;
	case 3:
		if (x0 <= myroi->x1)//(x0 <= xmax)
			return 1;
		break;
	default:
		break;
	}
	return 0;
}
static inline void YMGRE_Clip_u2Sortp(float* point, uint8 num)
{
	uint8 i, j, k;
	float uc;
	//找到最大值
	if (num)
	{
		if (point[1] != point[3])//y不相等 按y排序
		{
			//数据按y 递增重排 选择排序
			for (i = 0; i < num; i++)//选取表头
			{
				for (j = i, k = i; j < num; j++)
				{
					if ((point[2 * i + 1] > point[2 * j + 1]))
					{
						//纪录小的
						k = j;
					}
				}
				if (k != i)//交换数据
				{
					uc = point[2 * i]; point[2 * i] = point[2 * k]; point[2 * k] = uc; //x
					uc = point[2 * i + 1]; point[2 * i + 1] = point[2 * k + 1]; point[2 * k + 1] = uc;//y
				}
			}
		}
		else//按x排序
		{
			for (i = 0; i < num; i++)//选取表头
			{
				for (j = i, k = i; j < num; j++)
				{
					if ((point[2 * i] > point[2 * j]))
					{
						//纪录小的
						k = j;
					}
				}
				if (k != i)//交换数据
				{
					uc = point[2 * i]; point[2 * i] = point[2 * k]; point[2 * k] = uc;//x
					uc = point[2 * i + 1]; point[2 * i + 1] = point[2 * k + 1]; point[2 * k + 1] = uc;//y
				}
			}
		}
	}
}
//SutherlandHodgman 折线裁剪
static inline int SutherlandHodgmanPolygonClip2(GRE_fLinesList inlines, GRE_fLinesList outlines, GRE_FRECT myroi, uint8 flg)
{
	float x0, y0, x1, y1, jx, jy;
	int num = 0;//线段数量

	float jxy[10*2];//交点的xy
	uint8 jxyi = 0;
	//在内的线段
	for (int i = 0; i < inlines->lineNum; i++)
	{
		//S: x0,y0
		x0 = inlines->data[i].x0;
		y0 = inlines->data[i].y0;
		//P: x1,y1
		x1 = inlines->data[i].x1;
		y1 = inlines->data[i].y1;

		if (YMGRE_Clip_isInside(x1, y1, myroi, flg))//P在内侧
		{
			if (YMGRE_Clip_isInside(x0, y0, myroi, flg))//S在内侧  内部线段
			{
				//保留
				if (num >= outlines->lineMax) return 0;
				outlines->data[num].x0 = x0;
				outlines->data[num].y0 = y0;
				outlines->data[num].x1 = x1;
				outlines->data[num].y1 = y1;
				num++;
			}
			else//IP段在内
			{
				YMGRE_Clip_Intersect_(x0, y0, x1, y1, myroi, flg, &jx, &jy);//求交点
				//纪录交点
				if (jxyi >= 10) return 0;
				jxy[2 * jxyi] = jx;
				jxy[2 * jxyi + 1] = jy;
				jxyi++;
				//纪录IP段

				if (num >= outlines->lineMax) return 0;
				outlines->data[num].x0 = jx;
				outlines->data[num].y0 = jy;
				outlines->data[num].x1 = x1;
				outlines->data[num].y1 = y1;
				num++;
			}
		}
		else if (YMGRE_Clip_isInside(x0, y0, myroi, flg))//P在外 S在内侧
		{
			YMGRE_Clip_Intersect_(x0, y0, x1, y1, myroi, flg, &jx, &jy);//求交点
			//纪录交点
			if (jxyi >= 10) return 0;
			jxy[2 * jxyi] = jx;
			jxy[2 * jxyi + 1] = jy;
			jxyi++;
			//纪录SI段
			if (num >= outlines->lineMax) return 0;
			outlines->data[num].x0 = x0;
			outlines->data[num].y0 = y0;
			outlines->data[num].x1 = jx;
			outlines->data[num].y1 = jy;
			num++;
		}
		//访问下一条线段
	}
	//交点排序
	YMGRE_Clip_u2Sortp(jxy, jxyi);
	//两两连接交点
	jxyi = jxyi / 2;
	for (int i = 0; i < jxyi; i++)
	{
		if (num >= outlines->lineMax) return 0;
		outlines->data[num].x0 = jxy[4 * i];
		outlines->data[num].y0 = jxy[4 * i + 1];
		outlines->data[num].x1 = jxy[4 * i + 2];
		outlines->data[num].y1 = jxy[4 * i + 3];
		num++;
	}
	outlines->lineNum = num; //确定输出数量
	return 1;
}

//按多边形进行窗口裁剪
void YMGRE_Polygon_clip2D(GRE_fLinesList thislines, GRE_LinesList olines, GRE_FRECT winRect)
{
	gre_flineslist uline, uline2;

	/* lineNum is the caller's capacity on entry, and the result count on return. */
	if (!olines) return;
	unsigned capacity = olines->lineNum;
	olines->lineNum = 0;
	if (!thislines || !winRect || !thislines->data || !olines->data ||
		!capacity || thislines->lineNum > thislines->lineMax) return;

	uline.lineMax = capacity;
	uline2.lineMax = capacity;
	uline.data = (GRE_FLINE)GRE_malloc0(uline.lineMax * sizeof(gre_fline));
	uline2.data = (GRE_FLINE)GRE_malloc0(uline2.lineMax * sizeof(gre_fline));

	//L - > u
	if (!SutherlandHodgmanPolygonClip2(thislines, &uline, winRect, 0)) goto cleanup;//上
	//u - > u2
	if (!SutherlandHodgmanPolygonClip2(&uline, &uline2, winRect, 1)) goto cleanup;//下
	//u2 - > u
	if (!SutherlandHodgmanPolygonClip2(&uline2, &uline, winRect, 2)) goto cleanup;//左
	//u - > u2
	if (!SutherlandHodgmanPolygonClip2(&uline, &uline2, winRect, 3)) goto cleanup;//右

	//转short类型
	olines->lineNum = uline2.lineNum;
	for (int i = 0; i < uline2.lineNum; i++)
	{
		olines->data[i].x0 = (int16)(uline2.data[i].x0 + 0.5); //四舍五入
		olines->data[i].y0 = (int16)(uline2.data[i].y0 + 0.5);
		olines->data[i].x1 = (int16)(uline2.data[i].x1 + 0.5);
		olines->data[i].y1 = (int16)(uline2.data[i].y1 + 0.5);
	}
cleanup:
	//内存释放
	GRE_free0(uline2.data);//释放
	GRE_free0(uline.data);//释放
}

static gre_vertex4d_wN YMGRE_FrustumClipIntersect_wN(GRE_Vertex4d_wN start, GRE_Vertex4d_wN end,
	float32 startDistance, float32 endDistance)
{
	float32 t = startDistance / (startDistance - endDistance);
	gre_vertex4d_wN result = *start;
	result.base.pos.x += (end->base.pos.x - start->base.pos.x) * t;
	result.base.pos.y += (end->base.pos.y - start->base.pos.y) * t;
	result.base.pos.z += (end->base.pos.z - start->base.pos.z) * t;
	result.base.pos.w += (end->base.pos.w - start->base.pos.w) * t;
	result.base.u += (end->base.u - start->base.u) * t;
	result.base.v += (end->base.v - start->base.v) * t;
	result.lightmapU += (end->lightmapU - start->lightmapU) * t;
	result.lightmapV += (end->lightmapV - start->lightmapV) * t;
	result.normal.x += (end->normal.x - start->normal.x) * t;
	result.normal.y += (end->normal.y - start->normal.y) * t;
	result.normal.z += (end->normal.z - start->normal.z) * t;
	result.tangent.x += (end->tangent.x - start->tangent.x) * t;
	result.tangent.y += (end->tangent.y - start->tangent.y) * t;
	result.tangent.z += (end->tangent.z - start->tangent.z) * t;
	result.tangentW += (end->tangentW - start->tangentW) * t;
	result.color.R = (uint8)(start->color.R + (end->color.R - start->color.R) * t);
	result.color.G = (uint8)(start->color.G + (end->color.G - start->color.G) * t);
	result.color.B = (uint8)(start->color.B + (end->color.B - start->color.B) * t);
	result.vertexLighting.R = (uint8)(start->vertexLighting.R + (end->vertexLighting.R - start->vertexLighting.R) * t);
	result.vertexLighting.G = (uint8)(start->vertexLighting.G + (end->vertexLighting.G - start->vertexLighting.G) * t);
	result.vertexLighting.B = (uint8)(start->vertexLighting.B + (end->vertexLighting.B - start->vertexLighting.B) * t);
	result.vertexSpecular.R = (uint8)(start->vertexSpecular.R + (end->vertexSpecular.R - start->vertexSpecular.R) * t);
	result.vertexSpecular.G = (uint8)(start->vertexSpecular.G + (end->vertexSpecular.G - start->vertexSpecular.G) * t);
	result.vertexSpecular.B = (uint8)(start->vertexSpecular.B + (end->vertexSpecular.B - start->vertexSpecular.B) * t);
	return result;
}

uint16 YMGRE_Polygon_FrustumClip_wN(GRE_Vertex4d_wN input, uint16 inputNum, GRE_Vertex4d_wN output,
	uint16 outputMax, GRE_Camera4d camera)
{
	if (input == NULL || output == NULL || camera == NULL || inputNum < 3 || outputMax < 3)
		return 0;
	gre_vertex4d_wN buffer0[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	gre_vertex4d_wN buffer1[YMGRE_FRUSTUM_CLIP_VERTEX_MAX];
	GRE_Vertex4d_wN source = buffer0, target = buffer1;
	uint16 sourceNum = inputNum > YMGRE_FRUSTUM_CLIP_VERTEX_MAX ? YMGRE_FRUSTUM_CLIP_VERTEX_MAX : inputNum;
	for (uint16 i = 0; i < sourceNum; i++) source[i] = input[i];
	for (uint8 plane = 0; plane < 6; plane++)
	{
		uint16 targetNum = 0;
		GRE_Vertex4d_wN start = &source[sourceNum - 1];
		float32 sd = YMGRE_FrustumPlaneDistance(&start->base, camera, plane);
		for (uint16 i = 0; i < sourceNum; i++)
		{
			GRE_Vertex4d_wN end = &source[i];
			float32 ed = YMGRE_FrustumPlaneDistance(&end->base, camera, plane);
			if ((sd >= 0) != (ed >= 0) && targetNum < YMGRE_FRUSTUM_CLIP_VERTEX_MAX)
				target[targetNum++] = YMGRE_FrustumClipIntersect_wN(start, end, sd, ed);
			if (ed >= 0 && targetNum < YMGRE_FRUSTUM_CLIP_VERTEX_MAX) target[targetNum++] = *end;
			start = end; sd = ed;
		}
		if (targetNum == 0) return 0;
		GRE_Vertex4d_wN swap = source; source = target; target = swap; sourceNum = targetNum;
	}
	uint16 count = sourceNum < outputMax ? sourceNum : outputMax;
	for (uint16 i = 0; i < count; i++) output[i] = source[i];
	return count;
}

