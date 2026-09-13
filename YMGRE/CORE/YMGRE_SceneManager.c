#include"./YMGRE_ScenceManager.h"
#include"../IOFILE/YMCS_File_IO.h"
#include "./YMGRE_Rendering_Pipeline.h"
#include "./YMGRE_RenderContext.h"
#include "./YMGRE_CullingAndClipping.h"
#include "./YMGRE_Rasterization.h"
#include "../CONFIG/YMGRE_Mem.h"
#include <stdio.h>
#include <string.h>

static char* scene_make_path(const char* root, const char* suffix)
{
	if (root == NULL || suffix == NULL)
		return NULL;
	size_t rootLen = strlen(root);
	size_t suffixLen = strlen(suffix);
	uint8 slash = (rootLen > 0 && root[rootLen - 1] != '/');
	char* path = GRE_malloc1(rootLen + suffixLen + slash + 1);
	if (path == NULL)
		return NULL;
	memcpy(path, root, rootLen);
	if (slash)
		path[rootLen++] = '/';
	memcpy(path + rootLen, suffix, suffixLen + 1);
	return path;
}

static int scene_file_exists(const char* path)
{
	FILE* file = fopen(path, "rb");
	if (file == NULL)
		return 0;
	fclose(file);
	return 1;
}

static char* unum2str(uint32 unum)
{
	const char index[] = "0123456789";//索引表
	static char divByte[4*4] = {0};//255\0
	static char resStr[4*4] = { 0 };
	
	int8 i = 0;
	//转换部分，注意转换后是逆序的
	do
	{
		divByte[i++] = index[unum % 10];
		unum /= 10;//unum去掉最后一位

	} while (unum);//直至unum为0退出循环
	//倒序
	int8 s = 0, smax = i;
	for (; s < smax; s++)
	{
		i--;
		resStr[s] = divByte[i];
	}
	resStr[smax] = '\0';
	return resStr;
}


//////////////////////////////光源管理/////////////////////////

//设置灯光位置
void YMGRE_SetLight_Pos(GRE_List LightList, int16 Id, gre_fvector4d newPos)
{
	for (GRE_ListNode curLightlist = LightList->listhead; curLightlist != NULL; curLightlist = curLightlist->next)//遍历物体
	{
		GRE_Light4d thislight = curLightlist->data;
		if (thislight->ID == Id)
		{
			thislight->pos = newPos;
		}
	}
}

void YMGRE_Scence_AddLight(GRE_Scence thisSc, GRE_Light4d thiso)
{
	YMGRE_List_Append(&thisSc->LightList, sizeof(gre_light4d), thiso);
	thisSc->needUpdate = 1;
}

//管理相机
void YMGRE_Scence_AddCamera(GRE_Scence thisSc, GRE_Camera4d thiso)
{
	YMGRE_List_Append(&thisSc->CamList, sizeof(gre_camera4d), thiso);
	thisSc->needUpdate = 1;
}

//////////////////////////////物体管理/////////////////////////

//物体克隆
GRE_Object4d YMGRE_Object_Clone(GRE_Object4d thiso)
{
	////单个物体克隆次数不能超过250
	//gre_log_explain(thiso->cloneTimes >= 250, GRE_LOG_ParamI, "克隆次数超过250次，无法继续克隆该物体");

	//设置克隆体名字
	char objName[100];
	gre_log_explain(strlen(thiso->objName) + sizeof("_clone") + strlen(unum2str(thiso->cloneTimes)) > sizeof(objName), GRE_LOG_ParamI, "克隆物体名称过长");
	strcpy(objName, thiso->objName);
	strcat(objName, "_clone");
	strcat(objName, unum2str(thiso->cloneTimes++));
	//从场景中
	GRE_Object4d head = YMGRE_Creat_Object(thiso->pointNum, thiso->polygonNum, objName, thiso->materiaName);

	GRE_Object4d curobj = thiso;
	GRE_Object4d cptobj = head;

	while (1)
	{
		//物体初始化
		{
			//包围盒
			cptobj->boundType = curobj->boundType;
			cptobj->BoundingBoxMax = curobj->BoundingBoxMax;
			cptobj->BoundingBoxMin = curobj->BoundingBoxMin;
			cptobj->BoundingSphereR = curobj->BoundingSphereR;
			//位置 朝向 尺度
			cptobj->WorldCoordinate = curobj->WorldCoordinate;
			cptobj->direct = curobj->direct;
			cptobj->scale = curobj->scale;
			//构成单元
			for (int32 i = 0; i < curobj->pointNum; i++)//多边形顶点加载
			{
				cptobj->pointList[i] = curobj->pointList[i];
				//变换后的顶点
				cptobj->pointList_[i] = curobj->pointList_[i];
			}
			for (int i = 0; i < curobj->polygonNum; i++)//多边形索引加载
			{
				int idxnum = curobj->polygonList[i].num;//边索引数
				cptobj->polygonList[i].ishide = 0;//默认为不隐藏
				cptobj->polygonList[i].num = idxnum;
				cptobj->polygonList[i].index = GRE_PolyIndex_Malloc(idxnum * sizeof(uint16));
				//边索引保存
				for (int k = 0; k < idxnum; k++)
				{
					cptobj->polygonList[i].index[k] = curobj->polygonList[i].index[k];
				}
				cptobj->polygonList[i].pN = curobj->polygonList[i].pN;
				//平面法向量PN
				cptobj->polygonList[i].pN = curobj->polygonList[i].pN;
				//颜色
				cptobj->polygonList[i].planeColor = curobj->polygonList[i].planeColor;
			}
			if(curobj->importedUvs) {
				cptobj->importedUvCount=curobj->importedUvCount;
				size_t bytes=(size_t)curobj->pointNum*curobj->importedUvCount*2*sizeof(float32);
				cptobj->importedUvs=GRE_malloc1(bytes);GRE_memcpy(cptobj->importedUvs,curobj->importedUvs,bytes);
			}
			if(curobj->importedNormals) {
				size_t bytes=(size_t)curobj->pointNum*sizeof(gre_fvector4d);
				cptobj->importedNormals=GRE_malloc1(bytes);GRE_memcpy(cptobj->importedNormals,curobj->importedNormals,bytes);
			}
			//镜面反射率
			cptobj->mirrorKs = curobj->mirrorKs;
		}
		//没有submesh
		if (curobj->nextObject == NULL)
			break;
		curobj = curobj->nextObject;
		//添加submesh
		cptobj->nextObject = YMGRE_Creat_Object(curobj->pointNum, curobj->polygonNum, objName, curobj->materiaName);
		cptobj = cptobj->nextObject;
		
	}
	return head;
}

//找到对应的物体
GRE_Object4d YMGRE_Object_Find(GRE_List ObjList, char* objName)
{
	GRE_Object4d which = NULL;
	int nameLen = strlen(objName) + 1;

	for (GRE_ListNode curObjlist = ObjList->listhead; curObjlist != NULL; curObjlist = curObjlist->next)//遍历物体
	{
		GRE_Object4d thismt = curObjlist->data;
		if ((thismt->objNameLen == nameLen) && (YMGRE_Memcmp(thismt->objName, objName, nameLen) == 0))
		{
			which = thismt;
			break;
		}
	}
	return which;
}
//往场景中添加物体
void YMGRE_Scence_AddObject(GRE_Scence thisSc, GRE_Object4d thiso)
{
	//不存在，是新物体
	if (YMGRE_Object_Find(&thisSc->ObjList, thiso->objName) == NULL)
	{
		YMGRE_List_Append(&thisSc->ObjList, sizeof(gre_object4d), thiso);
		thisSc->needUpdate = 1;
	}
	//不重复加载，释放内存
	else
	{
		YMGRE_Free_Object(thiso);
	}
}
//设置物体位置
void YMGRE_SetObject_Pos(GRE_Object4d thiso, float32 x, float32 y, float32 z)
{
	//世界坐标的位置
	thiso->WorldCoordinate.x = x;
	thiso->WorldCoordinate.y = y;
	thiso->WorldCoordinate.z = z;
}
//设置物体的朝向

//设置物体的大小

/////////////////////////////材质管理//////////////////////////

//找到对应的材质
//往场景中添加材质
void YMGRE_Scence_AddMeterial(GRE_Scence thisSc, GRE_Material thiso)
{
	//材质不存在，是新材质
	if (YMGRE_Material_Find(&thisSc->MaterialList, thiso->name) == NULL)
	{
		YMGRE_List_Append(&thisSc->MaterialList, sizeof(gre_material), thiso);
		thisSc->needUpdate = 1;
	}
	//不重复加载，释放内存
	else
	{
		YMGRE_Free_Material(thiso);
	}
}

//////////////////////////////地形管理/////////////////////////

//找到对应的地形
GRE_Terrain YMGRE_Terrain_Find(GRE_List TerrainList, char* terrainName)
{
	GRE_Terrain which = NULL;
	int nameLen = strlen(terrainName) + 1;

	for (GRE_ListNode curMaterialist = TerrainList->listhead; curMaterialist != NULL; curMaterialist = curMaterialist->next)//遍历物体
	{
		GRE_Terrain thismt = curMaterialist->data;
		if ((thismt->nameLen == nameLen) && (YMGRE_Memcmp(thismt->name, terrainName, nameLen) == 0))
		{
			which = thismt;
			break;
		}
	}
	return which;
}

//往场景中添加地形
void YMGRE_Scence_AddTerrain(GRE_Scence thisSc, GRE_Terrain thiso)
{
	//地形不存在，是新地形
	if (YMGRE_Terrain_Find(&thisSc->TerrainList, thiso->name) == NULL)
	{
		YMGRE_List_Append(&thisSc->TerrainList, sizeof(gre_terrain), thiso);
		thisSc->needUpdate = 1;
	}
	//不重复加载，释放内存
	else
	{
		YMGRE_Free_Terrain(thiso);
	}
}




//场景初始化

void gre_terrainInit(GRE_Scence pthisc)
{
	//添加地形
	char* mapPath = scene_make_path(pthisc->resourceRoot, "map/map001.map");
	GRE_Terrain myTerr = YMGRE_Load_SceneTerrainAndMaterial(pthisc, mapPath);
	GRE_free1(mapPath);
	
	YMGRE_Scence_AddTerrain(pthisc, myTerr);//装载到全局地形库

	//设置当前场景的地形
	pthisc->curTerrain = YMGRE_Terrain_Find(&pthisc->TerrainList,"map001");


	//for (int i = 0; i < myTerr->obstaclesNum; i++)
	//{
	//	printf("%d %s\n",i, myTerr->obstaclesList[i]->objName);
	//}
}

void gre_lightInit(GRE_Scence pthisc)
{
	//全局光照 + 点光源
	GRE_Light4d gbl0 = YMGRE_Creat_Light(0, GRE_GlobalLight, (GRErgb24) { .R = 255, .G = 255, .B = 255 }, 1.0f);//全局光照
	GRE_Light4d ptl0 = YMGRE_Creat_Light(1, GRE_PointLight, (GRErgb24) { .R = 255, .G = 255, .B = 255 }, 1.0f);//点光源
	YMGRE_Scence_AddLight(pthisc, gbl0);
	YMGRE_Scence_AddLight(pthisc, ptl0);
	YMGRE_SetLight_Pos(&pthisc->LightList, 1, (gre_fvector4d) { .x = -50, .y = 50, .z = 50, .w = 1 });
}

void gre_objInit(GRE_Scence pthisc)
{
	//添加物体
	char* meshPath = scene_make_path(pthisc->resourceRoot, "obj/Tank1_Body.mesh");
	GRE_Object4d myMesh = YMGRE_LoadOgreMeshAndMaterial(pthisc, meshPath);
	GRE_free1(meshPath);
	YMGRE_Scence_AddObject(pthisc, myMesh);//装载到全局物体库
	
}

void gre_camInit(GRE_Scence pthisc)
{
	//左右开角
	float32 angL = 45;
	float32 angR = 45;
	//上下开角
	float32 angU = 45;
	float32 angD = 45;
	GRE_Camera4d mycam0 = YMGRE_Creat_Camera(0,500, 500, angL, angR, angU, angD);//创建摄像机，根据定义自动创建视平面
	YMGRE_Camera_Frustum_Init(mycam0, 10, 1500);

	GRE_Camera4d mycam1 = YMGRE_Creat_Camera(1, 300, 300, angL, angR, angU, angD);//创建摄像机，根据定义自动创建视平面
	YMGRE_Camera_Frustum_Init(mycam1, 50, 800);
	//添加到场景
	YMGRE_Scence_AddCamera(pthisc, mycam0);
	YMGRE_Scence_AddCamera(pthisc, mycam1);

	////设置为只渲染线框
	//mycam0->wireFrame = 1;
}

////////////////////////////////////////////渲染列表管理///////////////////////////////////////

//渲染列表初始化
void gre_RenderListInit(GRE_Scence pthisc)
{
	//地形装载
	GRE_Terrain curTerr = pthisc->curTerrain;
	if (curTerr != NULL)
	{
		YMGRE_List_Append(&pthisc->RenderList, sizeof(gre_object4d), curTerr->mesh);
		//障碍物装载
		for (int i = 0; i < curTerr->obstaclesNum; i++)
		{
			YMGRE_List_Append(&pthisc->RenderList, sizeof(gre_object4d), curTerr->obstaclesList[i]);
		}
	}

	//物体装载

	//场景需要更新
	pthisc->needUpdate = 1;
}
//渲染列表节点销毁
void RenderListNodefree(void* pnode) 
{
	//由于列表只是用来收集已经存在的mesh，所以不需要释放节点数据的内存
};
//渲染列表销毁
void gre_RenderListClear(GRE_Scence pthisc)
{
	YMGRE_List_Clear(&pthisc->RenderList, RenderListNodefree);
}

//物体操作更新
void gre_objOpera(GRE_Object4d pthis)
{
	//可以通过设置索引绑定操作函数
	//这里直接通过名字查找
	if ((pthis->objNameLen == sizeof("Tank")) && (YMGRE_Memcmp(pthis->objName, "Tank", sizeof("Tank")) == 0))
	{
		
	}
}

//场景更新
void gre_camUpdate(GRE_Scence scene, GRE_Camera4d pthis, GRE_SceneHost host)
{
	GRE_RenderTarget target = YMGRE_Camera_GetRenderTarget(pthis);
	gre_fvector4d* campos = &scene->cameraPos;
	gre_fvector4d* targetpos = &scene->cameraTarget;
	float32* theta = &scene->cameraTheta;

	if (pthis->ID == 0)
	{
		pthis->isMoved = 1;//相机被移动
		char a;
		if (host->readKey != NULL && host->readKey(host->userData, &a))
		{
			if (a == 'w')campos->z += 10;
			if (a == 's')campos->z -= 10;
			if (a == 'd')*theta += 10;
			if (a == 'a')*theta -= 10;

			printf("theta=%.5f  ", *theta);
			printf("z=%.5f  \n", campos->z);
		}

		int x, y;
		if (host->readPointer != NULL &&
			host->readPointer(host->userData, pthis, &x, &y))
		{
			float thx = (x - target->width / 2.0f);
			float thy = (target->height / 2.0f - y);//反向
			campos->x = thx;
			campos->y = thy;
			//YMGRE_SetLight_Pos(&mysc.LightList, 1, (gre_fvector4d) { .x = thx, .y = thy, .z = 20, .w = 1 });
			printf("x=%.5f y=%.5f \n", thx, thy);
		}

		//空间位置初始化,使用默认的v参考向量
		YMGRE_UVNCamera_PositionInit(pthis, campos, targetpos, NULL, *theta);
	}
	else if(pthis->ID == 1)
	{
		//空间位置初始化,使用默认的v参考向量
		YMGRE_UVNCamera_PositionInit(pthis, campos, targetpos, NULL, *theta);
	}
}

//场景渲染管线
int YMGRE_Scene_Init(GRE_Scence pthisc, const char* resourceRoot)
{
	if (pthisc == NULL)
		return 0;
	if (pthisc->initialized)
		return 1;
	const char* root = (resourceRoot != NULL && resourceRoot[0] != '\0') ?
		resourceRoot : "Resource";
	pthisc->resourceRoot = scene_make_path(root, "");
	if (pthisc->resourceRoot == NULL)
		return 0;
	char* mapPath = scene_make_path(pthisc->resourceRoot, "map/map001.map");
	char* meshPath = scene_make_path(pthisc->resourceRoot, "obj/Tank1_Body.mesh");
	int resourcesAvailable = mapPath != NULL && meshPath != NULL &&
		scene_file_exists(mapPath) && scene_file_exists(meshPath);
	GRE_free1(mapPath);
	GRE_free1(meshPath);
	if (!resourcesAvailable)
	{
		fprintf(stderr, "YMGRE: resource root '%s' is missing required scene files\n", root);
		GRE_free1(pthisc->resourceRoot);
		pthisc->resourceRoot = NULL;
		return 0;
	}
	pthisc->cameraPos = (gre_fvector4d) { .x = 10, .y = 25, .z = 30, .w = 1 };
	pthisc->cameraTarget = (gre_fvector4d) { .x = 0, .y = 0, .z = 0, .w = 1 };
	pthisc->cameraTheta = 0;
	gre_terrainInit(pthisc);
	gre_camInit(pthisc);
	gre_objInit(pthisc);
	gre_lightInit(pthisc);
	gre_RenderListInit(pthisc);
	for (GRE_ListNode node = pthisc->RenderList.listhead; node != NULL; node = node->next)
	{
		GRE_Object4d object = node->data;
		do { YMGRE_Object_LocalToWorld(object); }
		while ((object = object->nextObject) != NULL);
	}
	pthisc->initialized = 1;
	return 1;
}

void YMGRE_Scene_Destroy(GRE_Scence pthisc)
{
	if (pthisc == NULL)
		return;
	gre_RenderListClear(pthisc);
	YMGRE_List_Clear(&pthisc->CamList, YMGRE_Free_Camera);
	YMGRE_List_Clear(&pthisc->ObjList, YMGRE_Free_Object);
	YMGRE_List_Clear(&pthisc->LightList, YMGRE_Free_Light);
	YMGRE_List_Clear(&pthisc->MaterialList, YMGRE_Free_Material);
	YMGRE_List_Clear(&pthisc->TerrainList, YMGRE_Free_Terrain);
	GRE_free1(pthisc->resourceRoot);
	pthisc->resourceRoot = NULL;
	pthisc->curTerrain = NULL;
	pthisc->cameraPos = (gre_fvector4d) { 0 };
	pthisc->cameraTarget = (gre_fvector4d) { 0 };
	pthisc->cameraTheta = 0;
	pthisc->initialized = 0;
}

void YMGRE_Scene_Rendering(GRE_Scence pthisc, GRE_SceneHost host)
{
	gre_log_explain((pthisc == NULL) || (host == NULL) ||
		(host->step == NULL) || (host->present == NULL),
		GRE_LOG_PtrI, "场景或应用宿主不存在");
	if (!pthisc->initialized && !YMGRE_Scene_Init(pthisc, NULL))
		return;
	//多个相机顺序渲染时共享一份临时工作区，各自保留独立图像
	GRE_RenderWorkspace renderWorkspace = YMGRE_Creat_RenderWorkspace();

	int running = 1;
	while (running)
	{
		//遍历所有物体,更新对物体的操作
		for (GRE_ListNode curObjlist = pthisc->RenderList.listhead; curObjlist != NULL; curObjlist = curObjlist->next)
		{
			GRE_Object4d thisobj = curObjlist->data;
			gre_objOpera(thisobj);
		}
		//遍历所有摄像头，更新显示
		for (GRE_ListNode curCamlist = pthisc->CamList.listhead; curCamlist != NULL; curCamlist = curCamlist->next)//遍历物体
		{
			GRE_Camera4d thiscam = curCamlist->data;

			//对相机参数进行更新
			gre_camUpdate(pthisc, thiscam, host);

			//相机被移到 或者 场景更新
			if (thiscam->isMoved || pthisc->needUpdate)
			{
				////对相机所在管线进行渲染
				//YMGRE_Camera_PolygonPipline_Rendering(thiscam, &pthisc->LightList, &pthisc->RenderList, &pthisc->MaterialList);
				YMGRE_Camera_TanglePipline_RenderingWithWorkspace(thiscam, &pthisc->LightList, &pthisc->RenderList,
					&pthisc->MaterialList, renderWorkspace);

				//显示相机画面
				host->present(host->userData, thiscam);
				//相机移动更新完毕
			    thiscam->isMoved = 0;
			}
		}
		
		//场景更新完毕
		pthisc->needUpdate = 0;
		running = host->step(host->userData, 100);
	}

	YMGRE_Free_RenderWorkspace(renderWorkspace);
	YMGRE_Scene_Destroy(pthisc);
}




