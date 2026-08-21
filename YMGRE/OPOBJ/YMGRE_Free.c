#include"./YMGRE_Free.h"
#include"../DEBUG/YMGRE_Debug.h"
#include"../CONFIG/YMGRE_Mem.h"


//向量相关
//浮点向量内存释放
void YMGRE_Free_VectorF4d(GRE_Fvector4d pthis)
{
	if (pthis == NULL)return;
	GRE_free1(pthis);
}

//浮点矩阵内存释放
void YMGRE_Free_FMat4x4(GRE_FMat4x4 pthis)
{
	if (pthis == NULL)return;
	GRE_free0(pthis);
}

//灯光内存释放
void YMGRE_Free_Light(void* data)
{
	GRE_Light4d pthis = data;
	if (pthis == NULL)return;
	GRE_free0(pthis);
}

//相机内存释放
void YMGRE_Free_Camera(void* data)
{
	GRE_Camera4d pthis = data;
	if (pthis == NULL)return;
	GRE_ImageBuff_Free(pthis->img.zbuff);//z - buff
	GRE_ImageBuff_Free(pthis->img.data);//图像
	GRE_free0(pthis);
}

//物体内存释放
void YMGRE_Free_Object(void* data)
{
	GRE_Object4d pthis = data;
	while (pthis != NULL)
	{
		GRE_Object4d nextObject = pthis->nextObject;
		//顶点
		GRE_free1(pthis->pointList);
		GRE_free1(pthis->pointList_);
		//多边形
		for (int i = 0; i < pthis->polygonNum; i++)
		{
			GRE_PolyIndex_Free(pthis->polygonList[i].index);//释放索引内存
		}
		GRE_free1(pthis->polygonList);
		//名称
		GRE_free1(pthis->objName);
		GRE_free1(pthis->materiaName);
		GRE_free0(pthis);
		pthis = nextObject;
	}
}

//材质内存释放
void YMGRE_Free_Material(void* data)
{
	GRE_Material pthis = data;
	if (pthis == NULL)return;
	GRE_free1(pthis->name);//名字
	GRE_ImageBuff_Free(pthis->pixel);//图像
	GRE_free0(pthis);
}

//地形内存释放
void YMGRE_Free_Terrain(void* data)
{
	GRE_Terrain pthis = data;
	if (pthis == NULL)return;
	GRE_free1(pthis->name);//名字
	GRE_free1(pthis->mapHeight);//高度图像
	YMGRE_Free_Object(pthis->mesh);//地形网格
	//障碍物列表
	for (int i = 0; i < pthis->obstaclesNum; i++)
	{
		YMGRE_Free_Object(pthis->obstaclesList[i]);
	}
	GRE_TerrainObstacles_Free(pthis->obstaclesList);
	GRE_free0(pthis);
}



