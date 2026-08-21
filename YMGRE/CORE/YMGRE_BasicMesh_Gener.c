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
