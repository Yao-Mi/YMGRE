#ifndef _YMGRE_OBJ_H
#define _YMGRE_OBJ_H
#include"../CONFIG/YMGRE_PubType.h"
#include "../CONFIG/YMGRE_Features.h"

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

// Extended vertex format. The legacy vertex is deliberately the first member
// so buffers of this type can be passed to legacy position/UV code.
typedef struct gre_vertex4d_wN_
{
	gre_vertex4d base;
	gre_fvector4d normal;
	gre_fvector4d tangent;
	float32 tangentW;// bitangent handedness (+1/-1), reserved for normal maps
	GRErgb24 color;
	GRErgb24 vertexLighting;// per-vertex ambient/diffuse result
	GRErgb24 vertexSpecular;// per-vertex mirror result, kept separate from albedo
	float32 lightmapU, lightmapV;// independent UV1, interpolated through clipping
}gre_vertex4d_wN;
typedef gre_vertex4d_wN* GRE_Vertex4d_wN;
typedef char YMGRE_Vertex4d_wN_BaseMustBeFirst[
	(offsetof(gre_vertex4d_wN, base) == 0) ? 1 : -1];

typedef enum
{
	GRE_RenderMode_Face = 0,// legacy per-face lighting
	GRE_RenderMode_Vertex,// per-vertex lighting, interpolated color
	GRE_RenderMode_Pixel// per-pixel lighting, interpolated attributes
}GRE_VertexRenderMode;

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
	GRE_Index* index;

}gre_polygon4d;
typedef gre_polygon4d* GRE_Polygon4d;

// Owned by an object. UV1 is stored per triangle corner so shared UV0 vertices
// can cross atlas seams without modifying the source mesh or base texture.
typedef struct gre_lightmap_ {
	uint16 width, height;
	uint32 triangleCount;
	float32* uv1;// triangleCount * 6 floats, polygon order
	GRErgb24* pixels;// baked ambient + direct diffuse illumination, white albedo
	uint8 enabled;
	uint8 materialOnly;// atlas layout only; retain dynamic lighting in the exported material
	GRErgb24* specularPixels;// optional additive highlight captured from referenceView
	gre_fvector4d referenceView;
	float32 specularKs;
	uint8 specularPower;
	GRErgb24 specularColor;
	uint8 colorsBaked;// lighting evaluated with material colors before RGB8 saturation
	GRErgb24 capturedAmbient,capturedDiffuse;
} gre_lightmap;
typedef gre_lightmap* GRE_Lightmap;

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
	uint8 wireFrame;//模型是否叠加三角网格线
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
	GRE_Vertex4d_wN pointList_wN;//可选高级属性列表
	GRE_Vertex4d_wN pointList_wN_;//可选高级属性临时列表
	GRE_VertexRenderMode renderMode;//渲染路径，默认保持旧的逐面模式
	GRE_Lightmap lightmap;// optional, exclusively owned; freed with the object
	uint16 importedUvCount;// optional original float2 UV channels, including UV0
	float32* importedUvs;// pointNum * importedUvCount * 2 floats
	gre_fvector4d* importedNormals;// authored normals; transformed with the mesh

	int polygonNum;//多边形数量
	GRE_Polygon4d polygonList;//多边形列表
	GRE_Index* topologyStorage; // Optional contiguous owned polygon indices.
	uint32 topologyIndexCount;

	float32 mirrorKs;//镜面反射率

	char* objName;//物体名称
	char* materiaName; // 模型材质的名称--for Ogre Mesh
	// 这个变量是为了处理一个导入的mesh有多个submesh的情况来考虑的, 如果有多个submesh的话
	// nextObject != NULL 否则该指针会指向下一个submesh, 直至nextObject == NULL
	struct gre_object4d_* nextObject;
}gre_object4d;
typedef gre_object4d* GRE_Object4d;

//独立3D线段图元，用于编辑器网格、坐标轴等双面辅助几何
typedef struct gre_line3d_
{
	gre_fvector4d start;
	gre_fvector4d end;
	GRErgb24 color;
	uint8 thickness;
}gre_line3d;
typedef gre_line3d* GRE_Line3d;

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
			float32 cs_div_; // 内外锥角cos差值的倒数
			float32 pf;//指数因子
		}spot;
	}proper;

	gre_fvector4d pos;//灯光位置

}gre_light4d;
typedef gre_light4d* GRE_Light4d;

/*-------------------------------------  渲染上下文  ---------------------------------------------*/
typedef struct gre_render_target_
{
	uint16 width, height;//输出图像尺寸
	GRE_FrameBuffer data;//颜色缓存，像素格式由 GRE_FramePixel 决定
	float32* zbuff;//深度缓存，每个颜色像素对应一个 float32
}gre_render_target;
typedef gre_render_target* GRE_RenderTarget;

/* A dispatcher must call each job exactly once and join before returning.
   Scene/material/light data must remain immutable during dispatch. */
typedef void (*GRE_RasterJob)(void* jobContext, uint32 jobIndex);
typedef void (*GRE_RasterDispatch)(void* user, GRE_RasterJob job, void* jobContext, uint32 jobCount);

typedef struct gre_render_workspace_
{
	GRE_Vertex4d pointList;//当前物体变换后的顶点
	GRE_Vertex4d_wN pointList_wN;//可选高级顶点工作缓存
	uint8* polygonHide;//当前物体的逐面剔除结果
	GRErgb24* polygonColor;//当前物体的逐面光照颜色
	gre_fvector4d* lightPos;//当前相机空间中的灯光位置
	uint32 pointMax;//顶点缓存容量
	uint32 pointWNMax;//高级顶点缓存独立容量
	uint32 polygonMax;//多边形状态和颜色缓存容量
	uint32 lightMax;//灯光位置缓存容量
#if YMGRE_ENABLE_TRANSPARENCY
 void* transparentTriangles; size_t transparentCapacity; // reusable packet bytes
#if YMGRE_ENABLE_RASTER_DISPATCH
 GRE_RasterDispatch rasterDispatch; void* rasterDispatchUser;
#endif
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 float32* linearColor; size_t linearCapacity; // RGB float count
#endif
 int materialStatus; // 0 success; -1 insufficient material workspace
	gre_fvector4d* projectedPoints; // Camera-local projected positions.
	uint8* clipCodes; // Camera-local frustum codes and active-vertex mask.
	uint32 projectedMax;
 void* faceOrderScratch; uint32 faceOrderCapacity; // owned reusable opaque sorting buffer
	uint8 ownsMemory;//为1时由工作区扩容并释放内部缓存
}gre_render_workspace;
typedef gre_render_workspace* GRE_RenderWorkspace;

/*-------------------------------------  相机  ---------------------------------------------*/
//欧拉相机与UVN的区别在于欧拉使用角度来描述，UVN使用视点作为描述
typedef enum
{
	GRE_UVNCamera,//UVN相机
	//GRE_PointLight,//欧拉相机
}GRE_CameraType;

//相机渲染模式，保留 wireFrame 字段兼容旧代码
typedef enum
{
	GRE_Render_Solid = 0,//实体模型
	GRE_Render_Wireframe = 1,//只显示线框模型
	GRE_Render_SolidWire = 2,//实体模型叠加网格线
}GRE_RenderMode;

typedef struct gre_camera4d_
{
	int16 ID;
	uint8 isMoved;//被移动标识
	uint8 wireFrame;//渲染模式，取值见 GRE_RenderMode
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
	gre_render_target img;//兼容原有相机图像接口
	GRE_RenderTarget target;//实际输出目标，NULL时使用相机自带的img
	uint8 ownsImageBuffers;//由Creat_Camera置1，Free_Camera只释放相机创建的缓存
	GRE_RenderWorkspace workspace;//渲染工作区，顺序多相机可以共享
 uint8 meshBoundsEnabled; // opt-in conservative compact-mesh visibility
 GRE_FramePixel (*backgroundRow)(uint16 row,uint16 height,void* user);
 void* backgroundUser; // borrowed; NULL callback uses the supplied clear color
#if YMGRE_ENABLE_TRANSPARENCY
 uint8 opacityPass; // internal: 0 all, 1 opaque coverage, 2 fractional coverage
#endif
#if YMGRE_ENABLE_PBR
 uint8 pbrEnabled, pbrNormalEnabled; // opt-in; default disabled / normal enabled
#endif
#if YMGRE_ENABLE_LINEAR_COLOR
 uint8 linearColorEnabled; float32 exposure; // opt-in; default disabled / exposure 1
 float32* linearColor; // borrowed from workspace only during rendering
#endif
}gre_camera4d;
typedef gre_camera4d* GRE_Camera4d;

/*-------------------------------------  材质  ---------------------------------------------*/
typedef struct gre_color_mips_ {
 uint8 count;
 uint16 width[16],height[16];
 GRErgb24* pixels[16]; // level 0 aliases the material's base texture
 GRErgb24* source;
 uint16 sourceWidth,sourceHeight;
} gre_color_mips;
typedef gre_color_mips* GRE_ColorMips;

typedef struct gre_material_advanced_
{
	uint16 normalWidth, normalHeight;
	GRErgb24* normalPixel;//切线空间法线贴图（可选）
#if YMGRE_ENABLE_TRANSPARENCY
 uint16 opacityWidth, opacityHeight;
 uint8* opacityPixel; // owned single-channel 0..255 coverage, UV0
#if YMGRE_ENABLE_OPACITY_MIPMAP
 uint8 opacityMipCount, opacityUseMip;
 uint16 opacityMipWidth[16], opacityMipHeight[16];
 uint8* opacityMipPixels[16]; // level 0 aliases opacityPixel
#endif
#endif
#if YMGRE_ENABLE_PBR
 uint8 pbrEnabled;
 uint16 pbrWidth, pbrHeight;
 GRErgb24* pbrParameters; // linear R=metallic G=roughness B=specular IOR level
 float32 pbrIOR, pbrNormalStrength;
#endif
	uint8 specularPower;// Blinn-Phong exponent; 0 keeps the default value 30
	uint8 rayType;// 0 ordinary, 1 mirror, 2 dielectric glass
	float32 reflectivity, ior, raySpecularStrength;
	GRErgb24 transmissionColor;
	uint8 colorUseMip; // opt-in per material; colorMips owns the generated levels
	GRE_ColorMips colorMips; // optional, owned; higher levels use GRE_ImageBuff_Free
}gre_material_advanced;
typedef gre_material_advanced* GRE_MaterialAdvanced;

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
	uint8 doubleSided; // default 0; reverse back-face normals when enabled
	uint8 unlit;// Ogre lighting off: texture already contains final illumination
	GRE_MaterialAdvanced advanced;//高级材质资源，NULL 时保持紧凑基础材质
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

