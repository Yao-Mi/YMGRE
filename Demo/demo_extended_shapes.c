#include "YMGRE_YMGUI_LCD.h"
#include "YMGRE_BasicMesh_Gener.h"
#include "YMGRE_Rendering_Pipeline.h"
#include "YMGRE_Camera.h"
#include "YMGRE_Creat.h"
#include "YMGRE_Free.h"
#include "YMGRE_List.h"

//模型顶点经过局部旋转后，重新计算每个三角面的法线，保持背面剔除方向一致
static void updateObjectNormals(GRE_Object4d object)
{
	for (uint16 i = 0; i < object->polygonNum; i++)
	{
		GRE_Polygon4d polygon = &object->polygonList[i];
		GRE_Vertex4d point0 = &object->pointList[polygon->index[0]];
		GRE_Vertex4d point1 = &object->pointList[polygon->index[1]];
		GRE_Vertex4d point2 = &object->pointList[polygon->index[2]];
		gre_fvector4d edge0;
		gre_fvector4d edge1;
		YMGRE_Fvector4d_SubToResult(&point1->pos, &point0->pos, &edge0);
		YMGRE_Fvector4d_SubToResult(&point2->pos, &point0->pos, &edge1);
		YMGRE_Fvector4d_CrossToResult(&edge0, &edge1, &polygon->pN);
	}
}

//创建曲面体与规则多面体场景
static void creatExtendedScene(GRE_List objects, GRE_List lights)
{
	GRE_Object4d torus = YMGRE_MeshGener_Torus(60.0f, 22.5f, 20, 10,
		(GRErgb24){ 207, 174, 76 }, "torus", "");
	GRE_Object4d capsule = YMGRE_MeshGener_Capsule(54.0f, 100.0f, 6, 18,
		(GRErgb24){ 70, 166, 126 }, "capsule", "");
	GRE_Object4d tetrahedron = YMGRE_MeshGener_Tetrahedron(54.0f,
		(GRErgb24){ 225, 92, 76 }, "tetrahedron", "");
	GRE_Object4d octahedron = YMGRE_MeshGener_Octahedron(54.0f,
		(GRErgb24){ 218, 130, 63 }, "octahedron", "");
	GRE_Object4d dodecahedron = YMGRE_MeshGener_Dodecahedron(54.0f,
		(GRErgb24){ 146, 105, 190 }, "dodecahedron", "");
	GRE_Object4d icosahedron = YMGRE_MeshGener_Icosahedron(54.0f,
		(GRErgb24){ 74, 128, 210 }, "icosahedron", "");
	//上排放大圆环和横向胶囊体
	torus->WorldCoordinate = (gre_fvector4d){ 250.0f, 120.0f, 500.0f, 1.0f };
	capsule->WorldCoordinate = (gre_fvector4d){ -250.0f, 120.0f, 500.0f, 1.0f };
	//下排四个规则多面体横向排列
	tetrahedron->WorldCoordinate = (gre_fvector4d){ -270.0f, -150.0f, 500.0f, 1.0f };
	octahedron->WorldCoordinate = (gre_fvector4d){ -90.0f, -150.0f, 500.0f, 1.0f };
	dodecahedron->WorldCoordinate = (gre_fvector4d){ 90.0f, -150.0f, 500.0f, 1.0f };
	icosahedron->WorldCoordinate = (gre_fvector4d){ 270.0f, -150.0f, 500.0f, 1.0f };

	//生成器默认胶囊轴线沿 Y，这里旋转到水平 X 轴
	for (uint16 i = 0; i < capsule->pointNum; i++)
	{
		float32 x = capsule->pointList[i].pos.x;
		capsule->pointList[i].pos.x = capsule->pointList[i].pos.y;
		capsule->pointList[i].pos.y = -x;
	}
	updateObjectNormals(capsule);
	torus->wireFrame = 1;
	capsule->wireFrame = 1;
	tetrahedron->wireFrame = 1;
	octahedron->wireFrame = 1;
	dodecahedron->wireFrame = 1;
	icosahedron->wireFrame = 1;

	YMGRE_Object_LocalToWorld(torus);
	YMGRE_Object_LocalToWorld(capsule);
	YMGRE_Object_LocalToWorld(tetrahedron);
	YMGRE_Object_LocalToWorld(octahedron);
	YMGRE_Object_LocalToWorld(dodecahedron);
	YMGRE_Object_LocalToWorld(icosahedron);
	YMGRE_List_Append(objects, sizeof(gre_object4d), torus);
	YMGRE_List_Append(objects, sizeof(gre_object4d), capsule);
	YMGRE_List_Append(objects, sizeof(gre_object4d), tetrahedron);
	YMGRE_List_Append(objects, sizeof(gre_object4d), octahedron);
	YMGRE_List_Append(objects, sizeof(gre_object4d), dodecahedron);
	YMGRE_List_Append(objects, sizeof(gre_object4d), icosahedron);

	GRE_Light4d light = YMGRE_Creat_Light(0, GRE_GlobalLight,
		(GRErgb24){ 255, 255, 255 }, 1.0f);
	YMGRE_List_Append(lights, sizeof(gre_light4d), light);
}

//创建观察扩展基础形状的相机
static GRE_Camera4d creatExtendedCamera(void)
{
	GRE_Camera4d camera = YMGRE_Creat_Camera(0, 800, 800,
		38.0f, 38.0f, 38.0f, 38.0f);
	YMGRE_Camera_Frustum_Init(camera, 1.0f, 1500.0f);
	gre_fvector4d cameraPos = { 0, 300, -300, 1 };
	gre_fvector4d targetPos = { 0, 0, 500, 1 };
	YMGRE_UVNCamera_PositionInit(camera, &cameraPos, &targetPos, NULL, 0.0f);
	camera->wireFrame = GRE_Render_Solid;
	return camera;
}

//将渲染结果交给 YMGUI 显示
static void showCameraImage(GRE_Camera4d camera)
{
	LCD_Init(1200, 900);
	LCD_Fill_RgbRect(0, 0, camera->img.width, camera->img.height, camera->img.data);
	while (LCD_Update(60))
	{
	}
	LCD_Destory();
}

int main(void)
{
	gre_list objects = { 0 };
	gre_list lights = { 0 };
	gre_list materials = { 0 };
	creatExtendedScene(&objects, &lights);
	GRE_Camera4d camera = creatExtendedCamera();

	GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
	YMGRE_Camera_TanglePipline_RenderingWithWorkspace(camera,
		&lights, &objects, &materials, workspace);
	showCameraImage(camera);

	YMGRE_Free_RenderWorkspace(workspace);
	YMGRE_Free_Camera(camera);
	YMGRE_List_Clear(&lights, YMGRE_Free_Light);
	YMGRE_List_Clear(&objects, YMGRE_Free_Object);
	return 0;
}
