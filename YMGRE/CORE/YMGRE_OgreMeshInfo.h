#ifndef YMGRE_ORGEMSEHINFO_H
#define YMGRE_ORGEMSEHINFO_H

// Ogre Mesh文件相关信息
// Ogre Mesh二进制文件中的ID枚举, 来源--Ogre
typedef enum 
{
	//unsigned short --- uint32
	OGRE_HEADER = 0x1000,
	// char*          version   : Version number check
	OGRE_MESH = 0x3000,//版本信息
	// bool skeletallyAnimated   // important flag which affects h/w buffer policies
	OGRE_SUBMESH = 0x4000,//子网格信息
	// char* materialName
	// bool useSharedVertices
	// unsigned int indexCount
	// bool indexes32Bit
	// unsigned int* faceVertexIndices (indexCount)
	// OR
	// unsigned short* faceVertexIndices (indexCount)
	// OGRE_GEOMETRY chunk (Optional: present only if useSharedVertices = false)
	 //OGRE_GEOMETRY块 变量useSharedVertices=false时才会出现
	//这个块是被嵌入在网格和子网格中
	OGRE_GEOMETRY = 0x5000, // NB this chunk is embedded within M_MESH and M_SUBMESH
	// unsigned int vertexCount
	OGRE_GEOMETRY_VERTEX_DECLARATION = 0x5100,
	OGRE_GEOMETRY_VERTEX_ELEMENT = 0x5110, // Repeating section
	// unsigned short source;  	// buffer bind source
	// unsigned short type;    	// VertexElementType
	// unsigned short semantic; // VertexElementSemantic
	// unsigned short offset;	// start offset in buffer in bytes
	// unsigned short index;	// index of the semantic (for colours and texture coords)
	OGRE_GEOMETRY_VERTEX_BUFFER = 0x5200, // Repeating section 重复数据
	// unsigned short bindIndex;	// Index to bind this buffer to
	// unsigned short vertexSize;	// Per-vertex size, must agree with declaration at this index
	OGRE_GEOMETRY_VERTEX_BUFFER_DATA = 0x5210,
	// raw buffer data
	// float* pVertices (x, y, z order x numVertices)
	// float* pNormals (x, y, z order x numVertices)
	// float* pTexCoords  (u v dimensions x numVertices)

	OGRE_SUBMESH_OPERATION = 0x4010, // optional, trilist assumed if missing 可选项
	// unsigned short operationType

	OGRE_MESH_BOUNDS = 0x9000,//包围盒
	// float minx, miny, minz
	// float maxx, maxy, maxz
	// float radius

	OGRE_SUBMESH_NAME_TABLE = 0xA000,//子块名字列表，每个子块包含索引和字符串
	// Subchunks of the name table. Each chunk contains an index & string
	OGRE_SUBMESH_NAME_TABLE_ELEMENT = 0xA100,
	// short index
	// char* name
}OGRE_MeshID;

/// The rendering operation type to perform
typedef enum  {
	/// A list of points, 1 vertex per point
	OGRE_POINT_LIST = 1,//点列表
	/// A list of lines, 2 vertices per line
	OGRE_LINE_LIST = 2,//线列表：一条线两个顶点
	/// A strip of connected lines, 1 vertex per line plus 1 start vertex
	OGRE_LINE_STRIP = 3,//带状连接线
	/// A list of triangles, 3 vertices per triangle
	OGRE_TRIANGLE_LIST = 4,//三角形列表
	/// A strip of triangles, 3 vertices for the first triangle, and 1 per triangle after that 
	OGRE_TRIANGLE_STRIP = 5,//条带三角形
	/// A fan of triangles, 3 vertices for the first triangle, and 1 per triangle after that
	OGRE_TRIANGLE_FAN = 6 //三角形扇
}ORGE_OType;

#endif // !YMGRE_ORGEMSEHINFO_H



