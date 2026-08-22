#include "./YMGRE_TriangleRaster.h"
#include "./YMGRE_MathBase.h"

extern GRErgb24 GRE_brush;

#define YMGRE_RASTER_AREA_EPSILON 1e-5f

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

/////////////////////////////////////////// 平面着色器 --三角形快速光栅化//////////////////////////////////////////////////////////////

//获取贴图材质的颜色
static inline GRErgb24 EMaterial_GetPixel(GRErgb24* bitmap, uint16 width, uint16 height, float32 u, float32 v)
{
	if (bitmap)
	{
		// 这是一个更精确的方法, 但是效率低一点
		int x = YMGRE_Fabs(u - (int)u) * width;
		int y = YMGRE_Fabs(v - (int)v) * height;
		return bitmap[y * width + x];
	}
	else
		return DefaultPolygonColor;
}
//////////////////////////////////////////////// 使用材质绘制三角形 /////////////////////////////////////

// 绘制平底为下三角的三角形
//       v0
//       /\
//      /  \
//  v1 ------ v2
//只填充平底三角形，热路径不包含线框判断
static inline void Fill_Top_Trangle(float32 x0, float32 y0, float32 z0, float32 u0, float32 v0,
	float32 x1, float32 y1, float32 z1, float32 u1, float32 v1,
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, GRE_Camera4d mycam)
{
	if (y2 < 0 || y0 > mycam->img.height - 1)// 高度在图像范围之外
		return;

	uint16 height = mycam->img.height;
	uint16 width = mycam->img.width;
	float32 znear_v = mycam->frustum.Znear;

	float32 div10 = 1.0f / (y1 - y0);
	float32 div20 = 1.0f / (y2 - y0);
	//计算v0到两个顶点的直线增长量
	float32 dxdl = (x1 - x0) * div10; // dx L--R
	float32 dxdr = (x2 - x0) * div20;
	float32 dzdl = (z1 - z0) * div10; // dz L--R
	float32 dzdr = (z2 - z0) * div20;

	float32 startL = x0;
	float32 startR = x0;
	float32 zl = 0;
	float32 zr = 0;

	int begX;
	int endX;
	//不使用材质，则采用平面着色
	if ((mymater == NULL) || (mymater->pixel == NULL) || !mymater->valid)
	{
		//扫描线采用左闭右闭、下边界不包含规则，避免填充越过三角形边界
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x0 + (begY - y0) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		zl = z0 + (begY - y0) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			if (begX > endX)
			{
				startL += dxdl;
				startR += dxdr;
				zl += dzdl;
				zr += dzdr;
				continue;
			}
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zd = (startL == startR) ? 0 : (zr - zl) / (startR - startL);
			float32 zval = zl + (begX - startL) * zd;
			//填充固定颜色
			for (int x = begX; x <= endX; x++)
			{
				//Z-buff比较，若距离变小则更新缓存
				if (zbuff_i[x] > zval)
				{
					//且在近景平面内
					if (zval > znear_v)
					{
						zbuff_i[x] = zval;
						frame_i[x] = GRE_FramePixel_From_RGB24(planecolor);
					}
				}
				zval += zd;
			}
			startL += dxdl; //dx
			startR += dxdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
	else
	{
		float32 dudl = (u1 - u0) * div10; //du
		float32 dudr = (u2 - u0) * div20;
		float32 dvdl = (v1 - v0) * div10; //dv
		float32 dvdr = (v2 - v0) * div20;

		float32 startLU = u0;// L -- R
		float32 startRU = u0;
		float32 startLV = v0;
		float32 startRV = v0;

		//
		float32 begU = 0; // B -- E
		float32 endU = 0;
		float32 begV = 0;
		float32 endV = 0;

		float32 dx = 0;
		float32 ui = 0;
		float32 vi = 0;

		float32 zl = 0;
		float32 zr = 0;
		float32 zval = 0;

		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x0 + (begY - y0) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		startLU = u0 + (begY - y0) * dudl;
		startRU = u0 + (begY - y0) * dudr;
		startLV = v0 + (begY - y0) * dvdl;
		startRV = v0 + (begY - y0) * dvdr;
		zl = z0 + (begY - y0) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			//初始化 L -- R
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			//计算水平方向插值增量
			dx = startR - startL;
			ui = (dx == 0) ? 0 : (endU - begU) / dx;
			vi = (dx == 0) ? 0 : (endV - begV) / dx;
			float32 zd = (dx == 0) ? 0 : (zr - zl) / dx;
			begU += (begX - startL) * ui;
			begV += (begX - startL) * vi;
			zval = zl + (begX - startL) * zd;
			//修正x的范围
			if (begX < 0)
			{
				begU -= begX * ui;
				begV -= begX * vi;
				zval -= begX * zd;
				begX = 0;
			}
			if (endX > width - 1)
				endX = width - 1;
			//在图像范围内
			if (begX <= endX)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				for (int x = begX; x <= endX; x++)
				{
					//Z-buff比较，若距离变小则更新缓存
					if (zbuff_i[x] > zval)
					{
						//且在近景平面内
						if (zval > znear_v)
						{
							zbuff_i[x] = zval;
							GRErgb24 texel = EMaterial_GetPixel(mymater->pixel, mymater->width, mymater->height, begU, begV);//getPixel(begU, begV)
							//添加光照影响，最后一步才量化到帧缓冲格式
							int cr = texel.R * planecolor.R / DefaultPolygonClv;
							int cg = texel.G * planecolor.G / DefaultPolygonClv;
							int cb = texel.B * planecolor.B / DefaultPolygonClv;
							texel.R = GREMin(cr, 255);
							texel.G = GREMin(cg, 255);
							texel.B = GREMin(cb, 255);
							frame_i[x] = GRE_FramePixel_From_RGB24(texel);
						}
					}
					begU += ui; begV += vi;
					zval += zd;
				}
			}
			//L,R
			startL += dxdl; startR += dxdr;
			//u,v
			startLU += dudl; startLV += dvdl;
			startRU += dudr; startRV += dvdr;
			zl += dzdl; zr += dzdr;
		}
	}
}

// 绘制下三角
//  v1     v0
//   ------
//    \  /
//     \/
//     v2
//只填充平顶三角形，热路径不包含线框判断
static inline void Fill_Botton_Trangle(float32 x0, float32 y0, float32 z0, float32 u0, float32 v0,
	float32 x1, float32 y1, float32 z1, float32 u1, float32 v1,
	float32 x2, float32 y2, float32 z2, float32 u2, float32 v2, GRErgb24 planecolor, GRE_Material mymater, GRE_Camera4d mycam)
{
	if (y2 < 0 || y0 > mycam->img.height - 1)// 高度在图像范围之外
		return;

	uint16 height = mycam->img.height;
	uint16 width = mycam->img.width;
	float32 znear_v = mycam->frustum.Znear;
	//通过绘制水平直线来完成
	float32 dxdl = (x1 - x2) / (y1 - y2);//dx
	float32 dxdr = (x0 - x2) / (y0 - y2);
	float32 dzdl = (z1 - z2) / (y1 - y2);//dz
	float32 dzdr = (z0 - z2) / (y0 - y2);

	float32 startL = x1;
	float32 startR = x0;
	int begX = 0;
	int endX = 0;
	float32 zl = 0;
	float32 zr = 0;
	// 没有材质则使用平面着色
	if ((mymater == NULL) || (mymater->pixel == NULL) || !mymater->valid)
	{
		//扫描线采用左闭右闭、下边界不包含规则，避免填充越过三角形边界
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x1 + (begY - y1) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		zl = z1 + (begY - y1) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			if (begX > endX)
			{
				startL += dxdl;
				startR += dxdr;
				zl += dzdl;
				zr += dzdr;
				continue;
			}
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zd = (startL == startR) ? 0 : (zr - zl) / (startR - startL);
			float32 zval = zl + (begX - startL) * zd;
			//填充固定颜色
			for (int x = begX; x <= endX; x++)
			{
				//Z-buff比较，若距离变小则更新缓存
				if (zbuff_i[x] > zval)
				{
					//且在近景平面内
					if (zval > znear_v)
					{
						zbuff_i[x] = zval;
						frame_i[x] = GRE_FramePixel_From_RGB24(planecolor);
					}
				}
				zval += zd;
			}
			startL += dxdl; //dx
			startR += dxdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
	else
	{
		float32 dudl = (u1 - u2) / (y1 - y2);// du
		float32 dudr = (u0 - u2) / (y0 - y2);
		float32 dvdl = (v1 - v2) / (y1 - y2);// dv
		float32 dvdr = (v0 - v2) / (y0 - y2);

		float32 startLU = u1;// U  L -- R
		float32 startRU = u0;
		float32 startLV = v1;// V  L -- R
		float32 startRV = v0;

		float32 begU = 0;
		float32 endU = 0;
		float32 begV = 0;
		float32 endV = 0;

		float32 dx = 0;
		float32 ui = 0;
		float32 vi = 0;

		float32 zval = 0;
		int begY = GREMax(YMGRE_Raster_Ceil(y0), 0);
		int endY = GREMin(YMGRE_Raster_Ceil(y2), height);
		startL = x1 + (begY - y1) * dxdl;
		startR = x0 + (begY - y0) * dxdr;
		startLU = u1 + (begY - y1) * dudl;
		startRU = u0 + (begY - y0) * dudr;
		startLV = v1 + (begY - y1) * dvdl;
		startRV = v0 + (begY - y0) * dvdr;
		zl = z1 + (begY - y1) * dzdl;
		zr = z0 + (begY - y0) * dzdr;

		for (int y = begY; y < endY; y++)
		{
			//初始化 L -- R
			begX = YMGRE_Raster_Ceil(startL);
			endX = YMGRE_Raster_Floor(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			//计算水平方向插值增量
			dx = startR - startL;
			ui = (dx == 0) ? 0 : (endU - begU) / dx;
			vi = (dx == 0) ? 0 : (endV - begV) / dx;
			float32 zd = (dx == 0) ? 0 : (zr - zl) / dx;
			begU += (begX - startL) * ui;
			begV += (begX - startL) * vi;
			zval = zl + (begX - startL) * zd;
			//修正x的范围
			if (begX < 0)
			{
				begU -= begX * ui;
				begV -= begX * vi;
				zval -= begX * zd;
				begX = 0;
			}
			if (endX > width - 1)
				endX = width - 1;
			//在图像范围内
			if (begX <= endX)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				for (int x = begX; x <= endX; x++)
				{
					//Z-buff比较，若距离变小则更新缓存
					if (zbuff_i[x] > zval)
					{
						//且在近景平面内
						if (zval > znear_v)
						{
							zbuff_i[x] = zval;
							GRErgb24 texel = EMaterial_GetPixel(mymater->pixel, mymater->width, mymater->height, begU, begV);//getPixel(begU, begV)
							//添加光照影响，最后一步才量化到帧缓冲格式
							int cr = texel.R * planecolor.R / DefaultPolygonClv;
							int cg = texel.G * planecolor.G / DefaultPolygonClv;
							int cb = texel.B * planecolor.B / DefaultPolygonClv;
							texel.R = GREMin(cr, 255);
							texel.G = GREMin(cg, 255);
							texel.B = GREMin(cb, 255);
							frame_i[x] = GRE_FramePixel_From_RGB24(texel);
						}
					}
					begU += ui; begV += vi;
					zval += zd;
				}
			}
			startL += dxdl;
			startR += dxdr;

			startLU += dudl;
			startLV += dvdl;
			startRU += dudr;
			startRV += dvdr;
			zl += dzdl;
			zr += dzdr;
		}
	}
}

typedef struct triangle_split_
{
	GRE_Vertex4d top;
	GRE_Vertex4d left;
	GRE_Vertex4d right;
	GRE_Vertex4d bottom;
	gre_vertex4d middle;
	uint8 type;
}gre_triangle_split;

//排序三角形顶点并计算上下三角形共用的分割点
static uint8 YMGRE_TriangleRaster_Split(GRE_Vertex4d vertexList,
	GRE_Polygon4d polygon, gre_triangle_split* split)
{
	GRE_Vertex4d v0 = &vertexList[polygon->index[0]];
	GRE_Vertex4d v1 = &vertexList[polygon->index[1]];
	GRE_Vertex4d v2 = &vertexList[polygon->index[2]];
	GRE_Vertex4d temp;
	float32 area = (v1->pos.x - v0->pos.x) * (v2->pos.y - v0->pos.y) -
		(v1->pos.y - v0->pos.y) * (v2->pos.x - v0->pos.x);
	if ((area <= YMGRE_RASTER_AREA_EPSILON) && (area >= -YMGRE_RASTER_AREA_EPSILON))
		return 0;

#define GRE_SWAP_POINT(a,b) {temp = a; a = b; b = temp;}
	if (v1->pos.y < v0->pos.y)
		GRE_SWAP_POINT(v0, v1);
	if (v2->pos.y < v0->pos.y)
		GRE_SWAP_POINT(v0, v2);
	if (v2->pos.y < v1->pos.y)
		GRE_SWAP_POINT(v1, v2);
	if (v0->pos.y == v2->pos.y)
		return 0;

	if (v1->pos.y == v2->pos.y)
	{
		split->top = v0;
		split->left = (v1->pos.x <= v2->pos.x) ? v1 : v2;
		split->right = (v1->pos.x <= v2->pos.x) ? v2 : v1;
		split->type = 1;
	}
	else if (v0->pos.y == v1->pos.y)
	{
		split->left = (v0->pos.x <= v1->pos.x) ? v0 : v1;
		split->right = (v0->pos.x <= v1->pos.x) ? v1 : v0;
		split->bottom = v2;
		split->type = 2;
	}
	else
	{
		float32 factor = (v1->pos.y - v0->pos.y) / (v2->pos.y - v0->pos.y);
		split->middle.pos.x = v0->pos.x + factor * (v2->pos.x - v0->pos.x);
		split->middle.pos.y = v1->pos.y;
		split->middle.pos.z = v0->pos.z + factor * (v2->pos.z - v0->pos.z);
		split->middle.u = v0->u + factor * (v2->u - v0->u);
		split->middle.v = v0->v + factor * (v2->v - v0->v);
		split->top = v0;
		split->left = (v1->pos.x <= split->middle.pos.x) ? v1 : &split->middle;
		split->right = (v1->pos.x <= split->middle.pos.x) ? &split->middle : v1;
		split->bottom = v2;
		split->type = 3;
	}
#undef GRE_SWAP_POINT
	return 1;
}

//绘制纯填充三角形
void YMGRE_TriangleRaster_Fill(GRE_Vertex4d vertexList, GRE_Polygon4d polygon,
	GRErgb24 planecolor, GRE_Material material, GRE_Camera4d camera)
{
	gre_triangle_split split;
	if (!YMGRE_TriangleRaster_Split(vertexList, polygon, &split))
		return;
	if (split.type == 1)
		Fill_Top_Trangle(split.top->pos.x, split.top->pos.y, split.top->pos.z, split.top->u, split.top->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			planecolor, material, camera);
	else if (split.type == 2)
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, camera);
	else
	{
		Fill_Top_Trangle(split.top->pos.x, split.top->pos.y, split.top->pos.z, split.top->u, split.top->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			planecolor, material, camera);
		Fill_Botton_Trangle(split.right->pos.x, split.right->pos.y, split.right->pos.z, split.right->u, split.right->v,
			split.left->pos.x, split.left->pos.y, split.left->pos.z, split.left->u, split.left->v,
			split.bottom->pos.x, split.bottom->pos.y, split.bottom->pos.z, split.bottom->u, split.bottom->v,
			planecolor, material, camera);
	}
}


