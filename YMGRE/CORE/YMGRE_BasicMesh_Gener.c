#include "./YMGRE_MathBase.h"

#if 0

///////////////////////////////基本图元生成//////////////////////////////////

//矩形平面
GRE_Object4d YMGRE_Gener_RectPlane(uint16 w, uint16 h,float32 blockSize,char* objName,char* materiaName)
{
	//   0   1   2
	//   *---*---*
	//   | / | / |
	// 1 *---*---*
	int ptNum = (w + 1) * (h + 1);
	int agNum = 2 * w * h;
	GRE_Object4d head = YMGRE_Creat_Object((w + 1)* (h + 1), agNum, objName, materiaName);

	//构成单元初始化
	EFloat offsetX = (col - 1) * blockSize / 2;
	EFloat offsetZ = (row - 1) * blockSize / 2;

	EFloat minY = 0, maxY = 0;
	EFloat u = uTitle / (col - 1);
	EFloat v = vTitle / (row - 1);

	for (int r = 0,rowMax = (w + 1); r < rowMax; r++)
	{
		for (int c = 0,colMax = (h + 1); c < colMax; c++)
		{
			GRE_Vertex4d vts = &head->pointList[r * colMax + c];

			vts->pos = 0;
			vts->u = 0;
			vts->v = 0;
			EVertex4D vex;
			// 顶点坐标
			EColor color = heigthMap->getPixel(c, r);
			vex.x = c * blockSize - offsetX;
			vex.y = 0.1f * (0.299f * color.r + 0.587f * color.g + 0.114f * color.b);
			vex.z = r * blockSize - offsetZ;

			if (vex.y < minY)
				minY = vex.y;
			if (vex.y > maxY)
				maxY = vex.y;

			// 顶点UV
			vex.u = c * u;
			vex.v = r * v;

			obj->localList.push_back(vex);
			obj->transformList.push_back(vex);

			// 计算顶点索引
			if (r < row - 1 && c < col - 1)
			{
				EPolyon4D poly;
				poly.state = EPOLY_STATE_ACTIVE;
				poly.attribute = EPOLY_ATTR_VERTEX_POSITION | EPOLY_ATTR_VERTEX_UV;

				poly.verList = &obj->transformList;

				poly.verIndex[0] = r * col + c;
				poly.verIndex[1] = (r + 1) * col + c;
				poly.verIndex[2] = r * col + c + 1;
				obj->polyonList.push_back(poly);

				poly.verIndex[0] = r * col + c + 1;
				poly.verIndex[1] = (r + 1) * col + c;
				poly.verIndex[2] = (r + 1) * col + c + 1;
				obj->polyonList.push_back(poly);
			}
		}
	}
	for (int32 i = 0; i < ptNum; i++)//多边形顶点加载
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
}

//创建地形
//void createTerrain(const EString& heightMapFileName,
//	const EString& textureFileName, EFloat uTitle, EFloat vTitle, EFloat blockSize)
//{
//	if (mTerrainMesh)
//		SafeDelete(mTerrainMesh);
//
//	Log("Loading Terrain HeightMap : #%s Texture : ...", heightMapFileName.c_str(), textureFileName.c_str());
//
//	EBitmap* heigthMap = new EBitmap(heightMapFileName);
//
//	if (!heigthMap->isValid() ||
//		heigthMap->getWidth() % 2 == 0 ||
//		heigthMap->getHeight() % 2 == 0)
//	{
//		SafeDelete(heigthMap);
//		return NULL;
//	}
//
//	EMaterial* mat = new EMaterial();
//	mat->bitmap = new EBitmap(textureFileName);
//	SetMaterial("Terrain", mat);
//
//	EInt row = heigthMap->getHeight();
//	EInt col = heigthMap->getWidth();
//
//	// Y = 0.299R+0.587G+0.114B
//	mTerrainMesh = new EMesh();
//	EObject4D* obj = new EObject4D();
//	mTerrainMesh->mObject = obj;
//
//	obj->name = "Terrain";
//	obj->materiaName = "Terrain";
//	obj->state = EOBJECT_STATE_ACTIVE | EOBJECT_STATE_VISIBLE;
//	obj->scale = EVector4D(1, 1, 1);
//	obj->worldPosition = EVector4D::ZERO;
//	obj->vertexNumber = row * col;
//	obj->polyonNumber = (row - 1) * (col - 1) * 2;
//
//	mTerrainMesh->mVertexNumber = obj->vertexNumber;
//	mTerrainMesh->mPolyonNumber = obj->polyonNumber;
//
//	// 定点列表
//	obj->localList.reserve(obj->vertexNumber);
//	obj->transformList.reserve(obj->vertexNumber);
//	// 多边形列表
//	obj->polyonList.reserve(obj->polyonNumber);
//
//	EFloat offsetX = (col - 1) * blockSize / 2;
//	EFloat offsetZ = (row - 1) * blockSize / 2;
//
//	EFloat minY = 0, maxY = 0;
//	EFloat u = uTitle / (col - 1);
//	EFloat v = vTitle / (row - 1);
//	for (EInt r = 0; r < row; r++)
//	{
//		for (EInt c = 0; c < col; c++)
//		{
//			EVertex4D vex;
//			// 顶点坐标
//			EColor color = heigthMap->getPixel(c, r);
//			vex.x = c * blockSize - offsetX;
//			vex.y = 0.1f * (0.299f * color.r + 0.587f * color.g + 0.114f * color.b);
//			vex.z = r * blockSize - offsetZ;
//
//			if (vex.y < minY)
//				minY = vex.y;
//			if (vex.y > maxY)
//				maxY = vex.y;
//
//			// 顶点UV
//			vex.u = c * u;
//			vex.v = r * v;
//
//			obj->localList.push_back(vex);
//			obj->transformList.push_back(vex);
//
//			// 计算顶点索引
//			if (r < row - 1 && c < col - 1)
//			{
//				EPolyon4D poly;
//				poly.state = EPOLY_STATE_ACTIVE;
//				poly.attribute = EPOLY_ATTR_VERTEX_POSITION | EPOLY_ATTR_VERTEX_UV;
//
//				poly.verList = &obj->transformList;
//
//				poly.verIndex[0] = r * col + c;
//				poly.verIndex[1] = (r + 1) * col + c;
//				poly.verIndex[2] = r * col + c + 1;
//				obj->polyonList.push_back(poly);
//
//				poly.verIndex[0] = r * col + c + 1;
//				poly.verIndex[1] = (r + 1) * col + c;
//				poly.verIndex[2] = (r + 1) * col + c + 1;
//				obj->polyonList.push_back(poly);
//			}
//		}
//	}
//
//	obj->maxBoundingBox = EVector4D(offsetX, maxY, offsetZ);
//	obj->minBoundingBox = EVector4D(-offsetX, minY, -offsetZ);
//	obj->maxRadius = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
//
//	SafeDelete(heigthMap);
//	mMeshs.insert(std::make_pair("Terrain", mTerrainMesh));
//	// 设置不执行剔除操作
//	mTerrainMesh->setCullFlag(false);
//
//	Log("Terrain Load Sucessed!");
//	return mTerrainMesh;
//}

//立方体

//正三棱柱

//六棱锥

//正十二面体

//球体
GRE_Object4d YMGRE_MeshGener_Sphere(float32 radius,uint16 latitude , uint16 longitude,char* name, char* materiaName)
{
	int N1 = latitude;//纬度180度划分
	int N2 = longitude;//经度360划分
	//顶点列表
    //      0             >> N1 = 3
	//      *
	//    / | \
	//   *--*--*   1 2 3  >> N2 = 2
    //   |  |  |
    //   *--*--*   4 5 6
	//    \ | /
	//      *
	//      7
	int ptNum = 2 + (N1 - 1) * N2; //两个南北极点 + N-1个中间点
	int polyNum = 2 * N2 + (N1 - 1) * N2;//南北极点平面 2 * N2 + 中间平面 (N1-2) * N2
	GRE_Object4d myobj = YMGRE_Creat_Object(ptNum, polyNum, name, materiaName);

	//计算顶点坐标
	//北极点
	myobj->pointList[0].pos.x = 0; myobj->pointList[0].pos.y = radius; myobj->pointList[0].pos.z = 0;
	myobj->pointList[0].pos.w = 1;
	//南极点
	int indx = myobj->pointNum - 1;
	myobj->pointList[indx].pos.x = 0; myobj->pointList[indx].pos.y = -radius; myobj->pointList[indx].pos.z = 0;
	myobj->pointList[indx].pos.w = 1;

    //面片的夹角
	float32 galpha = 180 / N1;//纬度划分
	float32 gbeta = 360 / N2;//经度划分
	//中间点
	for (int i = 0; i < N1 - 1; i++)
	{
		float alp = (i + 1) * galpha * YMGRE_Deg2Rad;
		for (int j = 0; j < N2; j++)
		{
			float bet = j * gbeta * YMGRE_Deg2Rad;
			int indx = i * N2 + j + 1;
			myobj->pointList[indx].pos.x = radius * sin(alp) * sin(bet);
			myobj->pointList[indx].pos.y = radius * cos(alp);
			myobj->pointList[indx].pos.z = radius * sin(alp) * cos(bet);
			myobj->pointList[indx].pos.w = 1;
		}
	}

	// 构建南北极三角形面片
	indx = N2 + (N1 - 2) * N2; //北极索引起点
	for (int i = 0; i < N2; i++)
	{
		int j = i + 1;
		if (j == N2)j = 0; //首尾相连
		//        0
		//      / | \
		// 1 2 3 ... N2-1
		int polynum = 3;//三角形
		myobj->polygonList[i].num = polynum;
		myobj->polygonList[i].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj->polygonList[i].index[0] = 0;
		myobj->polygonList[i].index[1] = i + 1; //北极点偏移量为 1
		myobj->polygonList[i].index[2] = j + 1;
		myobj->polygonList[i].ishide = 0;//未被隐藏

		// 1 2 3 ... N2-1
		//      \ | /
		//        0

		//indx = (N1 - 1) * N2;
		int ii = indx + i;
		myobj->polygonList[ii].num = polynum;//三角形
		myobj->polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj->polygonList[ii].index[0] = indx - N2 + i + 1;
		myobj->polygonList[ii].index[1] = indx + 1;//北极点
		myobj->polygonList[ii].index[2] = indx - N2 + j + 1;

		myobj->polygonList[ii].ishide = 0;//未被隐藏
	}
	//中间四边形构建
	for (int i = 1; i < N1 - 1; i++)
	{
		indx = i * N2;
		for (int j = 0; j < N2; j++)
		{
			int tj = j + 1;
			if (tj == N2)tj = 0; //首尾相连
			//    2  3
			//    /  \
			// 1  2  3 ... N2-1
			int ii = indx + j;
			//indx = i * N2
			int polynum = 4;//四边形
			myobj->polygonList[ii].num = polynum;
			myobj->polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

			myobj->polygonList[ii].index[0] = indx - N2 + j + 1; //+1代表北极点偏移为1
			myobj->polygonList[ii].index[1] = indx + j + 1;
			myobj->polygonList[ii].index[2] = indx + tj + 1;
			myobj->polygonList[ii].index[3] = indx - N2 + tj + 1;
			myobj->polygonList[ii].ishide = 0;//未被隐藏
		}
	}

	//计算平面法向量
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		//计算平面法向量
		// 0   3
		// 1   2
		//u= p1->p2  v= p1->p3 ，边按逆时针排布
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		GRE_Vertex4d points = myobj->pointList;
		//计算平面法向量
		int i1 = thispoly->index[0];
		int i2 = thispoly->index[1];
		int i3 = thispoly->index[2];

		GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&points[i2].pos, &points[i1].pos);
		GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&points[i3].pos, &points[i1].pos);
		GRE_Fvector4d n = YMGRE_Fvector4d_Cross(u, v);// n=u×v

		YMGRE_Fvector4d_AssTo(&thispoly->pN, n);// pn = n
		//释放内存
		YMGRE_Free_VectorF4d(u);
		YMGRE_Free_VectorF4d(v);
		YMGRE_Free_VectorF4d(n);

		//设置平面的颜色
		GRErgb24 clor;
		int index_y = i / N2; //每行N2个数据，一共N1行
		int index_x = i % N2; //
		uint8* rgb16 = YMGRE_World_data; //w = 640  h=320
		float32 kx = (640.0f - 1) / (N2 - 1);
		float32 ky = (320.0f - 1) / (N1 - 1);

		//转为图形索引
		int i_x = index_x * kx;
		int i_y = index_y * ky;
		int rgb_index = (i_y * 640 + i_x) * 2;
		_color16_t thiscolor = rgb16[rgb_index] | ((uint16)rgb16[rgb_index + 1] << 8);
		//clor.R = (i / N2) * 255 / N1;
		clor.R = (thiscolor & 0xF800) >> 8;
		clor.G = (thiscolor & 0x07E0) >> 3;
		clor.B = (thiscolor & 0x001F) << 3;
		thispoly->planeColor = clor;
	}
}

//圆柱
GRE_Object4d YMGRE_MeshGener_Cylinder(float32 radius, float32 height, char* name, char* materiaName)
{
	int N1 = latitude;//纵向网格数
	int N2 = longitude;//圆周网格数
	//顶点列表
	//      0             >> N1 = 3
	//      *
	//    / | \
	//   *--*--*   1 2 3  >> N2 = 2
	//   |  |  |
	//   *--*--*   4 5 6
	//    \ | /
	//      *
	//      7
	int ptNum = 2 + (N1 - 1) * N2; //两个南北极点 + N-1个中间点
	int polyNum = 2 * N2 + (N1 - 1) * N2;//南北极点平面 2 * N2 + 中间平面 (N1-2) * N2
	GRE_Object4d myobj = YMGRE_Creat_Object(ptNum, polyNum, name, materiaName);

	//计算顶点坐标
	//北极点
	myobj->pointList[0].pos.x = 0; myobj->pointList[0].pos.y = radius; myobj->pointList[0].pos.z = 0;
	myobj->pointList[0].pos.w = 1;
	//南极点
	int indx = myobj->pointNum - 1;
	myobj->pointList[indx].pos.x = 0; myobj->pointList[indx].pos.y = -radius; myobj->pointList[indx].pos.z = 0;
	myobj->pointList[indx].pos.w = 1;

	//
	float32 galpha = 180 / N1;//周的夹角
	float32 gbeta = 360 / N2;//经度划分
	//中间点
	for (int i = 0; i < N1 - 1; i++)
	{
		float alp = (i + 1) * galpha * YMGRE_Deg2Rad;
		for (int j = 0; j < N2; j++)
		{
			float bet = j * gbeta * YMGRE_Deg2Rad;
			int indx = i * N2 + j + 1;
			myobj->pointList[indx].pos.x = radius * sin(alp) * sin(bet);
			myobj->pointList[indx].pos.y = radius * cos(alp);
			myobj->pointList[indx].pos.z = radius * sin(alp) * cos(bet);
			myobj->pointList[indx].pos.w = 1;
		}
	}

	// 构建南北极三角形面片
	indx = N2 + (N1 - 2) * N2; //北极索引起点
	for (int i = 0; i < N2; i++)
	{
		int j = i + 1;
		if (j == N2)j = 0; //首尾相连
		//        0
		//      / | \
		// 1 2 3 ... N2-1
		int polynum = 3;//三角形
		myobj->polygonList[i].num = polynum;
		myobj->polygonList[i].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj->polygonList[i].index[0] = 0;
		myobj->polygonList[i].index[1] = i + 1; //北极点偏移量为 1
		myobj->polygonList[i].index[2] = j + 1;
		myobj->polygonList[i].ishide = 0;//未被隐藏

		// 1 2 3 ... N2-1
		//      \ | /
		//        0

		//indx = (N1 - 1) * N2;
		int ii = indx + i;
		myobj->polygonList[ii].num = polynum;//三角形
		myobj->polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

		myobj->polygonList[ii].index[0] = indx - N2 + i + 1;
		myobj->polygonList[ii].index[1] = indx + 1;//北极点
		myobj->polygonList[ii].index[2] = indx - N2 + j + 1;

		myobj->polygonList[ii].ishide = 0;//未被隐藏
	}
	//中间四边形构建
	for (int i = 1; i < N1 - 1; i++)
	{
		indx = i * N2;
		for (int j = 0; j < N2; j++)
		{
			int tj = j + 1;
			if (tj == N2)tj = 0; //首尾相连
			//    2  3
			//    /  \
			// 1  2  3 ... N2-1
			int ii = indx + j;
			//indx = i * N2
			int polynum = 4;//四边形
			myobj->polygonList[ii].num = polynum;
			myobj->polygonList[ii].index = GRE_PolyIndex_Malloc(polynum * sizeof(uint32*));

			myobj->polygonList[ii].index[0] = indx - N2 + j + 1; //+1代表北极点偏移为1
			myobj->polygonList[ii].index[1] = indx + j + 1;
			myobj->polygonList[ii].index[2] = indx + tj + 1;
			myobj->polygonList[ii].index[3] = indx - N2 + tj + 1;
			myobj->polygonList[ii].ishide = 0;//未被隐藏
		}
	}

	//计算平面法向量
	for (int i = 0; i < myobj->polygonNum; i++)
	{
		//计算平面法向量
		// 0   3
		// 1   2
		//u= p1->p2  v= p1->p3 ，边按逆时针排布
		GRE_Polygon4d thispoly = &myobj->polygonList[i];
		GRE_Vertex4d points = myobj->pointList;
		//计算平面法向量
		int i1 = thispoly->index[0];
		int i2 = thispoly->index[1];
		int i3 = thispoly->index[2];

		GRE_Fvector4d u = YMGRE_Fvector4d_Sub(&points[i2].pos, &points[i1].pos);
		GRE_Fvector4d v = YMGRE_Fvector4d_Sub(&points[i3].pos, &points[i1].pos);
		GRE_Fvector4d n = YMGRE_Fvector4d_Cross(u, v);// n=u×v

		YMGRE_Fvector4d_AssTo(&thispoly->pN, n);// pn = n
		//释放内存
		YMGRE_Free_VectorF4d(u);
		YMGRE_Free_VectorF4d(v);
		YMGRE_Free_VectorF4d(n);

		//设置平面的颜色
		GRErgb24 clor;
		int index_y = i / N2; //每行N2个数据，一共N1行
		int index_x = i % N2; //
		uint8* rgb16 = YMGRE_World_data; //w = 640  h=320
		float32 kx = (640.0f - 1) / (N2 - 1);
		float32 ky = (320.0f - 1) / (N1 - 1);

		//转为图形索引
		int i_x = index_x * kx;
		int i_y = index_y * ky;
		int rgb_index = (i_y * 640 + i_x) * 2;
		_color16_t thiscolor = rgb16[rgb_index] | ((uint16)rgb16[rgb_index + 1] << 8);
		//clor.R = (i / N2) * 255 / N1;
		clor.R = (thiscolor & 0xF800) >> 8;
		clor.G = (thiscolor & 0x07E0) >> 3;
		clor.B = (thiscolor & 0x001F) << 3;
		thispoly->planeColor = clor;
	}
}

//圆锥

//圆环

#endif

#include "./YMGRE_BasicMesh_Gener.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../DEBUG/YMGRE_Debug.h"

/////////////////////////////////////////// 基础三角网格生成 ///////////////////////////////////////////

//设置单个三角面索引，同时根据顶点绕序计算平面法向量
static void meshSetTriangle(GRE_Object4d object, int polygonIndex, uint16 i0, uint16 i1, uint16 i2, GRErgb24 color)
{
	GRE_Polygon4d polygon = &object->polygonList[polygonIndex];
	polygon->num = 3;
	polygon->index = GRE_PolyIndex_Malloc(3 * sizeof(uint16));
	gre_log_explain(polygon->index == NULL, GRE_LOG_Mem1, "基础网格索引内存申请失败");
	polygon->index[0] = i0;
	polygon->index[1] = i1;
	polygon->index[2] = i2;
	polygon->planeColor = color;
	polygon->ishide = 0;

	gre_fvector4d u;
	gre_fvector4d v;
	YMGRE_Fvector4d_SubToResult(&object->pointList[i1].pos, &object->pointList[i0].pos, &u);
	YMGRE_Fvector4d_SubToResult(&object->pointList[i2].pos, &object->pointList[i0].pos, &v);
	YMGRE_Fvector4d_CrossToResult(&u, &v, &polygon->pN);
}

//封闭凸多面体按三角形中心方向修正为外向绕序
static void meshSetTriangleOutward(GRE_Object4d object, int polygonIndex,
	uint16 i0, uint16 i1, uint16 i2, GRErgb24 color)
{
	gre_fvector4d u;
	gre_fvector4d v;
	gre_fvector4d normal;
	GRE_Fvector4d p0 = &object->pointList[i0].pos;
	GRE_Fvector4d p1 = &object->pointList[i1].pos;
	GRE_Fvector4d p2 = &object->pointList[i2].pos;
	YMGRE_Fvector4d_SubToResult(p1, p0, &u);
	YMGRE_Fvector4d_SubToResult(p2, p0, &v);
	YMGRE_Fvector4d_CrossToResult(&u, &v, &normal);
	float32 centerDot = normal.x * (p0->x + p1->x + p2->x) +
		normal.y * (p0->y + p1->y + p2->y) +
		normal.z * (p0->z + p1->z + p2->z);
	if (centerDot < 0.0f)
		meshSetTriangle(object, polygonIndex, i0, i2, i1, color);
	else
		meshSetTriangle(object, polygonIndex, i0, i1, i2, color);
}

//球形边界同时用于球体和顶点都位于外接球面的正多面体
static void meshSetSphereBounds(GRE_Object4d object, float32 radius)
{
	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = radius;
	object->BoundingBoxMin = (gre_fvector4d){ -radius, -radius, -radius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  radius,  radius,  radius, 1 };
}

//在 XZ 平面生成法线朝 +Y 的矩形网格，每个网格单元拆成两个三角形
GRE_Object4d YMGRE_MeshGener_RectPlane(float32 width, float32 depth, uint16 rows, uint16 columns,
	GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain((width <= 0.0f) || (depth <= 0.0f) || (rows == 0) || (columns == 0),
		GRE_LOG_ParamI, "矩形平面尺寸或分段数错误");
	uint32 rowPointNum = (uint32)rows + 1;
	uint32 columnPointNum = (uint32)columns + 1;
	gre_log_explain(rowPointNum > 65535u / columnPointNum,
		GRE_LOG_ParamI, "矩形平面顶点数超过索引范围");
	uint32 pointNumValue = rowPointNum * columnPointNum;
	uint32 polygonNumValue = 2u * rows * columns;
	int pointNum = pointNumValue;
	int polygonNum = polygonNumValue;
	GRE_Object4d object = YMGRE_Creat_Object(pointNum, polygonNum, name, materiaName);
	float32 halfWidth = width * 0.5f;
	float32 halfDepth = depth * 0.5f;

	for (uint16 row = 0; row <= rows; row++)
	{
		float32 z = -halfDepth + depth * row / rows;
		for (uint16 column = 0; column <= columns; column++)
		{
			float32 x = -halfWidth + width * column / columns;
			int point = row * (columns + 1) + column;
			object->pointList[point].pos = (gre_fvector4d){ x, 0, z, 1 };
			object->pointList[point].u = (float32)column / columns;
			object->pointList[point].v = (float32)row / rows;
		}
	}

	int polygon = 0;
	for (uint16 row = 0; row < rows; row++)
	{
		for (uint16 column = 0; column < columns; column++)
		{
			uint16 topLeft = row * (columns + 1) + column;
			uint16 topRight = topLeft + 1;
			uint16 bottomLeft = topLeft + columns + 1;
			uint16 bottomRight = bottomLeft + 1;
			meshSetTriangle(object, polygon++, topLeft, bottomLeft, topRight, color);
			meshSetTriangle(object, polygon++, topRight, bottomLeft, bottomRight, color);
		}
	}

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = YMGRE_Sqrt(halfWidth * halfWidth + halfDepth * halfDepth);
	object->BoundingBoxMin = (gre_fvector4d){ -halfWidth, 0, -halfDepth, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  halfWidth, 0,  halfDepth, 1 };
	return object;
}

//生成以原点为中心的立方体，每个面使用两个三角形并保持外向绕序
GRE_Object4d YMGRE_MeshGener_Cube(float32 side, GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain(side <= 0.0f, GRE_LOG_ParamI, "立方体边长必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(8, 12, name, materiaName);
	float32 h = side * 0.5f;
	object->pointList[0].pos = (gre_fvector4d){ -h, -h, -h, 1 };
	object->pointList[1].pos = (gre_fvector4d){  h, -h, -h, 1 };
	object->pointList[2].pos = (gre_fvector4d){  h,  h, -h, 1 };
	object->pointList[3].pos = (gre_fvector4d){ -h,  h, -h, 1 };
	object->pointList[4].pos = (gre_fvector4d){ -h, -h,  h, 1 };
	object->pointList[5].pos = (gre_fvector4d){  h, -h,  h, 1 };
	object->pointList[6].pos = (gre_fvector4d){  h,  h,  h, 1 };
	object->pointList[7].pos = (gre_fvector4d){ -h,  h,  h, 1 };

	meshSetTriangle(object, 0, 0, 2, 1, color);//前
	meshSetTriangle(object, 1, 0, 3, 2, color);
	meshSetTriangle(object, 2, 4, 5, 6, color);//后
	meshSetTriangle(object, 3, 4, 6, 7, color);
	meshSetTriangle(object, 4, 0, 4, 7, color);//左
	meshSetTriangle(object, 5, 0, 7, 3, color);
	meshSetTriangle(object, 6, 1, 2, 6, color);//右
	meshSetTriangle(object, 7, 1, 6, 5, color);
	meshSetTriangle(object, 8, 0, 1, 5, color);//下
	meshSetTriangle(object, 9, 0, 5, 4, color);
	meshSetTriangle(object, 10, 3, 7, 6, color);//上
	meshSetTriangle(object, 11, 3, 6, 2, color);

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = h * YMGRE_Sqrt(3.0f);
	object->BoundingBoxMin = (gre_fvector4d){ -h, -h, -h, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  h,  h,  h, 1 };
	return object;
}

//生成以原点为中心的长方体，宽、高、深分别对应 X、Y、Z 轴
GRE_Object4d YMGRE_MeshGener_Box(float32 width, float32 height, float32 depth,
	GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain((width <= 0.0f) || (height <= 0.0f) || (depth <= 0.0f), GRE_LOG_ParamI,
		"长方体尺寸必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(8, 12, name, materiaName);
	float32 halfWidth = width * 0.5f;
	float32 halfHeight = height * 0.5f;
	float32 halfDepth = depth * 0.5f;
	object->pointList[0].pos = (gre_fvector4d){ -halfWidth, -halfHeight, -halfDepth, 1 };
	object->pointList[1].pos = (gre_fvector4d){  halfWidth, -halfHeight, -halfDepth, 1 };
	object->pointList[2].pos = (gre_fvector4d){  halfWidth,  halfHeight, -halfDepth, 1 };
	object->pointList[3].pos = (gre_fvector4d){ -halfWidth,  halfHeight, -halfDepth, 1 };
	object->pointList[4].pos = (gre_fvector4d){ -halfWidth, -halfHeight,  halfDepth, 1 };
	object->pointList[5].pos = (gre_fvector4d){  halfWidth, -halfHeight,  halfDepth, 1 };
	object->pointList[6].pos = (gre_fvector4d){  halfWidth,  halfHeight,  halfDepth, 1 };
	object->pointList[7].pos = (gre_fvector4d){ -halfWidth,  halfHeight,  halfDepth, 1 };

	meshSetTriangle(object, 0, 0, 2, 1, color);//前
	meshSetTriangle(object, 1, 0, 3, 2, color);
	meshSetTriangle(object, 2, 4, 5, 6, color);//后
	meshSetTriangle(object, 3, 4, 6, 7, color);
	meshSetTriangle(object, 4, 0, 4, 7, color);//左
	meshSetTriangle(object, 5, 0, 7, 3, color);
	meshSetTriangle(object, 6, 1, 2, 6, color);//右
	meshSetTriangle(object, 7, 1, 6, 5, color);
	meshSetTriangle(object, 8, 0, 1, 5, color);//下
	meshSetTriangle(object, 9, 0, 5, 4, color);
	meshSetTriangle(object, 10, 3, 7, 6, color);//上
	meshSetTriangle(object, 11, 3, 6, 2, color);

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = YMGRE_Sqrt(halfWidth * halfWidth + halfHeight * halfHeight +
		halfDepth * halfDepth);
	object->BoundingBoxMin = (gre_fvector4d){ -halfWidth, -halfHeight, -halfDepth, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  halfWidth,  halfHeight,  halfDepth, 1 };
	return object;
}

//沿 Y 轴生成封闭圆柱，segments 同时控制端面和侧面的圆周分段
GRE_Object4d YMGRE_MeshGener_Cylinder(float32 radius, float32 height, uint16 segments,
	GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain((radius <= 0.0f) || (height <= 0.0f) || (segments < 3), GRE_LOG_ParamI,
		"圆柱半径、高度或分段数错误");
	GRE_Object4d object = YMGRE_Creat_Object(2 + 2 * segments, 4 * segments, name, materiaName);
	float32 h = height * 0.5f;
	object->pointList[0].pos = (gre_fvector4d){ 0, h, 0, 1 };//上表面圆心
	object->pointList[1].pos = (gre_fvector4d){ 0, -h, 0, 1 };//下表面圆心

	for (uint16 i = 0; i < segments; i++)
	{
		float32 angle = YMGRE_2Pai * i / segments;
		float32 x = radius * YMGRE_Cos(angle);
		float32 z = radius * YMGRE_Sin(angle);
		object->pointList[2 + i].pos = (gre_fvector4d){ x, h, z, 1 };
		object->pointList[2 + segments + i].pos = (gre_fvector4d){ x, -h, z, 1 };
	}

	for (uint16 i = 0; i < segments; i++)
	{
		uint16 next = (i + 1 == segments) ? 0 : i + 1;
		uint16 top = 2 + i;
		uint16 topNext = 2 + next;
		uint16 bottom = 2 + segments + i;
		uint16 bottomNext = 2 + segments + next;
		int polygon = 4 * i;
		meshSetTriangle(object, polygon, 0, topNext, top, color);//上表面
		meshSetTriangle(object, polygon + 1, 1, bottom, bottomNext, color);//下表面
		meshSetTriangle(object, polygon + 2, top, bottomNext, bottom, color);//侧面
		meshSetTriangle(object, polygon + 3, top, topNext, bottomNext, color);
	}

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = YMGRE_Sqrt(radius * radius + h * h);
	object->BoundingBoxMin = (gre_fvector4d){ -radius, -h, -radius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  radius,  h,  radius, 1 };
	return object;
}

//沿 Y 轴生成封闭圆锥，顶点、底面圆心和圆周顶点分开存储
GRE_Object4d YMGRE_MeshGener_Cone(float32 radius, float32 height, uint16 segments,
	GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain((radius <= 0.0f) || (height <= 0.0f) || (segments < 3), GRE_LOG_ParamI,
		"圆锥半径、高度或分段数错误");
	GRE_Object4d object = YMGRE_Creat_Object(2 + segments, 2 * segments, name, materiaName);
	float32 halfHeight = height * 0.5f;
	object->pointList[0].pos = (gre_fvector4d){ 0, halfHeight, 0, 1 };//顶点
	object->pointList[1].pos = (gre_fvector4d){ 0, -halfHeight, 0, 1 };//底面圆心

	for (uint16 i = 0; i < segments; i++)
	{
		float32 angle = YMGRE_2Pai * i / segments;
		float32 x = radius * YMGRE_Cos(angle);
		float32 z = radius * YMGRE_Sin(angle);
		object->pointList[2 + i].pos = (gre_fvector4d){ x, -halfHeight, z, 1 };
	}

	for (uint16 i = 0; i < segments; i++)
	{
		uint16 next = (i + 1 == segments) ? 0 : i + 1;
		uint16 currentPoint = 2 + i;
		uint16 nextPoint = 2 + next;
		meshSetTriangle(object, 2 * i, 0, nextPoint, currentPoint, color);//侧面
		meshSetTriangle(object, 2 * i + 1, 1, currentPoint, nextPoint, color);//底面
	}

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = YMGRE_Sqrt(radius * radius + halfHeight * halfHeight);
	object->BoundingBoxMin = (gre_fvector4d){ -radius, -halfHeight, -radius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  radius,  halfHeight,  radius, 1 };
	return object;
}

//按纬线和经线生成球体，南北极单独使用一个顶点避免退化四边形
GRE_Object4d YMGRE_MeshGener_Sphere(float32 radius, uint16 latitude, uint16 longitude,
	GRErgb24 color, char* name, char* materiaName)
{
	gre_log_explain((radius <= 0.0f) || (latitude < 2) || (longitude < 3), GRE_LOG_ParamI,
		"球体半径、纬度或经度分段数错误");
	int pointNum = 2 + (latitude - 1) * longitude;
	int polygonNum = 2 * longitude * (latitude - 1);
	GRE_Object4d object = YMGRE_Creat_Object(pointNum, polygonNum, name, materiaName);
	object->pointList[0].pos = (gre_fvector4d){ 0, radius, 0, 1 };
	object->pointList[0].u = 0.0f; object->pointList[0].v = 0.0f;
	object->pointList[pointNum - 1].pos = (gre_fvector4d){ 0, -radius, 0, 1 };
	object->pointList[pointNum - 1].u = 0.0f; object->pointList[pointNum - 1].v = 1.0f;

	for (uint16 row = 0; row < latitude - 1; row++)
	{
		float32 phi = YMGRE_Pai * (row + 1) / latitude;
		for (uint16 col = 0; col < longitude; col++)
		{
			float32 theta = YMGRE_2Pai * col / longitude;
			int point = 1 + row * longitude + col;
			object->pointList[point].pos.x = radius * YMGRE_Sin(phi) * YMGRE_Cos(theta);
			object->pointList[point].pos.y = radius * YMGRE_Cos(phi);
			object->pointList[point].pos.z = radius * YMGRE_Sin(phi) * YMGRE_Sin(theta);
			object->pointList[point].pos.w = 1;
			object->pointList[point].u = (float32)col / longitude;
			object->pointList[point].v = (float32)(row + 1) / latitude;
		}
	}

	int polygon = 0;
	for (uint16 col = 0; col < longitude; col++)
	{
		uint16 next = (col + 1 == longitude) ? 0 : col + 1;
		meshSetTriangle(object, polygon++, 0, 1 + next, 1 + col, color);//北极
	}
	for (uint16 row = 0; row < latitude - 2; row++)
	{
		for (uint16 col = 0; col < longitude; col++)
		{
			uint16 next = (col + 1 == longitude) ? 0 : col + 1;
			uint16 top = 1 + row * longitude + col;
			uint16 topNext = 1 + row * longitude + next;
			uint16 bottom = top + longitude;
			uint16 bottomNext = topNext + longitude;
			meshSetTriangle(object, polygon++, top, topNext, bottomNext, color);
			meshSetTriangle(object, polygon++, top, bottomNext, bottom, color);
		}
	}
	uint16 lastRing = 1 + (latitude - 2) * longitude;
	for (uint16 col = 0; col < longitude; col++)
	{
		uint16 next = (col + 1 == longitude) ? 0 : col + 1;
		meshSetTriangle(object, polygon++, pointNum - 1, lastRing + col, lastRing + next, color);//南极
	}

	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = radius;
	object->BoundingBoxMin = (gre_fvector4d){ -radius, -radius, -radius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  radius,  radius,  radius, 1 };
	return object;
}

//在 XZ 平面绕 Y 轴生成圆环，主环和管截面首尾都闭合
GRE_Object4d YMGRE_MeshGener_Torus(float32 majorRadius, float32 tubeRadius,
	uint16 majorSegments, uint16 tubeSegments, GRErgb24 color,
	char* name, char* materiaName)
{
	gre_log_explain((majorRadius <= 0.0f) || (tubeRadius <= 0.0f) ||
		(majorRadius <= tubeRadius) || (majorSegments < 3) || (tubeSegments < 3),
		GRE_LOG_ParamI, "圆环半径或分段数错误");
	gre_log_explain((uint32)majorSegments > 65535u / tubeSegments,
		GRE_LOG_ParamI, "圆环顶点数超过索引范围");
	uint32 pointNumValue = (uint32)majorSegments * tubeSegments;
	uint32 polygonNumValue = 2u * pointNumValue;
	GRE_Object4d object = YMGRE_Creat_Object(pointNumValue, polygonNumValue, name, materiaName);

	for (uint16 major = 0; major < majorSegments; major++)
	{
		float32 majorAngle = YMGRE_2Pai * major / majorSegments;
		float32 majorCos = YMGRE_Cos(majorAngle);
		float32 majorSin = YMGRE_Sin(majorAngle);
		for (uint16 tube = 0; tube < tubeSegments; tube++)
		{
			float32 tubeAngle = YMGRE_2Pai * tube / tubeSegments;
			float32 ringRadius = majorRadius + tubeRadius * YMGRE_Cos(tubeAngle);
			int point = major * tubeSegments + tube;
			object->pointList[point].pos = (gre_fvector4d){
				ringRadius * majorCos,
				tubeRadius * YMGRE_Sin(tubeAngle),
				ringRadius * majorSin, 1 };
		}
	}

	int polygon = 0;
	for (uint16 major = 0; major < majorSegments; major++)
	{
		uint16 nextMajor = (major + 1 == majorSegments) ? 0 : major + 1;
		for (uint16 tube = 0; tube < tubeSegments; tube++)
		{
			uint16 nextTube = (tube + 1 == tubeSegments) ? 0 : tube + 1;
			uint16 current = major * tubeSegments + tube;
			uint16 nextV = major * tubeSegments + nextTube;
			uint16 nextU = nextMajor * tubeSegments + tube;
			uint16 diagonal = nextMajor * tubeSegments + nextTube;
			//圆环不是凸体，使用参数曲面的固定外向绕序
			meshSetTriangle(object, polygon++, current, nextV, nextU, color);
			meshSetTriangle(object, polygon++, nextU, nextV, diagonal, color);
		}
	}

	float32 outerRadius = majorRadius + tubeRadius;
	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = outerRadius;
	object->BoundingBoxMin = (gre_fvector4d){ -outerRadius, -tubeRadius, -outerRadius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  outerRadius,  tubeRadius,  outerRadius, 1 };
	return object;
}

//沿 Y 轴生成胶囊体，cylinderHeight 只表示两个半球之间的直筒高度
static GRE_Object4d meshGenerateCapsule(float32 radius, float32 cylinderHeight,
	uint16 hemisphereSegments, uint16 longitude, GRErgb24 color,
	char* name, char* materiaName, int legacy)
{
	gre_log_explain((radius <= 0.0f) || (cylinderHeight <= 0.0f) ||
		(hemisphereSegments == 0) || (longitude < 3), GRE_LOG_ParamI,
		"胶囊体尺寸或分段数错误");
	uint32 ringNum = 2u * hemisphereSegments;
	gre_log_explain(ringNum > 65533u / longitude,
		GRE_LOG_ParamI, "胶囊体顶点数超过索引范围");
	uint32 pointNumValue = 2u + ringNum * longitude;
	uint32 polygonNumValue = 2u * ringNum * longitude;
	GRE_Object4d object = YMGRE_Creat_Object(pointNumValue, polygonNumValue, name, materiaName);
	float32 halfHeight = cylinderHeight * 0.5f;
	object->pointList[0].pos = (gre_fvector4d){ 0, halfHeight + radius, 0, 1 };
	object->pointList[pointNumValue - 1].pos =
		(gre_fvector4d){ 0, -halfHeight - radius, 0, 1 };

	for (uint16 ring = 0; ring < hemisphereSegments; ring++)
	{
		float32 angle = YMGRE_Pai * (ring + 1) / (2.0f * hemisphereSegments);
		float32 ringRadius = radius * YMGRE_Sin(angle);
		float32 y = halfHeight + radius * YMGRE_Cos(angle);
		for (uint16 column = 0; column < longitude; column++)
		{
			float32 theta = YMGRE_2Pai * column / longitude;
			int point = 1 + ring * longitude + column;
			object->pointList[point].pos = (gre_fvector4d){
				ringRadius * YMGRE_Cos(theta), y,
				ringRadius * YMGRE_Sin(theta), 1 };
		}
	}
	for (uint16 ring = 0; ring < hemisphereSegments; ring++)
	{
		// Include the lower equator; the bottom pole already has its own vertex.
		float32 angle = YMGRE_Pai * (ring + (legacy ? 1 : 0)) / (2.0f * hemisphereSegments);
		float32 ringRadius = radius * YMGRE_Cos(angle);
		float32 y = -halfHeight - radius * YMGRE_Sin(angle);
		uint16 ringIndex = hemisphereSegments + ring;
		for (uint16 column = 0; column < longitude; column++)
		{
			float32 theta = YMGRE_2Pai * column / longitude;
			int point = 1 + ringIndex * longitude + column;
			object->pointList[point].pos = (gre_fvector4d){
				ringRadius * YMGRE_Cos(theta), y,
				ringRadius * YMGRE_Sin(theta), 1 };
		}
	}

	int polygon = 0;
	for (uint16 column = 0; column < longitude; column++)
	{
		uint16 next = (column + 1 == longitude) ? 0 : column + 1;
		meshSetTriangleOutward(object, polygon++, 0, 1 + column, 1 + next, color);
	}
	for (uint16 ring = 0; ring + 1 < ringNum; ring++)
	{
		for (uint16 column = 0; column < longitude; column++)
		{
			uint16 next = (column + 1 == longitude) ? 0 : column + 1;
			uint16 top = 1 + ring * longitude + column;
			uint16 topNext = 1 + ring * longitude + next;
			uint16 bottom = top + longitude;
			uint16 bottomNext = topNext + longitude;
			meshSetTriangleOutward(object, polygon++, top, topNext, bottom, color);
			meshSetTriangleOutward(object, polygon++, topNext, bottomNext, bottom, color);
		}
	}
	uint16 lastRing = 1 + (ringNum - 1) * longitude;
	uint16 bottomPole = pointNumValue - 1;
	for (uint16 column = 0; column < longitude; column++)
	{
		uint16 next = (column + 1 == longitude) ? 0 : column + 1;
		meshSetTriangleOutward(object, polygon++, bottomPole,
			lastRing + next, lastRing + column, color);
	}

	float32 boundRadius = halfHeight + radius;
	object->boundType = GRE_Bounding_Sphere_R;
	object->BoundingSphereR = boundRadius;
	object->BoundingBoxMin = (gre_fvector4d){ -radius, -boundRadius, -radius, 1 };
	object->BoundingBoxMax = (gre_fvector4d){  radius,  boundRadius,  radius, 1 };
	return object;
}

GRE_Object4d YMGRE_MeshGener_Capsule(float32 r,float32 h,uint16 a,uint16 b,GRErgb24 c,char* n,char* m)
{return meshGenerateCapsule(r,h,a,b,c,n,m,0);}
GRE_Object4d YMGRE_MeshGener_CapsuleLegacy(float32 r,float32 h,uint16 a,uint16 b,GRErgb24 c,char* n,char* m)
{return meshGenerateCapsule(r,h,a,b,c,n,m,1);}

//生成顶点位于指定外接球面的正四面体
GRE_Object4d YMGRE_MeshGener_Tetrahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName)
{
	gre_log_explain(radius <= 0.0f, GRE_LOG_ParamI, "正四面体半径必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(4, 4, name, materiaName);
	float32 value = radius / YMGRE_Sqrt(3.0f);
	object->pointList[0].pos = (gre_fvector4d){  value,  value,  value, 1 };
	object->pointList[1].pos = (gre_fvector4d){ -value, -value,  value, 1 };
	object->pointList[2].pos = (gre_fvector4d){ -value,  value, -value, 1 };
	object->pointList[3].pos = (gre_fvector4d){  value, -value, -value, 1 };
	meshSetTriangleOutward(object, 0, 0, 1, 2, color);
	meshSetTriangleOutward(object, 1, 0, 3, 1, color);
	meshSetTriangleOutward(object, 2, 0, 2, 3, color);
	meshSetTriangleOutward(object, 3, 1, 3, 2, color);
	meshSetSphereBounds(object, radius);
	return object;
}

//生成六个顶点位于坐标轴上的正八面体
GRE_Object4d YMGRE_MeshGener_Octahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName)
{
	gre_log_explain(radius <= 0.0f, GRE_LOG_ParamI, "正八面体半径必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(6, 8, name, materiaName);
	object->pointList[0].pos = (gre_fvector4d){  radius, 0, 0, 1 };
	object->pointList[1].pos = (gre_fvector4d){ -radius, 0, 0, 1 };
	object->pointList[2].pos = (gre_fvector4d){ 0,  radius, 0, 1 };
	object->pointList[3].pos = (gre_fvector4d){ 0, -radius, 0, 1 };
	object->pointList[4].pos = (gre_fvector4d){ 0, 0,  radius, 1 };
	object->pointList[5].pos = (gre_fvector4d){ 0, 0, -radius, 1 };
	uint16 face[8][3] = {
		{ 2, 0, 4 }, { 2, 4, 1 }, { 2, 1, 5 }, { 2, 5, 0 },
		{ 3, 4, 0 }, { 3, 1, 4 }, { 3, 5, 1 }, { 3, 0, 5 }
	};
	for (uint16 i = 0; i < 8; i++)
		meshSetTriangleOutward(object, i, face[i][0], face[i][1], face[i][2], color);
	meshSetSphereBounds(object, radius);
	return object;
}

static const uint16 meshIcosahedronFace[20][3] = {
	{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 },
	{ 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 }, { 10, 7, 6 }, { 7, 1, 8 },
	{ 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 },
	{ 4, 9, 5 }, { 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 }
};

//使用黄金比例生成正二十面体顶点，并缩放到指定外接球半径
static void meshSetIcosahedronPoint(GRE_Object4d object, float32 radius)
{
	float32 golden = (1.0f + YMGRE_Sqrt(5.0f)) * 0.5f;
	float32 scale = radius / YMGRE_Sqrt(1.0f + golden * golden);
	gre_fvector4d point[12] = {
		{ -1, golden, 0, 1 }, { 1, golden, 0, 1 },
		{ -1, -golden, 0, 1 }, { 1, -golden, 0, 1 },
		{ 0, -1, golden, 1 }, { 0, 1, golden, 1 },
		{ 0, -1, -golden, 1 }, { 0, 1, -golden, 1 },
		{ golden, 0, -1, 1 }, { golden, 0, 1, 1 },
		{ -golden, 0, -1, 1 }, { -golden, 0, 1, 1 }
	};
	for (uint16 i = 0; i < 12; i++)
	{
		object->pointList[i].pos = point[i];
		YMGRE_Fvector4d_ScaleTo(&object->pointList[i].pos, scale);
		object->pointList[i].pos.w = 1;
	}
}

//生成顶点位于指定外接球面的正二十面体
GRE_Object4d YMGRE_MeshGener_Icosahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName)
{
	gre_log_explain(radius <= 0.0f, GRE_LOG_ParamI, "正二十面体半径必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(12, 20, name, materiaName);
	meshSetIcosahedronPoint(object, radius);
	for (uint16 i = 0; i < 20; i++)
		meshSetTriangleOutward(object, i, meshIcosahedronFace[i][0],
			meshIcosahedronFace[i][1], meshIcosahedronFace[i][2], color);
	meshSetSphereBounds(object, radius);
	return object;
}

//判断正二十面体的两个面是否在指定顶点处相邻
static uint8 meshIcosahedronFaceAdjacent(uint16 faceA, uint16 faceB, uint16 vertex)
{
	uint8 common = 0;
	uint8 hasVertex = 0;
	for (uint16 i = 0; i < 3; i++)
	{
		if (meshIcosahedronFace[faceA][i] == vertex)
			hasVertex = 1;
		for (uint16 j = 0; j < 3; j++)
		{
			if (meshIcosahedronFace[faceA][i] == meshIcosahedronFace[faceB][j])
				common++;
		}
	}
	return hasVertex && (common == 2);
}

//使用正二十面体的对偶网格生成正十二面体，每个五边形面拆成三个三角形
GRE_Object4d YMGRE_MeshGener_Dodecahedron(float32 radius, GRErgb24 color,
	char* name, char* materiaName)
{
	gre_log_explain(radius <= 0.0f, GRE_LOG_ParamI, "正十二面体半径必须大于0");
	GRE_Object4d object = YMGRE_Creat_Object(20, 36, name, materiaName);
	gre_object4d icosahedron = { 0 };
	gre_vertex4d icosahedronPoint[12] = { 0 };
	icosahedron.pointList = icosahedronPoint;
	meshSetIcosahedronPoint(&icosahedron, 1.0f);

	//对偶网格的每个顶点对应正二十面体的一个三角面中心
	for (uint16 face = 0; face < 20; face++)
	{
		gre_fvector4d center = { 0 };
		for (uint16 i = 0; i < 3; i++)
		{
			GRE_Fvector4d point = &icosahedronPoint[meshIcosahedronFace[face][i]].pos;
			center.x += point->x;
			center.y += point->y;
			center.z += point->z;
		}
		float32 scale = radius / YMGRE_Sqrt(center.x * center.x +
			center.y * center.y + center.z * center.z);
		center.x *= scale;
		center.y *= scale;
		center.z *= scale;
		center.w = 1;
		object->pointList[face].pos = center;
	}

	int polygon = 0;
	for (uint16 vertex = 0; vertex < 12; vertex++)
	{
		uint16 adjacent[5] = { 0 };
		uint16 ordered[5] = { 0 };
		uint8 used[5] = { 0 };
		uint16 adjacentNum = 0;
		//正二十面体每个顶点相邻五个三角面，对应正十二面体的一个五边形面
		for (uint16 face = 0; face < 20; face++)
		{
			for (uint16 i = 0; i < 3; i++)
			{
				if (meshIcosahedronFace[face][i] == vertex)
				{
					adjacent[adjacentNum++] = face;
					break;
				}
			}
		}
		gre_log_explain(adjacentNum != 5, GRE_LOG_Mem1, "正十二面体对偶拓扑错误");
		//按共享边关系将五个对偶顶点排成闭环，避免交叉连接五边形
		ordered[0] = adjacent[0];
		used[0] = 1;
		for (uint16 order = 1; order < 5; order++)
		{
			for (uint16 candidate = 0; candidate < 5; candidate++)
			{
				if (!used[candidate] && meshIcosahedronFaceAdjacent(
					ordered[order - 1], adjacent[candidate], vertex))
				{
					ordered[order] = adjacent[candidate];
					used[candidate] = 1;
					break;
				}
			}
		}
		//保持顶点数不变，将有序五边形从第一个顶点扇形拆成三个三角形
		for (uint16 triangle = 1; triangle + 1 < 5; triangle++)
			meshSetTriangleOutward(object, polygon++, ordered[0],
				ordered[triangle], ordered[triangle + 1], color);
	}

	meshSetSphereBounds(object, radius);
	return object;
}
