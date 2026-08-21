#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMCS_File_IO.h"
#include "../DEBUG/YMGRE_Debug.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CORE/YMGRE_OgreMeshInfo.h"
#include "../CORE/YMGRE_ScenceManager.h"
#include <string.h>
#include <ctype.h>

/////////////////////////////////////材质解析 ///////////////////////////////

// 从路径里面截取文件名称, 并添加.material后缀
static inline char* GetMaterialFromSamePath(const char* path)
{
	static char Path[256];
	const char meterial[] = ".material";
	gre_log_explain(strlen(path) >= sizeof(Path), GRE_LOG_FILE, "材质文件路径过长");
	
	int len = 0;
	while (path[len]!='\0')
	{
		Path[len] = path[len];
		len++;
	}
	Path[len] = '\0';
	//尾部替换
	while ((len > 0) && (Path[len] != '.'))
		len--;
	gre_log_explain(Path[len] != '.', GRE_LOG_FILE, "网格文件路径没有后缀");
	gre_log_explain(len + sizeof(meterial) > sizeof(Path), GRE_LOG_FILE, "材质文件路径过长");
	for (uint8 i = 0; i < sizeof(meterial); i++)
	{
		Path[len + i] = meterial[i];
	}
	return Path;
}

//从路径中截取文件路径，并添加图片文件名
static inline char* GetImageFromSamePath(const char* path, const char* imageName)
{
	static char Path[256];
	gre_log_explain(strlen(path) >= sizeof(Path), GRE_LOG_FILE, "贴图文件路径过长");
	int len = 0;
	while (path[len] != '\0')
	{
		Path[len] = path[len];
		len++;
	}
	Path[len] = '\0';
	//尾部替换
	while ((len > 0) && (Path[len] != '\\') && (Path[len] != '/'))
		len--;
	gre_log_explain((Path[len] != '\\') && (Path[len] != '/'), GRE_LOG_FILE, "贴图文件路径没有根目录");
	gre_log_explain(len + strlen(imageName) + 2 > sizeof(Path), GRE_LOG_FILE, "贴图文件路径过长");
	GRE_memcpy(&Path[len + 1], imageName, strlen(imageName) + 1);//名字拷贝过去

	return Path;
}

//从路径中获取mesh名字
static inline char* GetMeshNameFromPath(const char* path)
{
	static char Path[256];
	gre_log_explain(strlen(path) >= sizeof(Path), GRE_LOG_FILE, "网格文件路径过长");
	int len = 0;
	while (path[len] != '\0')
	{
		Path[len] = path[len];
		len++;
	}
	Path[len] = '\0';
	//尾部替换
	while ((len > 0) && (Path[len] != '\\') && (Path[len] != '/'))
	{
		if (Path[len] == '.')//  xxx/xxx.mesh
			Path[len] = '\0';
		len--;
	}
	gre_log_explain((Path[len] != '\\') && (Path[len] != '/'), GRE_LOG_FILE, "网格文件路径没有根目录");
	return &Path[len + 1];
}

//解析材质脚本
void ParseMaterialScript(GRE_Scence mysc, const char* scriptName)
{
	YMGRE_FILE* file = YMGRE_fopen(scriptName, "r"); //打开文件
	gre_log_explain(file == NULL, GRE_LOG_FILE, "该文件打开失败");

	char line[256];
	while (!YMGRE_feof(file))//文件没有结束
	{
		fgets(line, 256, file);//读取一行
		//跳过空白字符
		char* info = line;
		while(YMGRE_Isspace(*info))info++;
		//文件未结束
		if ((*info)!='\0')
		{
			// 遇到{加1, 遇到}减1, 当brackets为0时,跳出当前材质
			int  brackets = 0;
			// 出现新材质
			if (YMGRE_Memcmp(info, "material",sizeof("material") - 1) == 0)
			{
				info += sizeof("material");
				//取得材质名称
				char Mtnames[20],si=0;
				while (YMGRE_Isspace(*info))info++;//跳过空白字符
					while (!YMGRE_Isspace(*info) && (*info != '\0'))//非空白
					{
						gre_log_explain(si >= sizeof(Mtnames) - 1, GRE_LOG_FILE, "材质名称过长");
						Mtnames[si++] = *(info++);
				}
				Mtnames[si] = '\0';
				// 创建材质
				GRE_Material materail = YMGRE_Creat_Material(Mtnames);

				//再读取一行
				fgets(line, 256, file);
				//跳过空白字符
				info = line; 
				while (YMGRE_Isspace(*info))info++;
				if ((*info) == '{') // { 
					brackets++;

				//读取材质内容
				while (brackets != 0)
				{
					//再读取一行
					fgets(line, 256, file);
					//跳过空白字符
					info = line;
					while (YMGRE_Isspace(*info))info++;

					if ((*info) == '{') brackets++;
					else if ((*info) == '}') brackets--;
					else
					{
						// 环境光
						if (YMGRE_Memcmp(info, "ambient", sizeof("ambient") - 1) == 0)
						{
							info += sizeof("ambient");
							//读取浮点颜色
							materail->ambient.R = 255 * YMGRE_Strtof(info, &info);//自动跳过空白字符
							materail->ambient.G = 255 * YMGRE_Strtof(info, &info);
							materail->ambient.B = 255 * YMGRE_Strtof(info, &info);//后面的alpha通道直接丢弃
							continue;
						}

						// 漫反射
						if (YMGRE_Memcmp(info, "diffuse", sizeof("diffuse") - 1) == 0)
						{
							info += sizeof("diffuse");

							//读取浮点颜色
							materail->diffuse.R = 255 * YMGRE_Strtof(info, &info); //自动跳过空白字符
							materail->diffuse.G = 255 * YMGRE_Strtof(info, &info);
							materail->diffuse.B = 255 * YMGRE_Strtof(info, &info);//后面的alpha通道直接丢弃
							continue;
						}

						// 镜面反射
						if (YMGRE_Memcmp(info, "specular", sizeof("specular") - 1) == 0)
						{
							info += sizeof("diffuse");
							//读取浮点颜色
							materail->specular.R = 255 * YMGRE_Strtof(info, &info); //自动跳过空白字符
							materail->specular.G = 255 * YMGRE_Strtof(info, &info);
							materail->specular.B = 255 * YMGRE_Strtof(info, &info);//后面的alpha通道直接丢弃
							continue;
						}

						// 贴图
						if (YMGRE_Memcmp(info, "texture ", sizeof("texture ") - 1) == 0)// 这里使用"texture ",后面保留一个空格, 与texture_unit区别
						{
							info += sizeof("texture");
							while (YMGRE_Isspace(*info))info++;//跳过空白
							//读取贴图名字
							char textureName[64], mi = 0;
								while (!YMGRE_Isspace(*info) && (*info != '\0'))//非空白
								{
									gre_log_explain(mi >= sizeof(textureName) - 1, GRE_LOG_FILE, "贴图名称过长");
									textureName[mi++] = *(info++);
							}
							textureName[mi] = '\0';
							//贴图内容读取
							YMGRE_Bmp_File_LoadTo_Image( GetImageFromSamePath(scriptName,textureName),\
								&materail->pixel, &materail->width, &materail->height);
							continue;
						}
					}
				}
				//装载到全局材质库
				YMGRE_Scence_AddMeterial(mysc, materail);
			}
		}
	}

	YMGRE_fclose(file);
}


//////////////////////////////////////////  读取各种类型  ////////////////////////

char* ReadString(YMGRE_FILE* filep, int size)
{
	static char cbuff[256];
	int len = 0;
	if (size == -1) // 读取一行
	{
		char c;
		while (( (c = fgetc(filep)) != YMGRE_FILE_END ) && (c != '\r') && (c != '\n')) //二进制文件读取时fread不会遇到换行终止
		{
			cbuff[len++] = c;
		}
	}
	else
	{
		len = YMGRE_fread(cbuff, 1, sizeof(cbuff), filep);//返回成功读取的对象个数
	}

	cbuff[len] = '\0';
	return cbuff;
}

uint16 ReadChunk(YMGRE_FILE* filep)
{
	uint16 id;
	YMGRE_fread(&id, sizeof(uint16), 1, filep);//ID

	uint32 length;
	YMGRE_fread(&length, sizeof(uint32), 1, filep);//长度

	return id;
}

uint8 ReadBool(YMGRE_FILE* filep)
{
	uint8 re = 0;
	YMGRE_fread(&re, sizeof(uint8), 1, filep);
	return re;
}

uint32 ReadInt(YMGRE_FILE* filep)
{
	uint32 re;
	YMGRE_fread(&re, sizeof(uint32), 1, filep);
	return re;
}

uint16 ReadShort(YMGRE_FILE* filep)
{
	uint16 re;
	YMGRE_fread(&re, sizeof(uint16), 1, filep);
	return re;
}

//读取Mesh,并自动将材质添加到材质库中
GRE_Object4d YMGRE_LoadOgreMeshAndMaterial(GRE_Scence mysc,const char* meshpath)
{
	// 加载材质
	char* materialPath = GetMaterialFromSamePath(meshpath);
    ParseMaterialScript(mysc, materialPath);//加载材质

	//打开文件
	YMGRE_FILE* file = YMGRE_fopen(meshpath, "rb"); //必须以二进制方式打开，不然fread遇到换行符会终止
	gre_log_explain(file == NULL, GRE_LOG_FILE, "该文件打开失败");

	char* name = GetMeshNameFromPath(meshpath);//网格文件名字

	// 网格模型加载
	GRE_Object4d head = NULL;
	{
		// Chunk--M_HEADER
		uint16 HEADER = ReadShort(file);
		// Version
		char* Version = ReadString(file,-1);

		// Chunk--M_MESH
		uint16 MESH = ReadChunk(file);
		// skeletallyAnimated
		uint8 SkeletallyAnimated = ReadBool(file);

		// Chunk--M_SUBMESH
		uint16 SUBMESH = ReadChunk(file);
		GRE_Object4d curObject = NULL;
		//依次读出 submesh
		while (!YMGRE_feof(file) && (SUBMESH == OGRE_SUBMESH))
		{
			// 读取材质名称
			char* materiaName = ReadString(file,-1);//Materia

			// bool useSharedVertices
			uint8 UseSharedVertices = ReadBool(file);

			//  unsigned int indexCount
			uint32 indexCount = ReadInt(file);
			int polyonNumber = indexCount / 3; //三角片元个数

			// bool indexes32Bit
			uint8 indexes32Bit = ReadBool(file);
			uint16* polyIndex16 = NULL;
			uint32* polyIndex32 = NULL;
			if (indexes32Bit)
			{
				gre_log_explain(indexes32Bit, GRE_LOG_ParamI, "模型顶点数超过65535");
				// unsigned int* faceVertexIndices (indexCount)
				polyIndex32 = GRE_PolyIndex_Malloc(indexCount * sizeof(uint32));
				YMGRE_fread(polyIndex32, sizeof(uint32), indexCount, file);// 读取模型顶点索引
			}
			else
			{
				// unsigned short* faceVertexIndices (indexCount)
				polyIndex16 = GRE_PolyIndex_Malloc(indexCount * sizeof(uint16));
				//必须以二进制方式读取，不然这里不能一次性读出相应长度的数据
				fread(polyIndex16, sizeof(uint16), indexCount, file);// 读取模型定点索引
			}

			// Chunk--M_GEOMETRY
			uint16 GEOMETRY = ReadChunk(file);

			// unsigned int vertexCount
			uint32 vertexCount = ReadInt(file);
			int pointNumber = vertexCount;// 模型顶点个数

			// Chunk--M_GEOMETRY_VERTEX_DECLARATION
			uint16 GEOMETRY_VERTEX_DECLARATION = ReadChunk(file);
			// Chunk--M_GEOMETRY_VERTEX_ELEMENT
			uint16 GEOMETRY_VERTEX_ELEMENT = ReadChunk(file);
			//跳过
			while (!YMGRE_feof(file) && GEOMETRY_VERTEX_ELEMENT == OGRE_GEOMETRY_VERTEX_ELEMENT)
			{
				// unsigned short source;  	// buffer bind source
				uint16 source = ReadShort(file);
				// unsigned short type;    	// VertexElementType
				uint16 type = ReadShort(file);
				// unsigned short semantic; // VertexElementSemantic
				uint16 semantic = ReadShort(file);
				// unsigned short offset;	// start offset file buffer file bytes
				uint16 offset = ReadShort(file);
				// unsigned short index;	// index of the semantic (for colours and texture coords)
				uint16 index = ReadShort(file);
				GEOMETRY_VERTEX_ELEMENT = ReadChunk(file);
			}

			//Chunk--M_GEOMETRY_VERTEX_BUFFER
			uint16 GEOMETRY_VERTEX_BUFFER = GEOMETRY_VERTEX_ELEMENT;
			// unsigned short bindIndex;	// Index to bind this buffer to
			uint16 bindIndex = ReadShort(file);

			// unsigned short vertexSize;	// Per-vertex size, must agree with declaration at this index
			uint16 vertexSize = ReadShort(file);

			// Chunk--M_GEOMETRY_VERTEX_BUFFER_DATA
			uint16 GEOMETRY_VERTEX_BUFFER_DATA = ReadChunk(file);

			int32 vertexNum = vertexSize / sizeof(float32);
			// data buffer
			int32 bufferSize = vertexCount * vertexNum;

			// 读取顶点数据
			float32* vertexArray = GRE_malloc1(bufferSize * sizeof(float32));
			YMGRE_fread(vertexArray, sizeof(float32), bufferSize, file);//必须二进制读取，不然遇到换行会终止

			//创建模型
			if (head == NULL)
			{
				head = YMGRE_Creat_Object(pointNumber, polyonNumber, name, materiaName);
				curObject = head;
			}
			else
			{
				curObject->nextObject = YMGRE_Creat_Object(pointNumber, polyonNumber, name, materiaName);
				curObject = curObject->nextObject;
			}
			//物体初始化
			{
				//多边形顶点加载
				GRE_Vertex4d pointList = curObject->pointList;
				for (int32 i = 0,ptid = 0; i < bufferSize; i += vertexNum,ptid++)
				{
					// 顶点坐标
					pointList[ptid].pos.x = vertexArray[i];
					pointList[ptid].pos.y = vertexArray[i + 1];
					pointList[ptid].pos.z = vertexArray[i + 2];
					pointList[ptid].pos.w = 1; //类型为点
					// 顶点法线
					//vex.nx	= vertexArray[i + 3];
					//vex.ny	= vertexArray[i + 4];
					//vex.nz	= vertexArray[i + 5];
					// 顶点纹理坐标UV，在[0,1]范围内
					pointList[ptid].u = vertexArray[i + 6];
					pointList[ptid].v = vertexArray[i + 7];
					//变换后的顶点也具有相同的纹理
					curObject->pointList_[ptid].u = pointList[ptid].u;
					curObject->pointList_[ptid].v = pointList[ptid].v;
				}
				//释放内存
				GRE_free1(vertexArray);

				//多边形索引加载
				if (indexes32Bit)
				{
					for (int i = 0; i < polyonNumber; i++)
					{
						GRE_Polygon4d plythis = &curObject->polygonList[i];
						plythis->ishide = 0;
						plythis->num = 3;
						plythis->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
						//可能存在int32截断错误
						plythis->index[0] = polyIndex32[3 * i + 0];
						plythis->index[1] = polyIndex32[3 * i + 1];
						plythis->index[2] = polyIndex32[3 * i + 2];
						//可以考虑使用顶点法线来计算平面法向量
						
						//计算平面法向量,用于背面剔除
						GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&pointList[(plythis->index[1])].pos, &pointList[(plythis->index[0])].pos);
						GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&pointList[(plythis->index[2])].pos, &pointList[(plythis->index[1])].pos);
						GRE_Fvector4d n = YMGRE_Fvector4d_Cross(u, v);// n=u×v
						//记录
						YMGRE_Fvector4d_AssTo(&plythis->pN, n);// pn = n
						//释放内存
						YMGRE_Free_VectorF4d(u);
						YMGRE_Free_VectorF4d(v);
						YMGRE_Free_VectorF4d(n);
						//颜色
						plythis->planeColor = DefaultPolygonColor;
					}
					//释放内存
					GRE_PolyIndex_Free(polyIndex32);
				}
				else
				{
					for (int i = 0; i < polyonNumber; i++)
					{
						GRE_Polygon4d plythis = &curObject->polygonList[i];
						plythis->ishide = 0;
						plythis->num = 3;
						plythis->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
						plythis->index[0] = polyIndex16[3 * i + 0];
						plythis->index[1] = polyIndex16[3 * i + 1];
						plythis->index[2] = polyIndex16[3 * i + 2];
						//可以考虑使用顶点法线来计算平面法向量
						
						//计算平面法向量
						GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&pointList[(plythis->index[1])].pos, &pointList[(plythis->index[0])].pos);
						GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&pointList[(plythis->index[2])].pos, &pointList[(plythis->index[1])].pos);
						GRE_Fvector4d n = YMGRE_Fvector4d_Cross(u, v);// n=u×v
						//记录
						YMGRE_Fvector4d_AssTo(&plythis->pN, n);// pn = n
						//释放内存
						YMGRE_Free_VectorF4d(u);
						YMGRE_Free_VectorF4d(v);
						YMGRE_Free_VectorF4d(n);
						//颜色
						plythis->planeColor = DefaultPolygonColor;
					}
					//释放内存
					GRE_PolyIndex_Free(polyIndex16);
				}
			}


			// Chunk--M_SUBMESH_OPERATION
			uint16 SUBMESH_OPERATION = ReadChunk(file);
			// unsigned short operationType
			uint16 operationType = ReadShort(file);

			SUBMESH = ReadChunk(file);
		}

		gre_log_explain(head == NULL, GRE_LOG_FILE, "读取mesh失败");

		// Chunk--M_MESH_BOUNDS
		uint16 MESH_BOUNDS = SUBMESH;
		if (MESH_BOUNDS == OGRE_MESH_BOUNDS)
		{
				float32 bounds[7];
			// float minx, miny, minz
			// float maxx, maxy, maxz
			// float radius
			YMGRE_fread(bounds, sizeof(float32), 7, file);//读取包围盒
			head->BoundingBoxMax = (gre_fvector4d){ .x = bounds[0], .y = bounds[1], .z = bounds[2] };
			head->BoundingBoxMin = (gre_fvector4d){ .x = bounds[3], .y = bounds[4], .z = bounds[5] };
			head->boundType = GRE_Bounding_Box_AABB;//使用AABB盒进行包围
			//包围圆
			float32 maxR0 = (bounds[0] > bounds[1])? bounds[0]: bounds[1];
			head->BoundingSphereR = (maxR0 > bounds[2]) ? maxR0 : bounds[2];

			//Chunk--M_SUBMESH_NAME_TABLE
			uint16 SUBMESH_NAME_TABLE = ReadChunk(file);
			if (SUBMESH_NAME_TABLE == OGRE_SUBMESH_NAME_TABLE)//submesh的名字
			{
				// Chunk--M_SUBMESH_NAME_TABLE_ELEMENT
				uint16 SUBMESH_NAME_TABLE_ELEMENT = ReadChunk(file);
				while (!YMGRE_feof(file) && SUBMESH_NAME_TABLE_ELEMENT == OGRE_SUBMESH_NAME_TABLE_ELEMENT)
				{
					// short index
					// char* name
					int16 index = ReadShort(file);
					char* name = ReadString(file,-1);
					SUBMESH_NAME_TABLE_ELEMENT = ReadChunk(file);
				}
			}

		}
	}

	YMGRE_fclose(file);
	return head;
}


/////////////////////////////////////地图解析 ///////////////////////////////

//去除字符串两头的空白字符
static inline char* Remove_HeadAndTail_Space(char* soucre) {

	if (soucre == NULL) {
		return NULL;
	}
	char* pev = soucre; //字符串前指针
	char* end = soucre + (strlen(soucre) - 1); //字符串的从后向前找空字符的指针

	while (isspace(*pev) && pev <= end) { pev++; };//清除前面的空白字符
	while (isspace(*end) && end >= pev) { *end-- = '\0'; };//清除后面的空白字符
	//返回字符串头
	return pev;
}
//字符串匹配 temp不能不存在
static inline int temp_strcmp(const char* temp, const char* str)
{
	while (*temp != '\0')
	{
		if(*temp != *str)
			return *temp - *str;
		temp++;
		str++;
	}
	return 0;
}
//提取字符块
static inline char* Reserve_StrBlock(char* soucre, char ltip, char rtip)
{
	if (soucre == NULL)
		return NULL;

	char* pev = soucre; //字符串前指针
	char* end = soucre + (strlen(soucre) - 1); //字符串的从后向前找空字符的指针

	while ((*pev!= ltip) && pev <= end) { pev++; };//左字符清除
	while ((*end!= rtip) && end >= pev) { *end-- = '\0'; };//右字符清除
	//标识符清除
	if (*pev == ltip) pev++;
	if (*end == rtip) *end = '\0';
	return pev;
}
// x ,z ,size
static inline void Get_XZS(char* source, int32* x, int32* z, float32* blockSize)
{
	// <Grid X="15" Z="14" Size="10" />
	while ((*source != '\0') && (*source != 'X'))source++;
	source += 3; *x = strtol(source, &source, 10);

	while ((*source != '\0') && (*source != 'Z'))source++;
	source += 3; *z = strtol(source, &source, 10);

	while ((*source != '\0') && (*source != 'S'))source++;
	source += 6; *blockSize = strtof(source, &source);
}
// x ,y ,z
static inline void Get_XYZ(char* source, float32* x, float32* y, float32* z)
{
	// <Position X="-49.649979" Y="4.247236" Z="-5.005510" />
	while ((*source != '\0') && (*source != 'X'))source++;
	source += 3; *x = strtof(source, &source);

	while ((*source != '\0') && (*source != 'Y'))source++;
	source += 3; *y = strtof(source, &source);

	while ((*source != '\0') && (*source != 'Z'))source++;
	source += 3; *z = strtof(source, &source);
}
// x ,z ,value
static inline void Get_XZV(char* source, int32* x, int32* z, int32* value)
{
	// <Map X="6" Z="2" Value="0" />
	while ((*source != '\0') && (*source != 'X'))source++;
	source += 3; *x = strtol(source, &source,10);

	while ((*source != '\0') && (*source != 'Z'))source++;
	source += 3; *z = strtol(source, &source,10);

	while ((*source != '\0') && (*source != 'V'))source++;
	source += 7; *value = strtol(source, &source,10);
}
//获取根目录
static inline char* GetRootNameFromPath(const char* path)
{
	static char Path[256];
	gre_log_explain(strlen(path) >= sizeof(Path), GRE_LOG_FILE, "地图文件路径过长");
	int len = 0;
	while (path[len] != '\0')
	{
		Path[len] = path[len];
		len++;
	}
	Path[len] = '\0';
	//尾部替换
	while ((len > 0) && (Path[len] != '\\') && (Path[len] != '/'))
	{
		Path[len] = '\0';//  xxx/xxx.map
		len--;
	}
	gre_log_explain((Path[len] != '\\') && (Path[len] != '/'), GRE_LOG_FILE, "地图文件路径没有根目录");
	return Path;
}

//加载地形和材质
GRE_Terrain YMGRE_Load_SceneTerrainAndMaterial(GRE_Scence mysc, const char* mapPath)
{
	GRE_Terrain myterr = NULL;
	char mapName[50] = { 0 };
	char meshName[50] = { 0 };
	char nodeName[50] = { 0 };
	uint32 node_i = 0;

	//打开文件
	YMGRE_FILE* file = YMGRE_fopen(mapPath, "r"); //必须以文本只读方式打开
	gre_log_explain(file == NULL, GRE_LOG_FILE, "该文件打开失败");

	static char textline[256];
	while (fgets(textline, sizeof(textline), file) != NULL)
	{
		//删除字符串两端空白字符
		char* thisline = Remove_HeadAndTail_Space(textline);

		// <Config Name="mapname">
		if (temp_strcmp("<Config", thisline) == 0)
		{
				//地图名字块获取
				char* mnp= Reserve_StrBlock(thisline, '\"', '\"');
				gre_log_explain(strlen(mnp) >= sizeof(mapName), GRE_LOG_FILE, "地图名称过长");
				strcpy(mapName,mnp);
			//<Mesh>meshname</Mesh>
			if (fgets(textline, sizeof(textline), file) != NULL)
				{
					char* msp = Reserve_StrBlock(thisline, '>', '<');
					gre_log_explain(strlen(msp) + sizeof(".mesh") > sizeof(meshName), GRE_LOG_FILE, "网格名称过长");
					strcpy(meshName, msp);
				strcat(meshName, ".mesh"); //追加文件格式尾
				//<Grid X="15" Z="14" Size="10" />
				if (fgets(textline, sizeof(textline), file) != NULL)
				{
					int32 grid_x, grid_z;//网格的x，z方向数量，
					float32 block_size;//块大小
					Get_XZS(textline, &grid_x, &grid_z, &block_size);
					//创建地形
					myterr = YMGRE_Creat_Terrain(grid_x, grid_z, block_size, mapName);
				}

				gre_log_explain(myterr == NULL, GRE_LOG_FILE, "地形创建失败");
					//加载地形mesh
					char* rootPath = GetRootNameFromPath(mapPath);//map根目录
					gre_log_explain(strlen(rootPath) + strlen(meshName) >= 256, GRE_LOG_FILE, "网格文件路径过长");
					strcat(rootPath, meshName);//获取mesh名字
				myterr->mesh = YMGRE_LoadOgreMeshAndMaterial(mysc, rootPath);
			}

		}
		//<Nodes Number="46">
		else if (temp_strcmp("<Nodes", thisline) == 0)
		{
			gre_log_explain(myterr == NULL, GRE_LOG_FILE, "地形创建失败");
			//障碍物节点数获取
			char* mnp = Reserve_StrBlock(thisline, '\"', '\"');
			myterr->obstaclesNum = strtol(mnp, &mnp, 10);
			myterr->obstaclesList = GRE_TerrainObstacles_Malloc(myterr->obstaclesNum * sizeof(myterr->obstaclesList[0]));
			//初始化为0
			GRE_memset(myterr->obstaclesList, 0, myterr->obstaclesNum * sizeof(myterr->obstaclesList[0]));
			
			//提取每个节点
			node_i = 0;
			while (fgets(textline, sizeof(textline), file) != NULL)
			{
				//删除字符串两端空白字符
				thisline = Remove_HeadAndTail_Space(textline);
				//<Node Name = "Tree_3#120">
				if (temp_strcmp("<Node", thisline) == 0)
				{
						//节点名字块获取
						char* nnp = Reserve_StrBlock(thisline, '\"', '\"');
						gre_log_explain(strlen(nnp) >= sizeof(nodeName), GRE_LOG_FILE, "节点名称过长");
						strcpy(nodeName, nnp);

					//<Mesh>Tree_3< / Mesh>
						fgets(textline, sizeof(textline), file);
						char* msp = Reserve_StrBlock(thisline, '>', '<');
						gre_log_explain(strlen(msp) + sizeof(".mesh") > sizeof(meshName), GRE_LOG_FILE, "网格名称过长");
						strcpy(meshName, msp);
					int meshNameLen = strlen(msp) + 1;

					//是否为新的mesh
					GRE_Object4d findResult = NULL;
					for (int mhi = 0;mhi < node_i; mhi++)
					{
						GRE_Object4d curMeshp= myterr->obstaclesList[mhi];
						if ((curMeshp->objNameLen == meshNameLen) && (YMGRE_Memcmp(curMeshp->objName, meshName, meshNameLen) == 0))
						{
							findResult = curMeshp;
							break;
						}
					}

					GRE_Object4d addObj = NULL;
					if (findResult == NULL)//不在列表中
					{
							strcat(meshName, ".mesh"); //追加文件格式尾
							char* rootPath = GetRootNameFromPath(mapPath);//map根目录
							gre_log_explain(strlen(rootPath) + strlen(meshName) >= 256, GRE_LOG_FILE, "网格文件路径过长");
							strcat(rootPath, meshName);//获取mesh名字
						//从文件中加载
						addObj = YMGRE_LoadOgreMeshAndMaterial(mysc, rootPath);
					}
					//已经存在
					else
					{
						addObj = YMGRE_Object_Clone(findResult);//克隆
					}

					gre_log_explain(node_i >= myterr->obstaclesNum, GRE_LOG_FILE, "地形障碍数统计错误");
					//将mesh添加到地形中
					myterr->obstaclesList[node_i++] = addObj;

					//<Position X = "-49.649979" Y = "4.247236" Z = "-5.005510" / >
					fgets(textline, sizeof(textline), file);
					float32 pos_x, pos_y, pos_z;
					Get_XYZ(textline, &pos_x, &pos_y, &pos_z);
					//重置mesh的位置
					YMGRE_SetObject_Pos(addObj, pos_x, pos_y, pos_z);

					// <Map X="6" Z="2" Value="0" />
					fgets(textline, sizeof(textline), file);
					int map_x, map_z, map_v;
					Get_XZV(textline, &map_x, &map_z, &map_v);
					//障碍物在地图网格中的位置，用于做障碍碰撞检测
					
				    //</Node>
					fgets(textline, sizeof(textline), file);	
				}
			}
		}
	}

	YMGRE_fclose(file);
	return myterr;
}


