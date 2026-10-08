#ifndef YMGRE_RENDERING_PIPELINE_H
#define YMGRE_RENDERING_PIPELINE_H
#include "YMGRE_Coordinates_Transform.h"
#include "YMGRE_RenderContext.h"
#include"./YMGRE_List.h"

/* Immutable compact geometry for opaque uniformly scaled mesh instances. */
typedef struct {gre_vertex4d base;gre_fvector4d normal;} GRE_CompactVertex;
typedef struct {gre_fvector4d min,max;} GRE_MeshBounds;
typedef struct {float32 x,y,z;} GRE_MeshFaceCenter;
typedef struct {GRE_MeshBounds bounds;uint32 firstFace,faceCount;} GRE_MeshCluster;
typedef struct {
 const GRE_CompactVertex *vertices;const GRE_Index *indices;
 const gre_fvector4d *faceNormals;uint32 vertexCount,triangleCount;
 uint8 triangleList; /* Three consecutive vertices per face; indices may be NULL. */
 uint8 facePlaneOffsetsInW; /* Optional: normal.w = -dot(normal.xyz, first vertex.xyz). */
 const GRE_MeshBounds *bounds; /* Optional conservative local AABB, immutable with the mesh. */
 const GRE_MeshCluster *clusters;uint32 clusterCount; /* Optional consecutive face ranges, immutable. */
 const gre_fvector4d *positions;const GRE_Index *vertexPositionIndices;uint32 positionCount; /* Exact shared positions; attributes stay per vertex. */
 const GRE_MeshFaceCenter *faceCenters; /* Optional immutable local face centroids. */
 uint8 indicesValidated; /* Caller may set only after validating immutable indices. */
} GRE_CompactMesh;
typedef struct {
 uint32 tag;gre_vertex4d_wN vertex;gre_fvector4d projected;uint8 code,lit;
} GRE_InstanceVertexCacheEntry;
#ifndef YMGRE_INSTANCE_CACHE_SIZE
#define YMGRE_INSTANCE_CACHE_SIZE 256
#endif
#if (YMGRE_INSTANCE_CACHE_SIZE & (YMGRE_INSTANCE_CACHE_SIZE-1)) || YMGRE_INSTANCE_CACHE_SIZE < 1
#error YMGRE_INSTANCE_CACHE_SIZE must be a positive power of two
#endif
typedef struct {gre_fvector4d camera;float32 screenX,screenY,u,v;GRErgb24 lighting,specular;uint8 code,lit;} GRE_InstanceFullVertex;
typedef struct {GRE_InstanceVertexCacheEntry entries[YMGRE_INSTANCE_CACHE_SIZE];
 GRE_InstanceFullVertex *fullVertices;uint32 fullCapacity; /* Optional caller-owned complete indexed-mesh cache. */
 GRE_InstanceFullVertex *epochStorage;uint32 epochCapacity;uint8 fullEpoch; /* optional lazy state reset metadata */
 void *meshScratch;size_t meshScratchBytes; /* Optional borrowed staging arena, default NULL/0. */
} GRE_InstanceVertexCache;
/**
 * @brief 严格按完整顶点字节去重，保留每个面的属性和顺序。
 * @param vertices 调用者持有的输出数组，capacity 至少为原顶点数。
 * @param indices 输出容量至少为 triangleCount * 3；不得与输入索引重叠。
 * @param vertexRemap 调用者的临时映射数组，至少为原顶点数。
 * @param hashTable 临时哈希数组，容量须为二次幂且至少两倍顶点数。
 * @return 1 成功并写出 vertexCount；0 输入或容量无效，输出可能部分写入。
 */
int YMGRE_CompactMesh_PrepareIndexed(const GRE_CompactMesh *mesh,GRE_CompactVertex *vertices,uint32 capacity,GRE_Index *indices,GRE_Index *vertexRemap,uint32 *hashTable,uint32 hashCapacity,uint32 *vertexCount);
/** @brief 按位置浮点字节去重，UV 和法线仍按原顶点独立。
 * @param positions 调用者的输出空间，capacity 至少为原顶点数。
 * @param positionIndices 输出映射空间，至少为原顶点数。
 * @param hashTable 临时空间，二次幂容量至少两倍原顶点数。
 * @return 1 成功，0 输入或容量无效；所有数组不得与输入重叠。
 */
int YMGRE_CompactMesh_PrepareSharedPositions(const GRE_CompactMesh *mesh,gre_fvector4d *positions,uint32 capacity,GRE_Index *positionIndices,uint32 *hashTable,uint32 hashCapacity,uint32 *positionCount);
/** @brief 按原面序生成保守簇包围盒，不修改网格。
 * @param clusters 调用者拥有的输出，capacity 至少为向上取整的面数/簇面数。
 * @return 1 成功，0 输入或容量无效；输出可能部分写入。
 */
int YMGRE_CompactMesh_PrepareClusters(const GRE_CompactMesh *mesh,uint32 facesPerCluster,GRE_MeshCluster *clusters,uint32 capacity);
/** @brief 从有限局部顶点计算保守 AABB，bounds 由调用者持有。
 * @return 1 成功，0 几何无效；网格改变后须重新准备。
 */
int YMGRE_CompactMesh_PrepareBounds(const GRE_CompactMesh *mesh,GRE_MeshBounds *bounds);
/** @brief 完整检查不可变网格的索引范围和三角形计数。
 * @return 1 合法；0 无效。不自动设置 indicesValidated，修改网格后须重新验证。
 */
int YMGRE_CompactMesh_ValidateIndices(const GRE_CompactMesh *mesh);
/** @brief 为每个面准备局部中心，供可选不透明深度分桶使用。
 * @param centers 调用者拥有的输出，capacity 至少为面数。
 * @return 1 成功，0 输入或容量无效；输出可能部分写入。
 */
int YMGRE_CompactMesh_PrepareFaceCenters(const GRE_CompactMesh *mesh,GRE_MeshFaceCenter *centers,uint32 capacity);
/** @brief 初始化可复用实例缓存，不申请内存。
 * @param scratch 借用暂存区，至少四字节对齐；可为 NULL/0，调用者负责释放。
 * @param cache 调用者持有，初始化后可绑定 fullVertices/fullCapacity，仍由调用者释放。
 */
void YMGRE_InstanceVertexCache_Init(GRE_InstanceVertexCache *cache,void *scratch,size_t bytes);
/** @brief 向已初始化目标追加正均匀缩放的不透明单面顶点光照实例。
 * @param mesh 不可变借用几何，不由库释放；顶点颜色隐含为白色。
 * @param localToWorld 旋转、平移和正均匀缩放的组合矩阵。
 * @param uniformScale 必须与 localToWorld 三个轴的长度一致；不支持反射或剪切。
 * @param workspace 须预留灯光容量；cache 和完整缓存均由调用者持有。
 * @return 1 绘制或完全不可见；0 模式、输入或容量不支持，应走普通管线。
 * @note 不清屏、不在帧内申请内存；同一缓存和工作区不可并发使用。
 */
int YMGRE_Camera_AppendMeshInstance_wN(GRE_Camera4d camera,GRE_List lights,
 GRE_Material material,const GRE_CompactMesh *mesh,GRE_FMat4x4 localToWorld,
 float32 uniformScale,GRE_InstanceVertexCache *cache,GRE_RenderWorkspace workspace);


GRE_Material YMGRE_Material_Find(GRE_List MaterialList, char* materialName);

void YMGRE_Camera_PolygonPipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList);//多边形物体
void YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList,
	GRE_List MaterialList, GRE_RenderWorkspace workspace);//使用共享或独立工作区渲染多边形物体
void YMGRE_Camera_TanglePipline_Rendering(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList);//三角形物体
void YMGRE_Camera_TanglePipline_RenderingWithWorkspace(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList,
	GRE_List MaterialList, GRE_RenderWorkspace workspace);//使用共享或独立工作区渲染三角形物体
void YMGRE_Camera_TanglePipline_wN(GRE_Camera4d thiscam, GRE_List LightList, GRE_List ObjList, GRE_List MaterialList,
	GRE_RenderWorkspace workspace);//独立高级顶点流程
void YMGRE_Camera_TanglePipline_VertexColor_wN(GRE_Camera4d thiscam,
	GRE_List ObjList,GRE_RenderWorkspace workspace);//独立透视校正顶点色流程
/** @brief 往同一已初始化目标追加不透明批次，保留颜色和深度。
 * @param workspace 调用者持有，按最大物体/灯光数量预留以避免帧内扩容。
 * @return 1 成功；0 输入、材质容量不足、透明度贴图或启用的线性色彩。
 */
int YMGRE_Camera_AppendOpaqueBatch_wN(GRE_Camera4d camera,GRE_List lights,
 GRE_List objects,GRE_List materials,GRE_RenderWorkspace workspace);
void YMGRE_Camera_LineList_Rendering(GRE_Camera4d camera, const gre_line3d* lines,
	uint32 lineNum, uint8 depthTest);//渲染独立3D线段，不清除目标

#endif // !YMGRE_RENDERING_PIPELINE_H

