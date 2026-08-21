#ifndef _YMGRE_OBJ_H
#define _YMGRE_OBJ_H
#include"../CONFIG/YMGRE_PubType.h"

/*-------------------------------------  向量  ---------------------------------------------*/

typedef struct gre_ivector2d_
{
	int16 x;
	int16 y;
}gre_ivector2d;
typedef gre_ivector2d* GRE_Ivector2d;

//浮点向量
typedef struct gre_fvector2d_ //2D
{
	int16 x;
	int16 y;
}gre_fvector2d;
typedef gre_fvector2d* GRE_Fvector2d;

typedef struct gre_fvector3d_//3D
{
	float32 x;
	float32 y;
	float32 z;
}gre_fvector3d;
typedef gre_fvector3d* GRE_Fvector3d;

typedef struct gre_fvector4d_//4D
{
	float32 x;
	float32 y;
	float32 z;
	float32 w;
}gre_fvector4d;
typedef gre_fvector4d* GRE_Fvector4d;

/*-------------------------------------  矩阵  ---------------------------------------------*/
//矩阵
typedef struct gre_fmat4x4_//4×4
{
	float32 val[4][4];
}gre_fmat4x4;
typedef gre_fmat4x4* GRE_FMat4x4;


/*-------------------------------------  物体结构单元  ---------------------------------------------*/

//顶点
typedef struct gre_vertex4d_
{
	gre_fvector4d pos;//位置
	float32 u, v;//纹理坐标
}gre_vertex4d;
typedef gre_vertex4d* GRE_Vertex4d;

//多边形，基于顶点索引
typedef struct gre_polygon4d_
{
	//GRE_Vertex4d vertex_set;//顶点集
	gre_fvector4d pN;//平面法矢量，朝向物体外部
	int ishide;//被隐藏
	GRErgb24 planeColor;//材质颜色
	GRErgb24 planeColor_;//颜色
	int num;
	//索引
	uint16* index;

}gre_polygon4d;
typedef gre_polygon4d* GRE_Polygon4d;

typedef enum
{
	GRE_Bounding_Box_AABB,//AABB包围盒剔除
	GRE_Bounding_Sphere_R,//使用球体进行包围
}GRE_BoundingType;

//物体：以多边形存储
typedef struct gre_object4d_
{
	int8 isVisible;//可见
	int8 isDelete;//被剔除了
	int8 boundType;
	uint8 objNameLen;
	uint32 cloneTimes;//被克隆的次数
	//包围盒
	gre_fvector4d BoundingBoxMax;
	gre_fvector4d BoundingBoxMin;
	//包围球
	float32 BoundingSphereR;//最大半径

	//位置
	gre_fvector4d WorldCoordinate;//在世界坐标中的位置，默认为 [0,0,0,1]
	gre_fvector4d direct;//在世界坐标中的朝向，方向向量，默认为[0,0,1,0]
	float32 scale; //大小缩放

	//构成单元
	int pointNum;//顶点数量
	GRE_Vertex4d pointList;//原始列表
	GRE_Vertex4d pointList_;//用于处理的临时列表

	int polygonNum;//多边形数量
	GRE_Polygon4d polygonList;//多边形列表

	float32 mirrorKs;//镜面反射率

	char* objName;//物体名称
	char* materiaName; // 模型材质的名称--for Ogre Mesh
	// 这个变量是为了处理一个导入的mesh有多个submesh的情况来考虑的, 如果有多个submesh的话
	// nextObject != NULL 否则该指针会指向下一个submesh, 直至nextObject == NULL
	struct gre_object4d_* nextObject;
}gre_object4d;
typedef gre_object4d* GRE_Object4d;

////三角形，基于顶点索引
//typedef struct gre_trigon4d_
//{
//	gre_fvector4d pn;//平面法矢量，朝向物体外部
//	int ishide;//被隐藏
//	uint32 index[3];//索引
//}gre_trigon4d;
//typedef gre_trigon4d* GRE_Trigon4d;
//
////确定物体：以三角网格存储,这个可以非常节省空间
//typedef struct gre_object4d_const_
//{
//	int8 isDelete;//被剔除了
//	//包围盒
//	gre_fvector4d BoundingBoxMax;
//	gre_fvector4d BoundingBoxMin;
//
//	//位置
//	gre_fvector4d WorldCoordinate;//在世界中的坐标
//	gre_fvector3d scale; //大小缩放
//	gre_fvector4d direct;//朝向向量，方向向量
//
//	//构成单元
//	int pointNum;//顶点数量
//	GRE_Vertex4d pointList;//顶点列表
//
//	int trigonNum;//三角形数量
//	GRE_Trigon4d trigonList;//三角形列表
//
//}gre_object4d_const;
//typedef gre_object4d_const* GRE_Object4d_Const;

/*-------------------------------------  光线  ---------------------------------------------*/
typedef enum
{
	GRE_GlobalLight,//全局光照
	GRE_SpotLight,//聚光灯
	GRE_PointLight,//点光源
}GRE_LightType;
typedef struct gre_light4d_
{
	int16 ID;
	int8 ishide;//被隐藏 | 关闭
	GRE_LightType type;//灯光类型

	struct
	{
		gre_fvector4d pos_; //变换后的坐标

		float32 strength;//光强，默认1.0f
		float32	shadowK;//灯光背面强度, 默认为0.1f倍光强，用于模拟阴影衰减
		float32 kc0, kc1, kc2;//衰减因子
		GRErgb24 lightcolor;//颜色
		//聚光灯参数
		struct
		{
			gre_fvector4d direct; //光源朝向
			float32 cs_inner_angle; //内锥角 cos值
			float32 cs_outer_angle; //外锥角 cos值
			float32 cs_div_; // 内外锥角cos差值平方的倒数
			float32 pf;//指数因子
		}spot;
	}proper;

	gre_fvector4d pos;//灯光位置

}gre_light4d;
typedef gre_light4d* GRE_Light4d;

/*-------------------------------------  相机  ---------------------------------------------*/
//欧拉相机与UVN的区别在于欧拉使用角度来描述，UVN使用视点作为描述
typedef enum
{
	GRE_UVNCamera,//UVN相机
	//GRE_PointLight,//欧拉相机
}GRE_CameraType;
typedef struct gre_camera4d_
{
	int16 ID;
	uint8 isMoved;//被移动标识
	uint8 wireFrame;//只显示线框模型标识
	gre_fvector4d pos;//位置
	gre_fvector4d traget;//注视目标，用于计算uvn向量
	//运动信息
	struct 
	{
		gre_fvector4d cu, cv, cn;//uvn向量
		float32 theta;//绕N轴的自旋角
		gre_fmat4x4 TMat;//世界空间->相机空间  的变换矩阵
	}move;

	//相机固有物理参数（镜头参数）
	struct 
	{
		float32 Dis;//视平面的距离
		float32 pL, pR, pU, pD;//采集框
		float32 kl, kr, ku, kd;//kl = pL / Dis
	}perspectPlane;

	//视景体，可视范围
	struct 
	{
		float32 Znear;//近平面
		float32 Zfar; //远平面
	}frustum;
	//硬件参数，采集图形的大小
	struct 
	{
		uint16 width, height;
		GRE_FrameBuffer data;
		float32* zbuff;
	}img;
}gre_camera4d;
typedef gre_camera4d* GRE_Camera4d;

/*-------------------------------------  材质  ---------------------------------------------*/
typedef struct gre_material_
{
	char* name;
	uint8 nameLen;
	uint8 valid;//是否启用
	GRErgb24 ambient;	// 环境色
	GRErgb24 diffuse;	// 漫反射颜色
	GRErgb24 specular;	// 镜面颜色

	uint16 width, height;
	GRErgb24* pixel;
}gre_material;
typedef gre_material* GRE_Material;

/*-------------------------------------  地形  ---------------------------------------------*/
typedef struct gre_terrain_
{
	char* name;
	uint8 nameLen;
	uint16 x_width, z_width;
	float32 blockSize;
	uint16* mapHeight;//地形高度
	GRE_Object4d mesh;
	// 存储当前加载地图的障碍物
	uint32 obstaclesNum;
	GRE_Object4d* obstaclesList;
}gre_terrain;
typedef gre_terrain* GRE_Terrain;

#endif

