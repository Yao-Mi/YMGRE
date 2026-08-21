#include "./YMGRE_Rasterization.h"
#include "./YMGRE_MathBase.h"
#include "../OPOBJ/YMGRE_Free.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"
#include "./YMGRE_CullingAndClipping.h"

GRErgb24 GRE_brush = { .R = 50,.G = 100,.B = 100 };
GRErgb24 GRE_background_color = { .R = 10,.G = 10,.B = 10 };
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
	uint16 t;
	int xerr = 0, yerr = 0, delta_x, delta_y, distance;
	int incx, incy;
	int uy, ux;
	gre_log_explain((x1 < 0) || (y1 < 0) || (x2 < 0) || (y2 < 0), GRE_LOG_ParamI, "输入的直线端点坐标<0");
	gre_log_explain((x1 >= width) || (y1 >= height) || (x2 >= width) || (y2 >= height), GRE_LOG_ParamI, "输入的直线端点坐标 >=边界条件");

	delta_x = x2 - x1; //计算坐标增量 
	delta_y = y2 - y1;
	ux = x1;
	uy = y1;
	if (delta_x > 0) //正向增长 
		incx = 1;
	else if (delta_x == 0)//垂直线 
		incx = 0;
	else //反向增长 
	{
		incx = -1;
		delta_x = -delta_x;
	}

	if (delta_y > 0)
		incy = 1;
	else if (delta_y == 0)//水平线 
		incy = 0;
	else
	{
		incy = -1;
		delta_y = -delta_y;
	}
	if (delta_x > delta_y)
		distance = delta_x; //选取基本增量坐标轴 
	else
		distance = delta_y;

	//Bresenham画线算法
	for (t = 0; t < distance + 2; t++)//画线输出 
	{
		data[uy*width + ux] = GRE_FramePixel_From_RGB24(GRE_brush);
		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance)
		{
			xerr -= distance;
			ux += incx;
		}

		if (yerr > distance)
		{
			yerr -= distance;
			uy += incy;
		}
	}
}

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

/**
  * @brief 多边形扫描填充算法 + zbuff
  */
void YMGRE_Img_Scanline_AreaFill(GRE_FrameBuffer datap, uint16 width, uint16 height, GRE_LinesList ring, GRE_Fvector4d pN, float32* zdeep, GRErgb24 fillcolor)
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
		//绘制填充多边形
		if (clipedlines.lineNum > 0) //存在多边形时
		{
			//平面一般方程 Ax+By+Cz+D=0; 
			//平面法向量pN = {A,B,C} ,将点(x,y,z)带入可求D
			
			GRE_Vertex4d tpoints = myobj->pointList_; //取透视变换后的点
			//计算平面法向量
			int i1 = thispoly->index[0];
			int i2 = thispoly->index[1];
			int i3 = thispoly->index[2];

			// 取 平面上的 两个向量
			GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&tpoints[i2].pos, &tpoints[i1].pos);
			GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&tpoints[i3].pos, &tpoints[i1].pos);

			//实际上由于变换到左手坐标系上，方向会发生改变，但此处仅用于求平面方程，并不关心方向问题
			GRE_Fvector4d pN = YMGRE_Fvector4d_Cross(u, v);//计算法向量 n=u×v 

			//将x0,y0,z0带入方程，计算参数 D = -（Ax+By+Cz）,放在 w上
			pN->w = -(pN->x * tpoints[i1].pos.x + pN->y * tpoints[i1].pos.y + pN->z * tpoints[i1].pos.z);
			
			//进行扫描线填充 + z - buff滤除
			YMGRE_Img_Scanline_AreaFill(mycam->img.data, mycam->img.width, mycam->img.height, &clipedlines,  pN, mycam->img.zbuff,thispoly->planeColor_);

			//释放向量内存
			YMGRE_Free_VectorF4d(u);
			YMGRE_Free_VectorF4d(v);
			YMGRE_Free_VectorF4d(pN);
		}

		if (showLines)
		{
			//绘制线框
			for (int j = 0; j < clipedlines.lineNum; j++)
			{
				YMGRE_Img_Line(mycam->img.data, mycam->img.width, mycam->img.height, clipedlines.data[j].x0, clipedlines.data[j].y0, clipedlines.data[j].x1, clipedlines.data[j].y1);
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
		YMGRE_Img_Scanline_AreaFill(mycam->img.data, mycam->img.width, mycam->img.height, &clipedlines, &pN, mycam->img.zbuff, mylight->proper.lightcolor);
	}
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
		//限制到图像范围内
		if (y0 < 0.0f) y0 = 0.0f;
		if (y2 > height - 1.0f) y2 = height - 1.0f;

		for (int y = (int)y0, ymx = (int)y2; y < ymx; y++)
		{

			begX = (int)(startL);
			endX = (int)(startR);
			//z
			zl = z0 + (y - y0) * dzdl;
			zr = z0 + (y - y0) * dzdr;

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zval = zl;
			float32 zd = (begX == endX) ? 0 : (zl - zr) / (begX - endX);
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

		for (int y = (int)y0, ymx = (int)y2; y < ymx; y++)
		{
			//初始化 L -- R
			begX = (int)(startL);
			endX = (int)(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			//z
			zl = z0 + (y - y0) * dzdl;
			zr = z0 + (y - y0) * dzdr;
			// 修正x的范围, 并修正贴图的u坐标
			if (begX < 0)
			{
				// 修正U
				if (startL != startR)
					begU = begU - begX * (startLU - startRU) / (startL - startR);
				begX = 0;
			}
			if (endX > width - 1)
			{
				// 修正U
				if (startL != startR)
					endU = endU - (endX - width) * (startLU - startRU) / (startL - startR);
				endX = width - 1;
			}
			dx = endX - begX;
			ui = (dx == 0) ? 0 : (endU - begU) / dx;
			vi = (dx == 0) ? 0 : (endV - begV) / dx;
			//在图像范围内
			if (y >= 0 && y < height)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				zval = zl;
				float32 zd = (begX == endX) ? 0 : (zl - zr) / (begX - endX);
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
		}
	}
}

// 绘制下三角
//  v1     v0
//   ------
//    \  /
//     \/
//     v2
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
		//限制到图像范围内
		if (y0 < 0.0f) y0 = 0.0f;
		if (y2 > height - 1.0f) y2 = height - 1.0f;

		for (int y = (int)y0, ymx = (int)y2; y < ymx; y++)
		{
			begX = (int)(startL);
			endX = (int)(startR);
			//z
			zl = z0 + (y - y0) * dzdl;
			zr = z0 + (y - y0) * dzdr;

			//水平方向限幅
			if (begX < 0) begX = 0;
			if (endX > width - 1)
				endX = width - 1;
			//-----------
			float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
			GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点

			float32 zval = zl;
			float32 zd = (begX == endX) ? 0 : (zl - zr) / (begX - endX);
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
		for (int y = (int)y0, ymx = (int)y2; y < ymx; y++)
		{
			//初始化 L -- R
			begX = (int)(startL);
			endX = (int)(startR);
			//u,v
			begU = startLU; endU = startRU;
			begV = startLV; endV = startRV;
			// 计算Z值
			zl = z0 + (y - y0) * dzdl;
			zr = z0 + (y - y0) * dzdr;

			// 修正x的范围, 并修正贴图的u坐标
			if (begX < 0)
			{
				// 修正U
				if (startL != startR)
					begU = begU - begX * (startLU - startRU) / (startL - startR);
				begX = 0;
			}
			if (endX > width - 1)
			{
				// 修正U
				if (startL != startR)
					endU = endU - (endX - width) * (startLU - startRU) / (startL - startR);
				endX = width - 1;
			}

			dx = endX - begX;
			ui = (dx == 0) ? 0 : (endU - begU) / dx; vi = (dx == 0) ? 0 : (endV - begV) / dx;

			//在图像范围内
			if (y >= 0 && y < height)
			{
				float32* zbuff_i = &mycam->img.zbuff[y * width];//深度缓冲器，该行起点
				GRE_FrameBuffer frame_i = &mycam->img.data[y * width];//帧缓冲区中，该行起点
				zval = zl;
				float32 zd = (begX == endX) ? 0 : (zl - zr) / (begX - endX);
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
		}
	}
}


// 绘制填充三角形
static inline void Fill_Trangle(GRE_Vertex4d vertexList, GRE_Polygon4d thispoly, GRE_Material mymaterial, GRE_Camera4d mycam)
{
	int id0 = thispoly->index[0];
	int id1 = thispoly->index[1];
	int id2 = thispoly->index[2];
	GRE_Vertex4d v0 = &vertexList[id0];
	GRE_Vertex4d v1 = &vertexList[id1];
	GRE_Vertex4d v2 = &vertexList[id2];
	GRE_Vertex4d exct = v0;

	// 保证 y0 < y1 < y2
#define GRE_SWAP(a,b) {exct = a; a = b; b = exct;}
	//确保 y0最小
	if (v1->pos.y < v0->pos.y)
	{
		GRE_SWAP(v0, v1);
	}
	if (v2->pos.y < v0->pos.y)
	{
		GRE_SWAP(v0, v2);
	}
	//确保 y1次小
	if (v2->pos.y < v1->pos.y)
	{
		GRE_SWAP(v1, v2);
	}
	//三个顶点在同一水平线上，没有可填充面积
	if (v0->pos.y == v2->pos.y)
		return;
	// 上三角
	if (v1->pos.y == v2->pos.y)
	{
		if (v2->pos.x < v1->pos.x)
		{
			GRE_SWAP(v1, v2);
		}
		//       v0
		//       /\
		//      /  \
		//  v1 ------ v2
		Fill_Top_Trangle(v0->pos.x, v0->pos.y, v0->pos.z, v0->u, v0->v, \
			v1->pos.x, v1->pos.y, v1->pos.z, v1->u, v1->v, \
			v2->pos.x, v2->pos.y, v2->pos.z, v2->u, v2->v, \
			thispoly->planeColor_, mymaterial, mycam);
	}
	else if (v0->pos.y == v1->pos.y) // 下三角
	{
		if (v1->pos.x > v0->pos.x)
		{
			GRE_SWAP(v0, v1);
		}
		//  v1     v0
		//   ------
		//    \  /
		//     \/
		//     v2
		Fill_Botton_Trangle(v0->pos.x, v0->pos.y, v0->pos.z, v0->u, v0->v, \
			v1->pos.x, v1->pos.y, v1->pos.z, v1->u, v1->v, \
			v2->pos.x, v2->pos.y, v2->pos.z, v2->u, v2->v, \
			thispoly->planeColor_, mymaterial, mycam);
	}
	else //分成两个三角形进行绘制
	{
		//计算y = y1与 v0->v2直线的交点
		float32 factor = (v1->pos.y - v0->pos.y) / (v2->pos.y - v0->pos.y);
		gre_vertex4d midv;
		midv.pos.x = v0->pos.x + factor * (v2->pos.x - v0->pos.x);
		midv.pos.y = v1->pos.y;
		midv.pos.z = v0->pos.z + factor * (v2->pos.z - v0->pos.z);
		midv.u = v0->u + factor * (v2->u - v0->u);
		midv.v = v0->v + factor * (v2->v - v0->v);
		GRE_Vertex4d midvp = &midv;
		if (midvp->pos.x < v1->pos.x)
		{
			GRE_SWAP(midvp, v1);
		}
		//       v0
		//      / |
		//	   /  |
		//  v1 ---|midvp
		//     \  |
		//      \ |
		//       v2
		//上三角绘制
		Fill_Top_Trangle(v0->pos.x, v0->pos.y, v0->pos.z, v0->u, v0->v, \
			v1->pos.x, v1->pos.y, v1->pos.z, v1->u, v1->v, \
			midvp->pos.x, midvp->pos.y, midvp->pos.z, midvp->u, midvp->v, \
			thispoly->planeColor_, mymaterial, mycam);
		//下三角绘制
		Fill_Botton_Trangle(midvp->pos.x, midvp->pos.y, midvp->pos.z, midvp->u, midvp->v, \
			v1->pos.x, v1->pos.y, v1->pos.z, v1->u, v1->v, \
			v2->pos.x, v2->pos.y, v2->pos.z, v2->u, v2->v, \
			thispoly->planeColor_, mymaterial, mycam);
	}
#undef GRE_SWAP
}

//三角形图元光栅化
void YMGRE_TrangleObject_Primitive_Rasterization(GRE_Object4d myTrangleObj, GRE_Material mymaterial, GRE_Camera4d mycam)
{
	gre_log_explain((myTrangleObj == NULL) || (mycam == NULL), GRE_LOG_PtrI, "输入的物体或相机不存在");

	//遍历物体
	for (int i = 0; i < myTrangleObj->polygonNum; i++)
	{
		GRE_Polygon4d thispoly = &myTrangleObj->polygonList[i];//取出该面
		if (thispoly->ishide) //被隐藏
			continue;
		int linesnum = thispoly->num;
		gre_log_explain((linesnum != 3), GRE_LOG_ParamI, "多边形边数必须为3（三角形）");

		//进行三角形绘制
		Fill_Trangle(myTrangleObj->pointList_, thispoly, mymaterial, mycam);
	}
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
			YMGRE_Img_Line(mycam->img.data, mycam->img.width, mycam->img.height, clipedlines.data[j].x0, clipedlines.data[j].y0, clipedlines.data[j].x1, clipedlines.data[j].y1);
		}
	}
}





