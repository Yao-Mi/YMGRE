#ifndef _YMGRE_SCENCEMANAGER_H
#define _YMGRE_SCENCEMANAGER_H
#include"./YMGRE_Camera.h"
#include"./YMGRE_Light.h"
#include"./YMGRE_List.h"

typedef struct
{
	gre_list LightList;//灯光
	gre_list CamList;//相机
	gre_list ObjList;//物体
	gre_list MaterialList;//材质
	gre_list TerrainList;//地形

	GRE_Terrain curTerrain;
	gre_list RenderList;//渲染列表
	uint8 needUpdate;//场景发生改变，需要更新
	char* resourceRoot;//资源根目录，由场景拥有
	gre_fvector4d cameraPos;
	gre_fvector4d cameraTarget;
	float32 cameraTheta;
	uint8 initialized;
}gre_scence;
typedef gre_scence* GRE_Scence;

typedef struct
{
	void* userData;
	int (*step)(void* userData, int fps);
	int (*readKey)(void* userData, char* key);
	int (*readPointer)(void* userData, GRE_Camera4d camera, int* x, int* y);
	void (*present)(void* userData, GRE_Camera4d camera);
} gre_scene_host;
typedef const gre_scene_host* GRE_SceneHost;


//材质管理
GRE_Material YMGRE_Material_Find(GRE_List MaterialList, char* materialName);//查找材质
void YMGRE_Scence_AddMeterial(GRE_Scence thisSc, GRE_Material thiso);//添加材质
//物体管理
GRE_Object4d YMGRE_Object_Clone(GRE_Object4d thiso);//克隆物体
GRE_Object4d YMGRE_Object_Find(GRE_List ObjList, char* objName);//查找物体
void YMGRE_Scence_AddObject(GRE_Scence thisSc, GRE_Object4d thiso);//添加物体
void YMGRE_SetObject_Pos(GRE_Object4d thiso, float32 x, float32 y, float32 z);//设置物体位置

//管理相机
void YMGRE_Scence_AddCamera(GRE_Scence thisSc, GRE_Camera4d thiso);//添加相机

//光源管理
void YMGRE_Scence_AddLight(GRE_Scence thisSc, GRE_Light4d thiso);//添加光源
void YMGRE_SetLight_Pos(GRE_List LightList, int16 Id, gre_fvector4d newPos);//设置灯光位置

//管线渲染
int YMGRE_Scene_Init(GRE_Scence pthisc, const char* resourceRoot);
void YMGRE_Scene_Destroy(GRE_Scence pthisc);
void YMGRE_Scene_Rendering(GRE_Scence pthisc, GRE_SceneHost host);

#endif // !_YMGRE_SCENCEMANAGER_H

