#include"../CONFIG/YMGRE_Mem.h"
#include"./YMGRE_Creat.h"
#include"../DEBUG/YMGRE_Debug.h"
#include "../CONFIG/YMGRE_PubDefine.h"
#include "../CORE/YMGRE_MathBase.h"

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
	mylight->proper.spot.cs_div_ = 1.0f / cs_sub;//cos内外锥角差值的倒数

	mylight->pos.x = 0; //位置
	mylight->pos.y = 0;
	mylight->pos.z = 0;
	mylight->pos.w = 1;
	return mylight;
}

/***********************************************************   相机创建  *****************************************************************************/
static GRE_Camera4d creatCameraHeader(int16 id, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy)
{
	GRE_Camera4d mycam;
	mycam = (GRE_Camera4d)GRE_malloc0(sizeof(gre_camera4d));
	//申请失败
	gre_log_explain(mycam == NULL, GRE_LOG_Mem0, "相机头内存申请失败");

#if YMGRE_ENABLE_TRANSPARENCY
 mycam->opacityPass=0;
#endif
#if YMGRE_ENABLE_PBR
 mycam->pbrEnabled=0; mycam->pbrNormalEnabled=1;
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 mycam->linearColorEnabled=0; mycam->exposure=1; mycam->linearColor=NULL;
#endif
	mycam->ID = id;
	mycam->isMoved = 1;
	mycam->wireFrame = GRE_Render_Solid;//默认使用实体模式

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

	mycam->img = (gre_render_target){ 0 };
	mycam->target = NULL;
	mycam->ownsImageBuffers = 0;
	mycam->workspace = NULL;
	return mycam;
}

GRE_Camera4d YMGRE_Creat_Camera(int16 id, uint16 imgW, uint16 imgH, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy)
{
	GRE_Camera4d mycam = creatCameraHeader(id, alpha_Lx, alpha_Rx, beta_Uy, beta_Dy);
	mycam->img.width = imgW;
	mycam->img.height = imgH;
	mycam->img.data = (GRE_FrameBuffer)GRE_RenderColorBuff_Malloc((size_t)imgW * imgH * sizeof(GRE_FramePixel));
	mycam->img.zbuff = (float32*)GRE_RenderTargetBuff_Malloc((size_t)imgW * imgH * sizeof(float32));
	mycam->target = &mycam->img;
	mycam->ownsImageBuffers = 1;
	gre_log_explain((mycam->img.data == NULL) || (mycam->img.zbuff == NULL), GRE_LOG_Mem1, "相机照片内存申请失败");
	return mycam;
}

GRE_Camera4d YMGRE_Creat_CameraFromTarget(int16 id, GRE_RenderTarget target, float32 alpha_Lx, float32 alpha_Rx, float32 beta_Uy, float32 beta_Dy)
{
	gre_log_explain(target == NULL, GRE_LOG_PtrI, "外部渲染目标不存在");
	GRE_Camera4d mycam = creatCameraHeader(id, alpha_Lx, alpha_Rx, beta_Uy, beta_Dy);
	mycam->img.width = target->width;//保留原相机尺寸读取接口
	mycam->img.height = target->height;
	//兼容旧的img读取接口，但缓存所有权仍属于调用者提供的RenderTarget
	mycam->img.data = target->data;
	mycam->img.zbuff = target->zbuff;
	mycam->target = target;
	mycam->ownsImageBuffers = 0;
	return mycam;
}

/***********************************************************   物体创建  *****************************************************************************/
GRE_Object4d YMGRE_Creat_Object(int pointNum,int polygonNum,char* name,char* materiaName)
{
	gre_log_explain(pointNum < 0 || (uint32)pointNum > YMGRE_MAX_VERTICES || polygonNum < 0 ||
		(size_t)pointNum > SIZE_MAX / sizeof(gre_vertex4d) ||
		(size_t)polygonNum > SIZE_MAX / sizeof(gre_polygon4d), GRE_LOG_ParamI,
		"模型数量超过当前索引或内存范围");
	GRE_Object4d myobj = GRE_malloc0(sizeof(gre_object4d));
	myobj->pointNum = pointNum;
	myobj->pointList = GRE_GeometryBuff_Malloc(pointNum * sizeof(gre_vertex4d));
	myobj->pointList_ = GRE_malloc1(pointNum * sizeof(gre_vertex4d));
	/* Untextured generators (including cube/box) only assign positions.
	   Give UV0 a defined default before advanced attribute generation/sampling. */
	for (int i = 0; i < pointNum; i++) {
		myobj->pointList[i].u = myobj->pointList[i].v = 0;
		myobj->pointList_[i].u = myobj->pointList_[i].v = 0;
	}
	myobj->pointList_wN = NULL;
	myobj->pointList_wN_ = NULL;
	myobj->mirrorKs = 0.0f;
	myobj->lightmap = NULL;
	myobj->importedUvCount = 0;
	myobj->importedUvs = NULL;
	myobj->importedNormals = NULL;
	myobj->polygonNum = polygonNum;
	myobj->polygonList = GRE_malloc1(polygonNum * sizeof(gre_polygon4d));
	myobj->topologyStorage = NULL;
	myobj->topologyIndexCount = 0;
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
	myobj->wireFrame = 0;//默认不绘制模型线框
	myobj->renderMode = GRE_RenderMode_Face;//保持兼容的逐面渲染
	myobj->cloneTimes = 0;//被克隆次数为0
	return myobj;
}

int YMGRE_Object_EnableVertexAttributes(GRE_Object4d object)
{
	if (object == NULL || object->pointNum <= 0)
		return 0;
	if (object->pointList_wN != NULL && object->pointList_wN_ != NULL)
		return 1;
	GRE_Vertex4d_wN base = GRE_malloc1(object->pointNum * sizeof(gre_vertex4d_wN));
	GRE_Vertex4d_wN transformed = GRE_malloc1(object->pointNum * sizeof(gre_vertex4d_wN));
	if (base == NULL || transformed == NULL)
	{
		GRE_free1(base);
		GRE_free1(transformed);
		return 0;
	}
	for (int i = 0; i < object->pointNum; i++)
	{
		base[i].base = object->pointList[i];
		base[i].normal = (gre_fvector4d){ 0, 0, 1, 0 };
		base[i].tangent = (gre_fvector4d){ 1, 0, 0, 0 };
		base[i].tangentW = 1.0f;
		base[i].lightmapU = base[i].lightmapV = 0;
		base[i].color = (GRErgb24){ 255, 255, 255 };
		base[i].vertexLighting = (GRErgb24){ 255, 255, 255 };
		base[i].vertexSpecular = (GRErgb24){ 0, 0, 0 };
		transformed[i] = base[i];
	}
	object->pointList_wN = base;
	object->pointList_wN_ = transformed;
	return 1;
}

/* Preparation only: consolidates owned polygon index allocations before rendering. */
int YMGRE_Object_CompactTopology(GRE_Object4d object)
{
 if(object==NULL || object->polygonNum<=0 || object->topologyStorage) return 0;
 size_t count=0;
 for(int pi=0;pi<object->polygonNum;pi++) {
  GRE_Polygon4d p=&object->polygonList[pi];
  if(p->num && !p->index) return 0;
  if(p->num>(SIZE_MAX/sizeof(GRE_Index))-count) return 0;
  count+=p->num;
 }
 if(count==0 || count>UINT32_MAX) return 0;
 GRE_Index *storage=GRE_RenderBuff_Malloc(count*sizeof(GRE_Index));
 if(!storage) return 0;
 size_t offset=0;
 for(int pi=0;pi<object->polygonNum;pi++) {
  GRE_Polygon4d p=&object->polygonList[pi];
  if(p->num) GRE_memcpy(storage+offset,p->index,p->num*sizeof(GRE_Index));
  GRE_PolyIndex_Free(p->index);
  p->index=p->num?storage+offset:NULL;offset+=p->num;
 }
 object->topologyStorage=storage;object->topologyIndexCount=(uint32)count;
 return 1;
}
#ifndef YMGRE_COMPACT_OWNED_TOPOLOGY
#define YMGRE_COMPACT_OWNED_TOPOLOGY 0
#endif

int YMGRE_Object_GenerateVertexAttributes(GRE_Object4d object)
{
	if (object == NULL || !YMGRE_Object_EnableVertexAttributes(object)) return 0;
	for (int i=0;i<object->pointNum;i++)
	{
		object->pointList_wN[i].base=object->pointList[i];
		object->pointList_wN[i].normal=(gre_fvector4d){0,0,0,0};
		object->pointList_wN[i].tangent=(gre_fvector4d){0,0,0,0};
		object->pointList_wN[i].tangentW=0.0f;
		object->pointList_wN[i].color=(GRErgb24){255,255,255};
		object->pointList_wN[i].vertexLighting=(GRErgb24){255,255,255};
		object->pointList_wN[i].vertexSpecular=(GRErgb24){0,0,0};
	}
	for (int i=0;i<object->polygonNum;i++)
	{
		GRE_Polygon4d polygon=&object->polygonList[i];
		if (polygon->num != 3) continue;
		GRE_Index i0=polygon->index[0],i1=polygon->index[1],i2=polygon->index[2];
		if(i0>=object->pointNum||i1>=object->pointNum||i2>=object->pointNum) continue;
		GRE_Vertex4d p0=&object->pointList[i0],p1=&object->pointList[i1],p2=&object->pointList[i2];
		gre_fvector4d e1={p1->pos.x-p0->pos.x,p1->pos.y-p0->pos.y,p1->pos.z-p0->pos.z,0};
		gre_fvector4d e2={p2->pos.x-p0->pos.x,p2->pos.y-p0->pos.y,p2->pos.z-p0->pos.z,0};
		gre_fvector4d face;
		YMGRE_Fvector4d_CrossToResult(&e1,&e2,&face);
		float32 du1=p1->u-p0->u,dv1=p1->v-p0->v,du2=p2->u-p0->u,dv2=p2->v-p0->v;
		float32 det=du1*dv2-du2*dv1;
		gre_fvector4d tangent={0};
		if(YMGRE_Fabs(det)>1e-8f)
		{
			float32 inv=1.0f/det;
			tangent.x=(e1.x*dv2-e2.x*dv1)*inv;
			tangent.y=(e1.y*dv2-e2.y*dv1)*inv;
			tangent.z=(e1.z*dv2-e2.z*dv1)*inv;
		}
		GRE_Index ids[3]={i0,i1,i2};
		for(int j=0;j<3;j++)
		{
			YMGRE_Fvector4d_AddTo(&object->pointList_wN[ids[j]].normal,&face);
			YMGRE_Fvector4d_AddTo(&object->pointList_wN[ids[j]].tangent,&tangent);
			if(YMGRE_Fabs(det)>1e-8f) object->pointList_wN[ids[j]].tangentW += det < 0.0f ? -1.0f : 1.0f;
		}
	}
	for(int i=0;i<object->pointNum;i++)
	{
		GRE_Vertex4d_wN vertex=&object->pointList_wN[i];
		if (object->importedNormals) vertex->normal=object->importedNormals[i];
		if(YMGRE_Fvector4d_Len1(&vertex->normal)<1e-8f) vertex->normal=(gre_fvector4d){0,0,1,0};
		else YMGRE_Fvector4d_Normalize(&vertex->normal);
		float32 ndt=YMGRE_Fvector4d_Dot(&vertex->normal,&vertex->tangent);
		vertex->tangent.x-=vertex->normal.x*ndt;vertex->tangent.y-=vertex->normal.y*ndt;vertex->tangent.z-=vertex->normal.z*ndt;
		if(YMGRE_Fvector4d_Len1(&vertex->tangent)<1e-8f)
		{
			gre_fvector4d axis=YMGRE_Fabs(vertex->normal.z)<0.9f?(gre_fvector4d){0,0,1,0}:(gre_fvector4d){0,1,0,0};
			YMGRE_Fvector4d_CrossToResult(&axis,&vertex->normal,&vertex->tangent);
		}
		YMGRE_Fvector4d_Normalize(&vertex->tangent);
		vertex->tangentW=vertex->tangentW<0.0f?-1.0f:1.0f;
		object->pointList_wN_[i]=*vertex;
	}
#if YMGRE_COMPACT_OWNED_TOPOLOGY
	(void)YMGRE_Object_CompactTopology(object);
#endif
	return 1;
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
	mymaterial->advanced = NULL;
	mymaterial->unlit = 0;
	mymaterial->doubleSided = 0;
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

