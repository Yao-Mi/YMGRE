#include "./YMGRE_Rasterization.h"
#include "./YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"
#include "./YMGRE_CullingAndClipping.h"
#include "./YMGRE_TriangleRaster.h"

GRErgb24 GRE_brush = { .R = 50,.G = 100,.B = 100 };
GRErgb24 GRE_background_color = { .R = 10,.G = 10,.B = 10 };

#define YMGRE_RASTER_AREA_EPSILON 1e-5f
#define YMGRE_RASTER_WIRE_DEPTH_EPSILON 0.1f

void YMGRE_Img_SetBrushColor(GRErgb24 color)
{
	GRE_brush = color;
}

//光栅化热路径使用整数转换完成取整，避免 MCU 上逐行调用 libm
static inline int YMGRE_Raster_Ceil(float32 value)
{
	int result = (int)value;
	return result + (value > result);
}

static inline int YMGRE_Raster_Floor(float32 value)
{
	int result = (int)value;
	return result - (value < result);
}

//从有序顶点中寻找第一组有效平面，允许多边形包含重复点或零长度边
static uint8 YMGRE_Raster_PolygonPlane(GRE_Vertex4d points, GRE_Polygon4d polygon,
	GRE_Fvector4d plane)
{
	int baseIndex = polygon->index[0];
	for (int i = 1; i + 1 < polygon->num; i++)
	{
		gre_fvector4d u;
		gre_fvector4d v;
		YMGRE_Fvector4d_SubToResult(&points[polygon->index[i]].pos,
			&points[baseIndex].pos, &u);
		YMGRE_Fvector4d_SubToResult(&points[polygon->index[i + 1]].pos,
			&points[baseIndex].pos, &v);
		YMGRE_Fvector4d_CrossToResult(&u, &v, plane);
		if ((plane->z > YMGRE_RASTER_AREA_EPSILON) ||
			(plane->z < -YMGRE_RASTER_AREA_EPSILON))
		{
			plane->w = -(plane->x * points[baseIndex].pos.x +
				plane->y * points[baseIndex].pos.y + plane->z * points[baseIndex].pos.z);
			return 1;
		}
	}
	return 0;
}

//清除照片底色
void YMGL_Img_Clear(GRE_FrameBuffer datap, uint16 width, uint16 height)
{
	gre_log_explain(datap == NULL, GRE_LOG_PtrI, "输入的图像不存在");

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			datap[i * width + j] = GRE_FramePixel_From_RGB24(GRE_background_color);
		}
	}
}

//直线绘制 不包含窗口裁剪
void YMGRE_Img_Line(GRE_FrameBuffer data, uint16 width, uint16 height, int16 x1, int16 y1, int16 x2, int16 y2)
{
	gre_log_explain((x1 < 0) || (y1 < 0) || (x2 < 0) || (y2 < 0), GRE_LOG_ParamI, "输入的直线端点坐标<0");
	gre_log_explain((x1 >= width) || (y1 >= height) || (x2 >= width) || (y2 >= height), GRE_LOG_ParamI, "输入的直线端点坐标 >=边界条件");

	int delta_x = (x2 > x1) ? (x2 - x1) : (x1 - x2);
	int delta_y = (y2 > y1) ? (y1 - y2) : (y2 - y1);
	int incx = (x1 < x2) ? 1 : -1;
	int incy = (y1 < y2) ? 1 : -1;
	int error = delta_x + delta_y;

	//对称 Bresenham 画线算法，首尾端点各绘制一次
	while (1)
	{
		data[y1 * width + x1] = GRE_FramePixel_From_RGB24(GRE_brush);
		if ((x1 == x2) && (y1 == y2))
			break;
		int error2 = error * 2;
		if (error2 >= delta_y)
		{
			error += delta_y;
			x1 += incx;
		}
		if (error2 <= delta_x)
		{
			error += delta_x;
			y1 += incy;
		}
	}
}

//已裁剪3D线段的屏幕空间光栅化，使用1/z插值保持透视深度正确
void YMGRE_Img_LineDepth(GRE_FrameBuffer data, float32* zbuff, uint16 width, uint16 height,
	int16 x1, int16 y1, float32 z1, int16 x2, int16 y2, float32 z2, GRErgb24 color, uint8 depthTest)
{
	gre_log_explain((data == NULL) || (zbuff == NULL), GRE_LOG_PtrIO, "线段颜色或深度缓存不存在");
	int32 dx = (x2 >= x1) ? x2 - x1 : x1 - x2;
	int32 dy = (y2 >= y1) ? y2 - y1 : y1 - y2;
	int32 steps = (dx > dy) ? dx : dy;
	GRE_FramePixel pixel = GRE_FramePixel_From_RGB24(color);
	float32 invZ1 = 1.0f / z1;
	float32 invZ2 = 1.0f / z2;
	for (int32 i = 0; i <= steps; i++)
	{
		float32 t = (steps > 0) ? (float32)i / steps : 0.0f;
		int32 x = x1 + (int32)((x2 - x1) * t + ((x2 >= x1) ? 0.5f : -0.5f));
		int32 y = y1 + (int32)((y2 - y1) * t + ((y2 >= y1) ? 0.5f : -0.5f));
		if (x < 0 || y < 0 || x >= width || y >= height) continue;
		float32 z = 1.0f / (invZ1 + (invZ2 - invZ1) * t);
		uint32 index = (uint32)y * width + x;
		if (!depthTest || z <= zbuff[index] + YMGRE_RASTER_WIRE_DEPTH_EPSILON)
		{
			data[index] = pixel;
			if (depthTest && z < zbuff[index]) zbuff[index] = z;
		}
	}
}

//多边形轮廓与扫描填充统一在像素中心采样，并使用同一平面深度参与遮挡
static void YMGRE_Img_PolygonLine(GRE_FrameBuffer data, uint16 width, float32* zbuff,
	GRE_Fvector4d plane, int16 x1, int16 y1, int16 x2, int16 y2)
{
	int32 deltaX = x2 - x1;
	int32 deltaY = y2 - y1;
	int32 absX = (deltaX >= 0) ? deltaX : -deltaX;
	int32 absY = (deltaY >= 0) ? deltaY : -deltaY;
	GRE_FramePixel brushPixel = GRE_FramePixel_From_RGB24(GRE_brush);
	float32 invZ = 1.0f / plane->z;
	float32 zStepX = -plane->x * invZ;
	float32 zStepY = -plane->y * invZ;
	float32 depthEpsilon = YMGRE_RASTER_WIRE_DEPTH_EPSILON +
		YMGRE_Fabs(zStepX) + YMGRE_Fabs(zStepY);

	if ((deltaX == 0) && (deltaY == 0))
	{
		uint32 index = y1 * width + x1;
		float32 z = -(plane->x * (x1 + 0.5f) + plane->y * (y1 + 0.5f) + plane->w) * invZ;
		if (z <= zbuff[index] + depthEpsilon)
		{
			data[index] = brushPixel;
			if (z < zbuff[index])
				zbuff[index] = z;
		}
		return;
	}

	if (absY >= absX)
	{
		if (y1 > y2)
		{
			int16 temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
			deltaX = x2 - x1;
			deltaY = y2 - y1;
		}

		//计算边与 y+0.5 的交点，余数递推避免逐像素除法
		int32 denominator = deltaY * 2;
		int32 error = deltaX;
		int32 x = x1;
		if (error < 0)
		{
			x--;
			error += denominator;
		}
		float32 z = -(plane->x * (x + 0.5f) + plane->y * (y1 + 0.5f) + plane->w) * invZ;
		for (int32 y = y1; y < y2; y++)
		{
			uint32 index = y * width + x;
			if (z <= zbuff[index] + depthEpsilon)
			{
				data[index] = brushPixel;
				if (z < zbuff[index])
					zbuff[index] = z;
			}
			error += deltaX * 2;
			z += zStepY;
			if (error >= denominator)
			{
				error -= denominator;
				x++;
				z += zStepX;
			}
			else if (error < 0)
			{
				error += denominator;
				x--;
				z -= zStepX;
			}
		}
	}
	else
	{
		if (x1 > x2)
		{
			int16 temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
			deltaX = x2 - x1;
			deltaY = y2 - y1;
		}

		//计算边与 x+0.5 的交点，余数递推避免逐像素除法
		int32 denominator = deltaX * 2;
		int32 error = deltaY;
		int32 y = y1;
		if (error < 0)
		{
			y--;
			error += denominator;
		}
		float32 z = -(plane->x * (x1 + 0.5f) + plane->y * (y + 0.5f) + plane->w) * invZ;
		for (int32 x = x1; x < x2; x++)
		{
			uint32 index = y * width + x;
			if (z <= zbuff[index] + depthEpsilon)
			{
				data[index] = brushPixel;
				if (z < zbuff[index])
					zbuff[index] = z;
			}
			error += deltaY * 2;
			z += zStepX;
			if (error >= denominator)
			{
				error -= denominator;
				y++;
				z += zStepY;
			}
			else if (error < 0)
			{
				error += denominator;
				y--;
				z -= zStepY;
			}
		}
	}
}

//三角网格使用方向无关的单像素线段，深度采样与三角形填充保持一致
static void YMGRE_Img_TriangleLine(GRE_FrameBuffer data, uint16 width, float32* zbuff,
	GRE_Fvector4d plane, int16 x1, int16 y1, int16 x2, int16 y2)
{
	int32 deltaX = (x2 >= x1) ? (x2 - x1) : (x1 - x2);
	int32 deltaY = (y2 >= y1) ? (y1 - y2) : (y2 - y1);
	int32 stepX = (x1 < x2) ? 1 : -1;
	int32 stepY = (y1 < y2) ? 1 : -1;
	int32 error = deltaX + deltaY;
	GRE_FramePixel brushPixel = GRE_FramePixel_From_RGB24(GRE_brush);
	float32 invZ = 1.0f / plane->z;
	float32 zStepX = -plane->x * invZ;
	float32 zStepY = -plane->y * invZ;
	float32 z = -(plane->x * x1 + plane->y * y1 + plane->w) * invZ;
	//线段取整最多偏移一个像素，按平面深度斜率补偿对应的深度差
	float32 depthEpsilon = YMGRE_RASTER_WIRE_DEPTH_EPSILON +
		YMGRE_Fabs(zStepX) + YMGRE_Fabs(zStepY);

	//与三角形填充统一在整数像素坐标采样，两个端点均只绘制一次
	while (1)
	{
		uint32 index = y1 * width + x1;
		if (z <= zbuff[index] + depthEpsilon)
		{
			data[index] = brushPixel;
			if (z < zbuff[index])
				zbuff[index] = z;
		}
		if ((x1 == x2) && (y1 == y2))
			break;
		int32 error2 = error * 2;
		if (error2 >= deltaY)
		{
			error += deltaY;
			x1 += stepX;
			z += stepX * zStepX;
		}
		if (error2 <= deltaX)
		{
			error += deltaX;
			y1 += stepY;
			z += stepY * zStepY;
		}
	}
}

#ifdef YMGRE_SCANLINE_LEGACY_TEST
//使用索引进行快排
static int YMGRE_SCL_Partsortp(short** array, int left, int right)
{
	//基准值
	short* ponit = array[left];//指针指向的数组内容值
	//坑的位置
	int temp = left;
	while (left <= right)
	{
		while (left <= right)
		{
			if ((*array[right]) < *ponit)
			{
				array[left] = array[right];//交换指针位置
				++left;
				temp = right;
				break;
			}
			else
				--right;
		}
		while (left <= right)
		{
			if ((*array[left]) > *ponit)
			{
				array[right] = array[left];
				--right;
				temp = left;
				break;
			}
			else
				++left;
		}

	}
	array[temp] = ponit;
	return temp;
}

//快速排序算法
static void YMGRE_SCL_QuickSortp(short** array, int num)
{
	short* stack = GRE_malloc1(num * sizeof(short));
	gre_log_explain(num == 0, GRE_LOG_ParamI, "排序个数为0");
	gre_log_explain(stack == NULL, GRE_LOG_Mem1, "排序计算缓存申请失败");
	int left = 0;
	int right = num - 1;
	int stacp = 0;

	stack[stacp++] = left;
	stack[stacp++] = right;
	while (stacp != 0)
	{
		int right = stack[--stacp];
		int left = stack[--stacp];
		//划分左右部分的边界线
		int Index = YMGRE_SCL_Partsortp(array, left, right);
		//左半部分
		if (Index - 1 > left)
		{
			stack[stacp++] = left;
			stack[stacp++] = (Index - 1);
		}
		//右半部分
		if (Index + 1 < right)
		{
			stack[stacp++] = (Index + 1);
			stack[stacp++] = right;
		}
	}
	GRE_free1(stack);
}

typedef struct et_
{
	short xmin;
	short ymax;
	float dx;
	float udx;
	struct et_* next;
}gre_flet;
#endif

typedef struct scanedge_
{
	int16 ymin;
	int16 ymax;
	float32 x;
	float32 dx;
}gre_scanedge;

typedef struct scanline_workspace_
{
	gre_scanedge* edges;
	gre_scanedge** active;
	uint16 edgeMax;
}gre_scanline_workspace;

/**
  * @brief 使用外部缓存的多边形扫描填充算法 + zbuff
  */
static void YMGRE_Img_Scanline_AreaFillWithWorkspace(GRE_FrameBuffer datap, uint16 width, uint16 height, GRE_LinesList ring, GRE_Fvector4d pN, float32* zdeep, GRErgb24 fillcolor, gre_scanline_workspace* workspace)
{
	gre_log_explain(datap == NULL, GRE_LOG_PtrIO, "输入输出图像不存在");
	gre_log_explain(ring == NULL, GRE_LOG_PtrI, "输入的线段不存在");
	gre_log_explain(workspace == NULL, GRE_LOG_PtrI, "输入的扫描线缓存不存在");
	gre_log_explain(ring->lineNum == 0, GRE_LOG_Mem1, "多边形线段数为0");
	gre_log_explain(ring->lineNum > workspace->edgeMax, GRE_LOG_ParamI, "多边形线段数超过扫描线缓存");
	if ((pN->z <= YMGRE_RASTER_AREA_EPSILON) && (pN->z >= -YMGRE_RASTER_AREA_EPSILON))
		return;

	uint16 edgeNum = 0;
	for (uint16 i = 0; i < ring->lineNum; i++)// 初始化所有非水平边
	{
		gre_log_explain((ring->data[i].x0 < 0) || (ring->data[i].x0 >= width) || (ring->data[i].y0 < 0) || (ring->data[i].y0 >= height), GRE_LOG_ParamI, "点超过图像范围");
		gre_log_explain((ring->data[i].x1 < 0) || (ring->data[i].x1 >= width) || (ring->data[i].y1 < 0) || (ring->data[i].y1 >= height), GRE_LOG_ParamI, "点超过图像范围");

		gre_scanedge* edge = &workspace->edges[edgeNum];
		if (ring->data[i].y0 > ring->data[i].y1)//交换顺序
		{
			edge->ymin = ring->data[i].y1;
			edge->ymax = ring->data[i].y0;
			edge->dx = (ring->data[i].x0 - ring->data[i].x1) / (float32)(ring->data[i].y0 - ring->data[i].y1);
			edge->x = ring->data[i].x1 + edge->dx * 0.5f;//首行在像素中心 y+0.5 取交点
			edgeNum++;
		}
		else if (ring->data[i].y0 < ring->data[i].y1)//忽略水平线
		{
			edge->ymin = ring->data[i].y0;
			edge->ymax = ring->data[i].y1;
			edge->dx = (ring->data[i].x1 - ring->data[i].x0) / (float32)(ring->data[i].y1 - ring->data[i].y0);
			edge->x = ring->data[i].x0 + edge->dx * 0.5f;//首行在像素中心 y+0.5 取交点
			edgeNum++;
		}
	}
	if (edgeNum == 0)
		return;

	//按ymin递增排序，相同ymin按x递增排序
	for (uint16 i = 1; i < edgeNum; i++)
	{
		gre_scanedge edge = workspace->edges[i];
		int32 j = i - 1;
		while ((j >= 0) && ((workspace->edges[j].ymin > edge.ymin) ||
			((workspace->edges[j].ymin == edge.ymin) && (workspace->edges[j].x > edge.x))))
		{
			workspace->edges[j + 1] = workspace->edges[j];
			j--;
		}
		workspace->edges[j + 1] = edge;
	}

	uint16 edgeIndex = 0;
	uint16 activeNum = 0;
	float32 zadd = -(pN->x / pN->z);//z(x+1,y) = z(x,y)-A/C
	GRE_FramePixel fillPixel = GRE_FramePixel_From_RGB24(fillcolor);
	for (int32 y = workspace->edges[0].ymin; (y < height) && ((edgeIndex < edgeNum) || (activeNum > 0)); y++)
	{
		//剔除已完成的边，并更新上一行保留下来的交点
		uint16 remainNum = 0;
		for (uint16 i = 0; i < activeNum; i++)
		{
			gre_scanedge* edge = workspace->active[i];
			if (y < edge->ymax)
			{
				edge->x += edge->dx;
				workspace->active[remainNum++] = edge;
			}
		}
		activeNum = remainNum;

		//加入当前扫描线开始的边
		while ((edgeIndex < edgeNum) && (workspace->edges[edgeIndex].ymin == y))
		{
			workspace->active[activeNum++] = &workspace->edges[edgeIndex];
			edgeIndex++;
		}

		//活动边按x递增重排，边数很小时插入排序更快
		for (uint16 i = 1; i < activeNum; i++)
		{
			gre_scanedge* edge = workspace->active[i];
			int32 j = i - 1;
			while ((j >= 0) && (workspace->active[j]->x > edge->x))
			{
				workspace->active[j + 1] = workspace->active[j];
				j--;
			}
			workspace->active[j + 1] = edge;
		}

		float32* zbuff_i = &zdeep[y * width];//深度缓冲器，该行起点
		GRE_FrameBuffer frame_i = &datap[y * width];//帧缓冲区中，该行起点
		for (uint16 i = 0; i + 1 < activeNum; i += 2)
		{
			//像素中心位于左右交点之间，右边界不重复填充
			int32 begX = GREMax(YMGRE_Raster_Ceil(workspace->active[i]->x - 0.5f), 0);
			int32 endX = GREMin(YMGRE_Raster_Ceil(workspace->active[i + 1]->x - 0.5f) - 1, width - 1);
			float32 zval_L = -(pN->x * (begX + 0.5f) + pN->y * (y + 0.5f) + pN->w) / pN->z;//左侧像素中心深度
			for (int32 x = begX; x <= endX; x++)//在两段间画上线段
			{
				//Z-buff比较，若距离变小则更新缓存
				if (zbuff_i[x] > zval_L)
				{
					zbuff_i[x] = zval_L;
					frame_i[x] = fillPixel;
				}
				zval_L += zadd;
			}
		}
	}
}

/**
  * @brief 多边形扫描填充算法 + zbuff
  */
void YMGRE_Img_Scanline_AreaFill(GRE_FrameBuffer datap, uint16 width, uint16 height, GRE_LinesList ring, GRE_Fvector4d pN, float32* zdeep, GRErgb24 fillcolor)
{
	gre_log_explain(ring == NULL, GRE_LOG_PtrI, "输入的线段不存在");
	gre_scanline_workspace workspace;
	workspace.edgeMax = ring->lineNum;
	workspace.edges = GRE_malloc1(workspace.edgeMax * sizeof(gre_scanedge));
	workspace.active = GRE_malloc1(workspace.edgeMax * sizeof(gre_scanedge*));
	gre_log_explain((workspace.edges == NULL) || (workspace.active == NULL), GRE_LOG_Mem1, "扫描线计算缓存申请失败");

	YMGRE_Img_Scanline_AreaFillWithWorkspace(datap, width, height, ring, pN, zdeep, fillcolor, &workspace);

	GRE_free1(workspace.active);
	GRE_free1(workspace.edges);
}

#ifdef YMGRE_SCANLINE_LEGACY_TEST
/**
  * @brief 用于结果对照的旧版多边形扫描填充算法 + zbuff
  */
static void YMGRE_Img_Scanline_AreaFillLegacy(GRE_FrameBuffer datap, uint16 width, uint16 height, GRE_LinesList ring, GRE_Fvector4d pN, float32* zdeep, GRErgb24 fillcolor)
{
	gre_log_explain(datap == NULL, GRE_LOG_PtrIO, "输入输出图像不存在");
	gre_log_explain(ring == NULL, GRE_LOG_PtrI, "输入的线段不存在");

	uint32 lineNum = ring->lineNum;
	gre_log_explain(lineNum == 0, GRE_LOG_Mem1, "多边形线段数为0");

	short* linesET = GRE_malloc1(4 * lineNum * sizeof(short));
	gre_log_explain(linesET == NULL, GRE_LOG_Mem1, "linesET计算缓存申请失败");

	uint8 errflg = 0;
	uint32 lpi = 0, i = 0;
	int32 j = 0;
	for (i = 0; i < lineNum; i++)// 初始化所有直线形式（x_0,ymin,x_1,ymax）
	{
		gre_log_explain((ring->data[i].x0 < 0) || (ring->data[i].x0 >= width) || (ring->data[i].y0 < 0) || (ring->data[i].y0 >= height), GRE_LOG_ParamI, "点超过图像范围");
		gre_log_explain((ring->data[i].x1 < 0) || (ring->data[i].x1 >= width) || (ring->data[i].y1 < 0) || (ring->data[i].y1 >= height), GRE_LOG_ParamI, "点超过图像范围");
		// A : x0,y0
		// B : x1,y1

		//忽略水平线 , 线段按y从小到大排列
		if (ring->data[i].y0 > ring->data[i].y1)//交换顺序
		{
			linesET[4 * lpi] = ring->data[i].x1;//x2
			linesET[4 * lpi + 1] = ring->data[i].y1;//y2
			linesET[4 * lpi + 2] = ring->data[i].x0;//x1
			linesET[4 * lpi + 3] = ring->data[i].y0;//y1
			lpi++;
		}
		else if (ring->data[i].y0 < ring->data[i].y1)
		{
			linesET[4 * lpi] = ring->data[i].x0;//x1
			linesET[4 * lpi + 1] = ring->data[i].y0;//y1
			linesET[4 * lpi + 2] = ring->data[i].x1;//x2
			linesET[4 * lpi + 3] = ring->data[i].y1;//y2
			lpi++;
		}
	}
	// 并非只有水平线，
	if (lpi != 0)
	{
		short** plET = GRE_malloc1(lpi * sizeof(short*));
		gre_log_explain(plET == NULL, GRE_LOG_Mem1, "plET计算缓存申请失败");

		//按x1递增排序
		for (i = 0; i < lpi; i++)
		{
			plET[i] = &linesET[4 * i]; //初始化线段位置指针
		}
		YMGRE_SCL_QuickSortp(plET, lpi);//快速指针排序
		//建立表AET
		gre_flet** pAET = GRE_malloc1(height * sizeof(gre_flet*));//当前ymin扫描线通过的直线记录表 头部
		gre_flet** npAET = GRE_malloc1(height * sizeof(gre_flet*));//尾部
		gre_log_explain((pAET == NULL) || (npAET == NULL), GRE_LOG_Mem1, "AET计算缓存申请失败");
		GRE_memset(pAET, 0, height * sizeof(gre_flet*));//清除数据 初始化

		//一条线段【xmin，ymax，detax，detay *next】
		for (i = 0; i < lpi; i++)
		{
			short* slsp = plET[i];//取直线地址
			int ymin = slsp[1];//得 ymin = slsp[1]
			if (pAET[ymin] == NULL)
			{
				gre_flet* thisAET = GRE_malloc1(sizeof(gre_flet));//添加一条直线
				gre_log_explain(thisAET == NULL, GRE_LOG_Mem1, "thisAET计算缓存申请失败");
				thisAET->xmin = slsp[0];
				thisAET->ymax = slsp[3];
				thisAET->dx = (slsp[2] - slsp[0]) / (float)(slsp[3] - slsp[1]);
				thisAET->udx = (float32)slsp[0];
				thisAET->next = NULL;

				pAET[ymin] = thisAET;//放入活动边表AET
				npAET[ymin] = thisAET;
			}
			else
			{
				gre_flet* thisAET = GRE_malloc1(sizeof(gre_flet));//添加一条直线
				gre_log_explain(thisAET == NULL, GRE_LOG_Mem1, "thisAET计算缓存申请失败");

				thisAET->xmin = slsp[0];
				thisAET->ymax = slsp[3];
				thisAET->dx = (slsp[2] - slsp[0]) / (float)(slsp[3] - slsp[1]);
				thisAET->udx = (float32)slsp[0];
				thisAET->next = NULL;

				npAET[ymin]->next = thisAET;//记录
				npAET[ymin] = thisAET;//更新尾部
			}
		}

		gre_flet* ufrp = NULL, * lstp = NULL, * mid = NULL;
		uint8 joflg = 0;//0为偶，255为奇数
		gre_flet* pNET = NULL;
		gre_flet exCD;
		int netnum = 0;
		GRE_memset(npAET, 0, height * sizeof(gre_flet*));//清除数据 初始化，用于记录绘制的线段

		for (i = 0; i < height; i++)
		{
			if ((pAET[i] != NULL) || (pNET != NULL))//上一次扫描列表非空 或者当前有扫描队列加入
			{
				if (pNET == NULL) //首次查询到直线端点
				{
					ufrp = pAET[i];//当前端点

					//将当前端点 添加到扫描队列
					pNET = GRE_malloc1(sizeof(gre_flet));//添加当前
					gre_log_explain(pNET == NULL, GRE_LOG_Mem1, "pNET计算缓存申请失败");
					GRE_memcpy(pNET, ufrp, sizeof(gre_flet));
					netnum++;

					//将后面挂的端点也添加进去
					lstp = pNET; ufrp = ufrp->next;
					while (ufrp)
					{
						lstp->next = GRE_malloc1(sizeof(gre_flet));//添加后续
						lstp = lstp->next;
						gre_log_explain(lstp == NULL, GRE_LOG_Mem1, "lstp计算缓存申请失败");
						GRE_memcpy(lstp, ufrp, sizeof(gre_flet));
						netnum++;
						//接着查找
						ufrp = ufrp->next;
					}
				}
				else
				{
					//计算上层扫描线到下层扫描线 x的变化。剔除已经完成的直线端点
					{
						//表头部处理：
						ufrp = pNET;
						while (ufrp)//将表头移除
						{
							if (i < ufrp->ymax)//小于ymax
								break;
							else
							{
								mid = pNET;//表头数据
								//跳到下一个
								pNET = pNET->next;
								ufrp = pNET;
								//移除
								GRE_free1(mid);
								netnum--;
							}
						}
						//剩下部分处理：
						lstp = pNET;
						while (ufrp)//从表中间移除
						{
							if (i < ufrp->ymax)//小于ymax
							{
								lstp = ufrp;
								//不处理，跳过
								ufrp = lstp->next;
							}
							else
							{
								mid = ufrp;//当前端点
								//跳到下个数据
								lstp->next = ufrp->next; //更新pNET记录
								ufrp = lstp->next;
								//移除
								//printf("  %d ", mid->xmin);//显示
								GRE_free1(mid);
								netnum--;
							}
						}
					}

					//更新已有数据的  x坐标
					ufrp = pNET;
					while (ufrp)
					{
						ufrp->udx += ufrp->dx;
						ufrp = ufrp->next;
					}
					//新端点加入 扫描队列尾部
					if (pAET[i] != NULL)
					{
						ufrp = pAET[i];//首端点位置
						while (ufrp)
						{
							//lstp为扫描队列 最尾巴的那个
							if (lstp == NULL)//pNET==Null
							{
								pNET = GRE_malloc1(sizeof(gre_flet));//创建一个端点
								gre_log_explain(pNET == NULL, GRE_LOG_Mem1, "pNET计算缓存申请失败");
								lstp = pNET;
								netnum = 1;
							}
							else
							{
								lstp->next = GRE_malloc1(sizeof(gre_flet));//添加一个端点
								lstp = lstp->next;
								gre_log_explain(lstp == NULL, GRE_LOG_Mem1, "lstp计算缓存申请失败");
								netnum++;
							}
							GRE_memcpy(lstp, ufrp, sizeof(gre_flet));//拷贝端点到扫描队列
							ufrp = ufrp->next;
						}
					}
					//表被处理完
					if (netnum == 0)
						pNET = NULL;
				}

				//扫描队列按x 递增重排 选择排序
				for (lstp = pNET; lstp != NULL; lstp = lstp->next)//选取表头
				{
					mid = lstp;//当前查询位置
					//遍历未查询过的数据
					for (ufrp = lstp->next; ufrp != NULL; ufrp = ufrp->next)
					{
						//记录x最小的位置
						if ((mid->udx > ufrp->udx))
						{
							mid = ufrp;
						}
					}
					//交互数据
					if (mid != lstp)
					{
						//exCD = mid
						exCD.xmin = mid->xmin;	exCD.ymax = mid->ymax;
						exCD.dx = mid->dx;		exCD.udx = mid->udx;
						//mid = lstp
						mid->xmin = lstp->xmin;	mid->ymax = lstp->ymax;
						mid->dx = lstp->dx;		mid->udx = lstp->udx;
						//lstp = exCD
						lstp->xmin = exCD.xmin; lstp->ymax = exCD.ymax;
						lstp->dx = exCD.dx;		lstp->udx = exCD.udx;
					}
				}

				//填充区域
				if (pNET != NULL)
				{
					lstp = pNET;
					ufrp = pNET->next;
					lstp->xmin = (short)YMGRE_Ceil(lstp->udx);//向上取整
					joflg = 0;//绘制标识
					while (ufrp)
					{
						//数据取整
						if (joflg)//向上取整
						{
							ufrp->xmin = (short)YMGRE_Ceil(ufrp->udx);
						}
						else//向下取整
						{
							ufrp->xmin = (short)YMGRE_Floor(ufrp->udx);
						}
						joflg = !joflg; //两个点一条线段

						//记录要绘制的线段
						if (joflg)
						{
							if (npAET[i] == NULL)//第一条线段
							{
								gre_flet* thisAET = GRE_malloc1(sizeof(gre_flet));//添加一条直线
								gre_log_explain(thisAET == NULL, GRE_LOG_Mem1, "thisAET计算缓存申请失败");
								thisAET->xmin = lstp->xmin;
								thisAET->ymax = ufrp->xmin;
								thisAET->next = NULL;
								mid = thisAET;

								npAET[i] = thisAET;
							}
							else
							{
								mid->next = GRE_malloc1(sizeof(gre_flet));//添加一条直线
								mid = mid->next;
								gre_log_explain(mid == NULL, GRE_LOG_Mem1, "mid计算缓存申请失败");
								mid->xmin = lstp->xmin;
								mid->ymax = ufrp->xmin;
								mid->next = NULL;
							}
						}

						//跳到下一个端
						lstp = ufrp;
						ufrp = lstp->next;
					}
				}
			}
		}

		//释放AET相关内存
		for (i = 0; i < height; i++)
		{
			gre_flet* thisAET = pAET[i];
			while (thisAET != NULL)
			{
				ufrp = thisAET->next;
				GRE_free1(thisAET);
				thisAET = ufrp;
			}
		}
		GRE_free1(pAET);
		GRE_free1(plET);

		//y直线填充绘制，画上线段，释放内存
		for (i = 0; i < height; i++)
		{
			if (npAET[i] != NULL)
			{
				float32* zbuff_i = &zdeep[i * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &datap[i * width];//帧缓冲区中，该行起点
				ufrp = npAET[i];
				while (ufrp)
				{
					// x,y =  j,i
					// 将x,y带入方程，计算得到 z = -(Ax+By+D)/C
					int32 begX = GREMax(ufrp->xmin, 0);
					int32 endX = GREMin(ufrp->ymax, width - 1);
					float32 zval_L = -(pN->x * begX + pN->y * i + pN->w) / pN->z;//左端点深度
					float32 zadd = -(pN->x / pN->z);//z(x+1,y) = z(x,y)-A/C
					for (j = begX; j <= endX; j++)//在两段间画上线段
					{
						//Z-buff比较，若距离变小则更新缓存
						if (zbuff_i[j]> zval_L)
						{
							zbuff_i[j] = zval_L;
						frame_i[j] = GRE_FramePixel_From_RGB24(fillcolor);
						}
						zval_L += zadd;
					}
					lstp = ufrp;
					ufrp = ufrp->next;
					GRE_free1(lstp);//释放一条直线
				}
		}
	}
		GRE_free1(npAET);

	}
	GRE_free1(linesET);
}
#endif

/////////////////////////////////////////// 平面着色器 -- 多边形光栅化//////////////////////////////////////////////////////////////

//图元光栅化
void YMGRE_PolygonObject_Primitive_Rasterization(GRE_Object4d myobj, GRE_Camera4d mycam,uint8 showLines)
{
	gre_flineslist thislines;
	gre_fline linesbuff[10]; //最多10条边
	thislines.lineMax = 10;
	thislines.data = linesbuff; //内存

	gre_lineslist clipedlines;
	gre_line drawlinebuff[10]; //最多10条边 ,条数需 > thislines.lineMax
	clipedlines.lineNum = 10;
	int clipoutnumMax = 10;
	clipedlines.data = drawlinebuff;//内存
	gre_scanedge scanEdges[10];
	gre_scanedge* activeEdges[10];
	gre_scanline_workspace scanWorkspace = { scanEdges, activeEdges, 10 };

	gre_log_explain((myobj == NULL) || (mycam == NULL), GRE_LOG_PtrI, "输入的物体或相机不存在");
	gre_frect myrec; //矩形框
	myrec.x0 =0;//xmin
	myrec.y0 = 0;//ymin
	myrec.x1 = mycam->img.width - 1;//xmax
	myrec.y1 = mycam->img.height - 1;//ymax

	//遍历物体
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];//取出该面
		if (thispoly->ishide) //被隐藏
			continue;
		int linesnum = thispoly->num;
		gre_log_explain((linesnum >= thislines.lineMax), GRE_LOG_ParamI, "多边形边数必须<10");
		thislines.lineNum = linesnum;//记录直线条数
		clipedlines.lineNum = clipoutnumMax;//重置最大参数
		//依次取出各边，构建多边形
		for (int j = 0; j < linesnum; j++)
		{
			int i1 = thispoly->index[j];
			int i2;
			if (j == (linesnum - 1))
				i2 = thispoly->index[0];
			else
			{
				i2 = thispoly->index[j + 1]; //完成闭环
			}
			thislines.data[j].x0 = myobj->pointList_[i1].pos.x;
			thislines.data[j].y0 = myobj->pointList_[i1].pos.y;
			thislines.data[j].x1 = myobj->pointList_[i2].pos.x;
			thislines.data[j].y1 = myobj->pointList_[i2].pos.y;
		}

		//进行边界裁剪
		YMGRE_Polygon_clip2D(&thislines,&clipedlines, &myrec);
		gre_fvector4d pN;
		uint8 hasPlane = 0;
		//绘制填充多边形
		if (clipedlines.lineNum > 0) //存在多边形时
		{
			//平面一般方程 Ax+By+Cz+D=0;
			//平面法向量pN = {A,B,C} ,将点(x,y,z)带入可求D

			GRE_Vertex4d tpoints = myobj->pointList_; //取透视变换后的点
			hasPlane = YMGRE_Raster_PolygonPlane(tpoints, thispoly, &pN);
			if (hasPlane)
			{
				//进行扫描线填充 + z - buff滤除
				YMGRE_Img_Scanline_AreaFillWithWorkspace(mycam->img.data, mycam->img.width,
					mycam->img.height, &clipedlines, &pN, mycam->img.zbuff,
					thispoly->planeColor_, &scanWorkspace);
			}
		}

		if (showLines && hasPlane)
		{
			//绘制线框
			for (int j = 0; j < clipedlines.lineNum; j++)
			{
				YMGRE_Img_PolygonLine(mycam->img.data, mycam->img.width, mycam->img.zbuff,
					&pN, clipedlines.data[j].x0, clipedlines.data[j].y0,
					clipedlines.data[j].x1, clipedlines.data[j].y1);
			}
		}
	}
}

void YMGRE_PolygonObject_Primitive_RasterizationTo(GRE_Object4d myobj, GRE_Vertex4d points, uint8* polygonHide,
	GRErgb24* polygonColor, GRE_Camera4d mycam, uint8 showLines)
{
	gre_flineslist thislines;
	gre_fline linesbuff[10]; //最多10条边
	thislines.lineMax = 10;
	thislines.data = linesbuff;
	gre_lineslist clipedlines;
	gre_line drawlinebuff[10]; //最多10条边
	clipedlines.lineNum = 10;
	clipedlines.data = drawlinebuff;
	gre_scanedge scanEdges[10];
	gre_scanedge* activeEdges[10];
	gre_scanline_workspace scanWorkspace = { scanEdges, activeEdges, 10 };
	gre_log_explain((myobj == NULL) || (points == NULL) || (polygonHide == NULL) ||
		(polygonColor == NULL) || (mycam == NULL), GRE_LOG_PtrIO, "输入的物体、工作区或相机不存在");
	gre_frect myrec = { 0, 0, mycam->img.width - 1, mycam->img.height - 1 };

	for (int i = 0; i < myobj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		if (polygonHide[i])
			continue;
		int linesnum = thispoly->num;
		gre_log_explain(linesnum >= thislines.lineMax, GRE_LOG_ParamI, "多边形边数必须<10");
		thislines.lineNum = linesnum;
		clipedlines.lineNum = 10;
		for (int j = 0; j < linesnum; j++)
		{
			int i1 = thispoly->index[j];
			int i2 = thispoly->index[(j + 1) % linesnum];
			thislines.data[j].x0 = points[i1].pos.x;
			thislines.data[j].y0 = points[i1].pos.y;
			thislines.data[j].x1 = points[i2].pos.x;
			thislines.data[j].y1 = points[i2].pos.y;
		}

		YMGRE_Polygon_clip2D(&thislines, &clipedlines, &myrec);
		gre_fvector4d pN;
		uint8 hasPlane = 0;
		if (clipedlines.lineNum > 0)
		{
			hasPlane = YMGRE_Raster_PolygonPlane(points, thispoly, &pN);
			if (hasPlane)
			{
				YMGRE_Img_Scanline_AreaFillWithWorkspace(mycam->img.data, mycam->img.width,
					mycam->img.height, &clipedlines, &pN, mycam->img.zbuff,
					polygonColor[i], &scanWorkspace);
			}
		}

		if (showLines && hasPlane)
		{
			for (int j = 0; j < clipedlines.lineNum; j++)
			{
				YMGRE_Img_PolygonLine(mycam->img.data, mycam->img.width, mycam->img.zbuff,
					&pN, clipedlines.data[j].x0, clipedlines.data[j].y0,
					clipedlines.data[j].x1, clipedlines.data[j].y1);
			}
		}
	}
}

// 灯光绘制
void YMGRE_Light_Primitive_Rasterization(GRE_Light4d mylight, GRE_Camera4d mycam, uint8 showLightSize)
{

	if (showLightSize == 0) //无需显示
		return;

	gre_log_explain((mylight == NULL) || (mycam == NULL), GRE_LOG_PtrI, "输入的灯光或相机不存在");

	if (mylight->type == GRE_GlobalLight) //非全局光照才绘制
		return;
	//     B   C
	//  A    *    D
	//     F   E
	// (x,y)坐标是
	// A(-2，0), B(-1，-√3),C(1，-√3)
	// D(2，0),  E(1，√3),  F(-1，√3)
	float32 sexangleXY[6][2] = { {-2,0}, {-1,-1.732},  {1,-1.732}, {2,0}, {1,1.732},  {-1,1.732} };

	gre_flineslist sexangle;
	sexangle.lineNum = 6;
	sexangle.lineMax = 6;//最大边数
	gre_fline saglines[6];
	sexangle.data = saglines;
	//构建直线段
	for (int i = 0; i < 6; i++)
	{
		int j = i + 1;
		if (j >= 6)  j -= 6;
		//记录直线
		saglines[i].x0 = showLightSize * sexangleXY[i][0] + mylight->proper.pos_.x;
		saglines[i].y0 = showLightSize * sexangleXY[i][1] + mylight->proper.pos_.y;

		saglines[i].x1 = showLightSize * sexangleXY[j][0] + mylight->proper.pos_.x;
		saglines[i].y1 = showLightSize * sexangleXY[j][1] + mylight->proper.pos_.y;
	}

	//输出多边形
	gre_lineslist clipedlines;
	gre_line drawlinebuff[10]; //最多10条边 ,正常最多8条
	clipedlines.lineNum = 10;
	clipedlines.data = drawlinebuff;
	gre_scanedge scanEdges[10];
	gre_scanedge* activeEdges[10];
	gre_scanline_workspace scanWorkspace = { scanEdges, activeEdges, 10 };

    //裁剪矩形框
	gre_frect myrec;
	myrec.x0 = 0;//xmin
	myrec.y0 = 0;//ymin
	myrec.x1 = mycam->img.width - 1;//xmax
	myrec.y1 = mycam->img.height - 1;//ymax

	//进行边界裁剪
	YMGRE_Polygon_clip2D(&sexangle, &clipedlines, &myrec);
	//绘制填充多边形
	if (clipedlines.lineNum > 0) //存在多边形时
	{
		//计算平面一般方程 Ax + By + Cz + D = 0
		gre_fvector4d pN = { 0 }; //平面法向量
		pN.z = 1.0f; //垂直屏幕
        //实际上由于变换到左手坐标系上，方向会发生改变，但此处仅用于求平面方程，并不关心方向问题

		//将x0,y0,z0带入方程，计算参数 D = -（Ax+By+Cz）,放在 w上
		pN.w = -(pN.x * mylight->proper.pos_.x + pN.y * mylight->proper.pos_.y + pN.z * mylight->proper.pos_.z);

		//进行扫描线填充 + z - buff滤除
		YMGRE_Img_Scanline_AreaFillWithWorkspace(mycam->img.data, mycam->img.width, mycam->img.height, &clipedlines, &pN, mycam->img.zbuff, mylight->proper.lightcolor, &scanWorkspace);
	}
}

//三角形图元光栅化
void YMGRE_TrangleObject_Primitive_Rasterization(GRE_Object4d myTrangleObj, GRE_Material mymaterial, GRE_Camera4d mycam)
{
	gre_log_explain((myTrangleObj == NULL) || (mycam == NULL), GRE_LOG_PtrI, "输入的物体或相机不存在");

	//第一遍统一填充全部片元，防止后绘制片元覆盖已经画好的共享边
	for (int i = 0; i < myTrangleObj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myTrangleObj->polygonList[i];
		if (thispoly->ishide)
			continue;
		gre_log_explain(thispoly->num != 3, GRE_LOG_ParamI, "多边形边数必须为3（三角形）");
		YMGRE_TriangleRaster_Fill(myTrangleObj->pointList_, thispoly,
			thispoly->planeColor_, mymaterial, mycam);
	}
	//第二遍统一后画线框，同深度线框覆盖填充颜色
	if (myTrangleObj->wireFrame)
		YMGRE_TrangleObject_Wires(myTrangleObj, mycam);
}

void YMGRE_TrangleObject_Primitive_RasterizationTo(GRE_Object4d myTrangleObj, GRE_Vertex4d points, uint8* polygonHide,
	GRErgb24* polygonColor, GRE_Material mymaterial, GRE_Camera4d mycam)
{
	gre_log_explain((myTrangleObj == NULL) || (points == NULL) || (polygonHide == NULL) ||
		(polygonColor == NULL) || (mycam == NULL), GRE_LOG_PtrIO, "输入的物体、工作区或相机不存在");
	for (int i = 0; i < myTrangleObj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myTrangleObj->polygonList[i];
		if (polygonHide[i])
			continue;
		gre_log_explain(thispoly->num != 3, GRE_LOG_ParamI, "多边形边数必须为3（三角形）");
		YMGRE_TriangleRaster_Fill(points, thispoly, polygonColor[i], mymaterial, mycam);
	}
	if (myTrangleObj->wireFrame)
		YMGRE_TrangleObject_WiresTo(myTrangleObj, points, polygonHide, mycam);
}

void YMGRE_TrangleObject_Primitive_Rasterization_wN(GRE_Object4d object, GRE_Vertex4d_wN points,
	uint8* polygonHide, GRE_Material material, GRE_List lights, gre_fvector4d* lightPos,
	GRE_FMat4x4 worldToCamera, GRE_Camera4d camera)
{
	if (object == NULL || points == NULL || polygonHide == NULL || camera == NULL) return;
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		if (!polygonHide[i] && polygon->num == 3)
			YMGRE_TriangleRaster_Fill_wN(points, polygon, material, lights, lightPos, worldToCamera,
				object->mirrorKs, camera);
	}
}

void YMGRE_TrangleObject_Primitive_Rasterization_VertexColor_wN(GRE_Object4d object,
	GRE_Vertex4d_wN points,uint8* polygonHide,GRE_Camera4d camera)
{
	if(object==NULL||points==NULL||polygonHide==NULL||camera==NULL) return;
	for(int i=0;i<object->polygonNum;i++)
	{
		GRE_Polygon4d polygon=&object->polygonList[i];
		if(!polygonHide[i]&&polygon->num==3)
			YMGRE_TriangleRaster_FillVertexColor_wN(points,polygon,camera);
	}
}

//三角网格线也参与深度测试，避免被遮挡的内部边覆盖前景表面
static void YMGRE_Raster_TriangleWire(GRE_FrameBuffer data, uint16 width, uint16 height,
	float32* zbuff, GRE_Vertex4d points, GRE_Polygon4d polygon,
	int16 x1, int16 y1, int16 x2, int16 y2)
{
	gre_fvector4d u;
	gre_fvector4d v;
	gre_fvector4d plane;
	GRE_Vertex4d p0 = &points[polygon->index[0]];
	GRE_Vertex4d p1 = &points[polygon->index[1]];
	GRE_Vertex4d p2 = &points[polygon->index[2]];
	YMGRE_Fvector4d_SubToResult(&p1->pos, &p0->pos, &u);
	YMGRE_Fvector4d_SubToResult(&p2->pos, &p0->pos, &v);
	YMGRE_Fvector4d_CrossToResult(&u, &v, &plane);
	plane.w = -(plane.x * p0->pos.x + plane.y * p0->pos.y + plane.z * p0->pos.z);
	if ((plane.z <= YMGRE_RASTER_AREA_EPSILON) && (plane.z >= -YMGRE_RASTER_AREA_EPSILON))
		return;
	YMGRE_Img_TriangleLine(data, width, zbuff, &plane, x1, y1, x2, y2);
}

//三角图元线框绘制
void YMGRE_TrangleObject_Wires(GRE_Object4d myTrangleObj, GRE_Camera4d mycam)
{
	gre_flineslist thislines;
	gre_fline linesbuff[3]; //最多3条边
	thislines.lineMax = 3;
	thislines.data = linesbuff; //内存

	gre_lineslist clipedlines;
	gre_line drawlinebuff[6]; //最多6条边 ,条数需 > thislines.lineMax
	clipedlines.lineNum = 6;
	int clipoutnumMax = 6;
	clipedlines.data = drawlinebuff;//内存

	gre_log_explain((myTrangleObj == NULL) || (mycam == NULL), GRE_LOG_PtrI, "输入的物体或相机不存在");
	gre_frect myrec; //矩形框
	myrec.x0 = 0;//xmin
	myrec.y0 = 0;//ymin
	myrec.x1 = mycam->img.width - 1;//xmax
	myrec.y1 = mycam->img.height - 1;//ymax

	//遍历物体
	for (int i = 0; i < myTrangleObj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myTrangleObj->polygonList[i];//取出该面
		if (thispoly->ishide) //被隐藏
			continue;
		int linesnum = thispoly->num;
		gre_log_explain((linesnum != 3), GRE_LOG_ParamI, "多边形边数必须为3（三角形）");
		thislines.lineNum = linesnum;//记录直线条数
		clipedlines.lineNum = clipoutnumMax;//重置最大参数
		//依次取出各边，构建多边形
		for (int j = 0; j < linesnum; j++)
		{
			int i1 = thispoly->index[j];
			int i2;
			if (j == (linesnum - 1))
				i2 = thispoly->index[0];
			else
			{
				i2 = thispoly->index[j + 1]; //完成闭环
			}
			thislines.data[j].x0 = myTrangleObj->pointList_[i1].pos.x;
			thislines.data[j].y0 = myTrangleObj->pointList_[i1].pos.y;
			thislines.data[j].x1 = myTrangleObj->pointList_[i2].pos.x;
			thislines.data[j].y1 = myTrangleObj->pointList_[i2].pos.y;
		}

		//进行边界裁剪
		YMGRE_Polygon_clip2D(&thislines, &clipedlines, &myrec);

		//绘制线框
		for (int j = 0; j < clipedlines.lineNum; j++)
		{
			YMGRE_Raster_TriangleWire(mycam->img.data, mycam->img.width,
				mycam->img.height, mycam->img.zbuff, myTrangleObj->pointList_,
				thispoly, clipedlines.data[j].x0, clipedlines.data[j].y0,
				clipedlines.data[j].x1, clipedlines.data[j].y1);
		}
	}
}

//使用外部顶点和剔除状态绘制三角网格，不修改物体上的兼容工作字段
void YMGRE_TrangleObject_WiresTo(GRE_Object4d myTrangleObj, GRE_Vertex4d points,
	uint8* polygonHide, GRE_Camera4d mycam)
{
	gre_flineslist thislines;
	gre_fline linesbuff[3];
	thislines.lineMax = 3;
	thislines.data = linesbuff;
	gre_lineslist clipedlines;
	gre_line drawlinebuff[6];
	clipedlines.lineNum = 6;
	clipedlines.data = drawlinebuff;
	gre_log_explain((myTrangleObj == NULL) || (points == NULL) || (polygonHide == NULL) || (mycam == NULL), GRE_LOG_PtrIO, "输入的物体、工作区或相机不存在");
	gre_frect myrec = { 0, 0, mycam->img.width - 1, mycam->img.height - 1 };

	for (int i = 0; i < myTrangleObj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myTrangleObj->polygonList[i];
		if (polygonHide[i])
			continue;
		gre_log_explain(thispoly->num != 3, GRE_LOG_ParamI, "多边形边数必须为3（三角形）");
		thislines.lineNum = thispoly->num;
		clipedlines.lineNum = 6;
		for (int j = 0; j < thispoly->num; j++)
		{
			int i1 = thispoly->index[j];
			int i2 = thispoly->index[(j + 1) % thispoly->num];
			thislines.data[j].x0 = points[i1].pos.x;
			thislines.data[j].y0 = points[i1].pos.y;
			thislines.data[j].x1 = points[i2].pos.x;
			thislines.data[j].y1 = points[i2].pos.y;
		}
		YMGRE_Polygon_clip2D(&thislines, &clipedlines, &myrec);
		for (int j = 0; j < clipedlines.lineNum; j++)
		{
			YMGRE_Raster_TriangleWire(mycam->img.data, mycam->img.width,
				mycam->img.height, mycam->img.zbuff, points, thispoly,
				clipedlines.data[j].x0, clipedlines.data[j].y0,
				clipedlines.data[j].x1, clipedlines.data[j].y1);
		}
	}
}

void YMGRE_TrangleObject_Wires_wN(GRE_Object4d object, GRE_Vertex4d_wN points,
	uint8* polygonHide, GRE_Camera4d camera)
{
	if (object == NULL || points == NULL || polygonHide == NULL || camera == NULL) return;
	for (int i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon=&object->polygonList[i];
		if (polygonHide[i]) continue;
		for (int j=0;j<polygon->num;j++)
		{
			GRE_Vertex4d_wN a=&points[polygon->index[j]], b=&points[polygon->index[(j+1)%polygon->num]];
			YMGRE_Img_LineDepth(camera->img.data,camera->img.zbuff,camera->img.width,camera->img.height,
				(int16)a->base.pos.x,(int16)a->base.pos.y,a->base.pos.z,(int16)b->base.pos.x,(int16)b->base.pos.y,b->base.pos.z,GRE_brush,1);
		}
	}
}
