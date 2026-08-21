#include"../CONFIG/YMGRE_Mem.h"
#include"./YMGRE_Creat.h"
#include"../DEBUG/YMGRE_Debug.h"
#include "../CONFIG/YMGRE_PubDefine.h"

/**
  ***************************************************************************************************************************
  *	@FileName:    YMGRE_Creat.c
  *	@Author:      yaomimaoren
  *	@Date:        2021-10-31
  *	@Description: 对象创建,该模块主管对象创建、封装，以及存放内存区
  *	@Version:     1.0
  *
  ***************************************************************************************************************************
  *
  * 备注信息：
  * 1.涵盖了图像创建、裁剪、拷贝、数据转图像格式
  *
  *
  * 存在问题备注：
  *
  ***************************************************************************************************************************
  * <author> <time> <version > <desc>
  * yaomi 21/10/31 1.0 build this moudle
  *
  *   __  __ ___    ____   __  ___ ____     ______ ______ ______ __  __
  *   \ \/ //   |  / __ \ /  |/  //  _/    /_  __// ____// ____// / / /
  *    \  // /| | / / / // /|_/ / / /       / /  / __/  / /    / /_/ /
  *    / // ___ |/ /_/ // /  / /_/ /       / /  / /___ / /___ / __  /
  *   /_//_/  |_|\____//_/  /_//___/      /_/  /_____/ \____//_/ /_/
  *
  * Copyright (C), 2021-2031, YAOMI Tech. Co., Ltd.
  ***************************************************************************************************************************/


  /***********************************************************   向量创建  *****************************************************************************/

/**
  * @brief float32-2d向量创建，内存申请
  */
GRE_Fvector2d YMGRE_Creat_VectorF2d()
{
	GRE_Fvector2d myvec;

	myvec = (GRE_Fvector2d)GRE_malloc1(sizeof(gre_fvector2d));
	//申请失败
	gre_log_explain(myvec == NULL, GRE_LOG_Mem1, "向量头内存申请失败");
	return myvec;
}

GRE_Fvector3d YMGRE_Creat_VectorF3d()
{
	GRE_Fvector3d myvec;

	myvec = (GRE_Fvector3d)GRE_malloc1(sizeof(gre_fvector3d));
	//申请失败
	gre_log_explain(myvec == NULL, GRE_LOG_Mem1, "向量头内存申请失败");
	return myvec;
}

GRE_Fvector4d YMGRE_Creat_VectorF4d()
{
	GRE_Fvector4d myvec;

	myvec = (GRE_Fvector4d)GRE_malloc1(sizeof(gre_fvector4d));
	//申请失败
	gre_log_explain(myvec == NULL, GRE_LOG_Mem1, "向量头内存申请失败");
	return myvec;
}

/***********************************************************   矩阵创建  *****************************************************************************/

/**
  * @brief 创建float类型矩阵
  */
GRE_FMat4x4 YMGRE_Creat_FMAT4x4()
{
	GRE_FMat4x4 mymat;

	mymat = (GRE_FMat4x4)GRE_malloc0(sizeof(gre_fmat4x4));
	//申请失败
	gre_log_explain(mymat == NULL, GRE_LOG_Mem0, "矩阵头内存申请失败");

	return mymat;
}

/***********************************************************   灯光创建  *****************************************************************************/
GRE_Light4d YMGRE_Creat_Light(int16 id, GRE_LightType type, GRErgb24 color, float32 strength)
{
	GRE_Light4d mylight= GRE_malloc0(sizeof(gre_light4d));
	mylight->ID = id;
	mylight->ishide = 0;//关闭隐藏
	mylight->type = type;
	mylight->proper.strength = strength;//光强
	mylight->proper.lightcolor = color;//颜色
	mylight->proper.shadowK = strength * 0.1f;//背光光强
	//默认属性
	mylight->proper.kc0 = 1.0f; //确保衰减系数:  1 /（c0+c1*d +c2*d^2）<= 1
	mylight->proper.kc1 = 0.01f;
	mylight->proper.kc2 = 0.0f;

	mylight->proper.spot.direct.x = 0;//平行光的方向向量
	mylight->proper.spot.direct.y = -1.0f; //默认朝下
	mylight->proper.spot.direct.z = 0;
	mylight->proper.spot.direct.w = 0;
	mylight->proper.spot.cs_inner_angle = YMGRE_Cos(10.0f * YMGRE_Deg2Rad);//内锥角10°
	mylight->proper.spot.cs_outer_angle = YMGRE_Cos(30.0f * YMGRE_Deg2Rad);//外锥角30°
	float32 cs_sub = mylight->proper.spot.cs_inner_angle - mylight->proper.spot.cs_outer_angle;
	mylight->proper.spot.cs_div_ = 1.0f / (cs_sub * cs_sub);//差值的倒数

	mylight->pos.x = 0; //位置
	mylight->pos.y = 0;
	mylight->pos.z = 0;
	mylight->pos.w = 1;
	return mylight;
}

/***********************************************************   相机创建  *****************************************************************************/
GRE_Camera4d YMGRE_Creat_Camera(int16 id, uint16 imgW, uint16 imgH, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy)
{
	GRE_Camera4d mycam;
	mycam = (GRE_Camera4d)GRE_malloc0(sizeof(gre_camera4d));
	//申请失败
	gre_log_explain(mycam == NULL, GRE_LOG_Mem0, "相机头内存申请失败");

	mycam->ID = id;
	mycam->isMoved = 1;
	mycam->wireFrame = 0;//默认关闭线框模式

	gre_log_explain((alpha_Lx <= 0.0f) || (alpha_Rx <= 0.0f) || (beta_Uy <= 0.0f) || (beta_Dy <= 0.0f), GRE_LOG_Mem0, "偏角必须大于0");
	//视平面确定
	float32 pdis = 100;
	mycam->perspectPlane.Dis = pdis;
	
	//    /
	//   / Uy      \Lx|Ly/
	//  * ---       \ | /
	//   \ Dy        \|/
	//    \           *
	//
	mycam->perspectPlane.pR = (pdis * YMGRE_Tan(alpha_Rx * YMGRE_Deg2Rad));
	mycam->perspectPlane.pL = -(pdis * YMGRE_Tan(alpha_Lx * YMGRE_Deg2Rad));
	mycam->perspectPlane.pU = (pdis * YMGRE_Tan(beta_Uy * YMGRE_Deg2Rad));
	mycam->perspectPlane.pD = -(pdis * YMGRE_Tan(beta_Dy * YMGRE_Deg2Rad));

	mycam->perspectPlane.kl = mycam->perspectPlane.pL / pdis;
	mycam->perspectPlane.kr = mycam->perspectPlane.pR / pdis;
	mycam->perspectPlane.ku = mycam->perspectPlane.pU / pdis;
	mycam->perspectPlane.kd = mycam->perspectPlane.pD / pdis;

	//初始化图片参数
	mycam->img.width = imgW;
	mycam->img.height = imgH;
	mycam->img.data = (GRE_FrameBuffer)GRE_ImageBuff_Malloc(imgW * imgH * sizeof(GRE_FramePixel));
	mycam->img.zbuff = (float32*)GRE_ImageBuff_Malloc(imgW * imgH * sizeof(float32));
	//申请失败
	gre_log_explain(mycam->img.data == NULL, GRE_LOG_Mem1, "相机照片内存申请失败");
	return mycam;
}

/***********************************************************   物体创建  *****************************************************************************/
GRE_Object4d YMGRE_Creat_Object(int pointNum,int polygonNum,char* name,char* materiaName)
{
	GRE_Object4d myobj = GRE_malloc0(sizeof(gre_object4d));
	myobj->pointNum = pointNum;
	myobj->pointList = GRE_malloc1(pointNum * sizeof(gre_vertex4d));
	myobj->pointList_ = GRE_malloc1(pointNum * sizeof(gre_vertex4d));
	myobj->polygonNum = polygonNum;
	myobj->polygonList = GRE_malloc1(polygonNum * sizeof(gre_polygon4d));
	myobj->nextObject = NULL;//默认只有一个submesh
	//物体名称设置
	int namelen = (name == NULL) ? 1 : strlen(name) + 1;
	myobj->objNameLen = namelen;
	myobj->objName = GRE_malloc1(namelen * sizeof(char));
	GRE_memcpy(myobj->objName, name, namelen - 1);//拷贝名字
	myobj->objName[namelen - 1] = '\0';//单独拷贝结束符，兼容name=NULL的情况
	//材质名称设置
    namelen = (materiaName == NULL)? 1: strlen(materiaName) + 1;
	myobj->materiaName = GRE_malloc1(namelen*sizeof(char));
	GRE_memcpy(myobj->materiaName, materiaName, namelen - 1);//拷贝名字
	myobj->materiaName[namelen - 1] = '\0';//单独拷贝结束符，兼容name=NULL的情况
	//设置尺寸、朝向和坐标
	myobj->WorldCoordinate = (gre_fvector4d){ .w = 1 }; //默认为[0, 0, 0, 1]
	myobj->direct = (gre_fvector4d){ .z = 1 };//默认为[0,0,1,0]
	myobj->scale =1.0f; //尺寸
	
	myobj->isVisible = 1;//可见
	myobj->isDelete = 0;//未被剔除
	myobj->cloneTimes = 0;//被克隆次数为0
	return myobj;
}

/***********************************************************   材质创建  *****************************************************************************/
GRE_Material YMGRE_Creat_Material(char* name)
{
	GRE_Material mymaterial = GRE_malloc0(sizeof(gre_material));
	mymaterial->nameLen = (name == NULL) ? 1 : strlen(name) + 1;
	mymaterial->name = GRE_malloc1(mymaterial->nameLen * sizeof(char));
	//名字拷贝
	GRE_memcpy(mymaterial->name, name, mymaterial->nameLen - 1);
	mymaterial->name[mymaterial->nameLen - 1] = '\0';//单独拷贝结束符，兼容name=NULL的情况
	//有效
	mymaterial->valid = 1;
	mymaterial->pixel = NULL;
	mymaterial->width = 0;
	mymaterial->height = 0;
	//默认为白色
	GRErgb24 comcolor = (GRErgb24){ 255,255,255 };
	mymaterial->ambient = comcolor;//环境色
	mymaterial->diffuse = comcolor;//漫反射颜色
	mymaterial->specular = comcolor;//镜面颜色
	return mymaterial;
}

/***********************************************************   地形创建  *****************************************************************************/
GRE_Terrain YMGRE_Creat_Terrain(uint16 x_width, uint16 z_width,float32 blockSize, char* name)
{
	GRE_Terrain myterrain = GRE_malloc0(sizeof(gre_terrain));
	//地图大小
	myterrain->x_width = x_width;
	myterrain->z_width = z_width;
	myterrain->blockSize = blockSize;
	myterrain->mapHeight = GRE_malloc1(x_width * z_width * sizeof(myterrain->mapHeight[0]));
	//初始化为无障碍物
	myterrain->obstaclesNum = 0;
	myterrain->obstaclesList = NULL;
	//地形信息
	int namelen = (name == NULL) ? 1 : strlen(name) + 1;
	myterrain->nameLen = namelen;
	myterrain->name = GRE_malloc1(namelen * sizeof(char));
	GRE_memcpy(myterrain->name, name, namelen - 1);//拷贝名字
	myterrain->name[namelen - 1] = '\0';//单独拷贝结束符，兼容name=NULL的情况
	myterrain->mesh = NULL;
	return myterrain;
}

