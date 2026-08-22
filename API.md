# YMGRE API 记录

本文优先记录新引入的跨平台渲染资源 API。旧 API 继续保留，后续按模块逐步补齐。

## RenderTarget

```c
GRE_RenderTarget YMGRE_Creat_RenderTarget(uint16 width, uint16 height);
void YMGRE_RenderTarget_Init(GRE_RenderTarget target, uint16 width, uint16 height,
    GRE_FrameBuffer color, float32* depth);
void YMGRE_Free_RenderTarget(GRE_RenderTarget target);
```

- `Creat` 创建的 Target 拥有颜色和深度缓存，必须用 `Free` 释放。
- `Init` 绑定调用者提供的缓存，YMGRE 不拥有缓存，不得调用 `YMGRE_Free_RenderTarget`。
- PC/YMGUI 日常 Target 使用 RGB565，即 `2 + 4 = 6` 字节/像素；其他颜色深度只作为库源码兼容分支，不纳入日常构建。

## RenderWorkspace

```c
GRE_RenderWorkspace YMGRE_Creat_RenderWorkspace(void);
void YMGRE_RenderWorkspace_Reserve(GRE_RenderWorkspace workspace,
    uint32 pointNum, uint32 polygonNum, uint32 lightNum);
void YMGRE_RenderWorkspace_Init(GRE_RenderWorkspace workspace,
    GRE_Vertex4d points, uint32 pointMax,
    uint8* polygonHide, GRErgb24* polygonColor, uint32 polygonMax,
    gre_fvector4d* lightPos, uint32 lightMax);
void YMGRE_Free_RenderWorkspace(GRE_RenderWorkspace workspace);
```

- 动态 Workspace 会在容量不足时扩容，正常渲染前应主动 Reserve 到最大容量。
- 外部 Workspace 不扩容，容量不足按参数错误处理。
- 同一 Workspace 同一时刻只能被一条渲染任务使用。
- `Init` 的外部缓存由调用者负责生命周期。

## Camera

```c
GRE_Camera4d YMGRE_Creat_Camera(...);
GRE_Camera4d YMGRE_Creat_CameraFromTarget(int16 id, GRE_RenderTarget target, ...);
void YMGRE_Camera_BindRenderTarget(GRE_Camera4d camera, GRE_RenderTarget target);
void YMGRE_Camera_BindRenderWorkspace(GRE_Camera4d camera, GRE_RenderWorkspace workspace);
GRE_RenderTarget YMGRE_Camera_GetRenderTarget(GRE_Camera4d camera);
```

- `YMGRE_Creat_Camera` 保持原行为，相机拥有私有 `img` Target。
- `YMGRE_Creat_CameraFromTarget` 不申请 framebuffer，Target 始终由调用者管理。
- 普通相机后续绑定其他 Target 时，原私有 `img` 仍由相机持有并在析构时释放。
- `camera->wireFrame` 保留旧字段名，取值使用 `GRE_Render_Solid`、`GRE_Render_Wireframe` 或 `GRE_Render_SolidWire`；旧代码中的 `0/1` 语义不变。

## Context 管线

```c
void YMGRE_Camera_TanglePipline_RenderingWithWorkspace(
    GRE_Camera4d camera, GRE_List lights, GRE_List objects,
    GRE_List materials, GRE_RenderWorkspace workspace);

void YMGRE_Camera_PolygonPipline_RenderingWithWorkspace(
    GRE_Camera4d camera, GRE_List lights, GRE_List objects,
    GRE_List materials, GRE_RenderWorkspace workspace);
```

传入 `workspace == NULL` 时使用相机绑定的 Workspace。两者都为空属于参数错误。

顺序多相机推荐：

```c
GRE_RenderWorkspace workspace = YMGRE_Creat_RenderWorkspace();
YMGRE_Camera_TanglePipline_RenderingWithWorkspace(cam0, lights, objects, materials, workspace);
YMGRE_Camera_TanglePipline_RenderingWithWorkspace(cam1, lights, objects, materials, workspace);
YMGRE_Free_RenderWorkspace(workspace);
```

## 基础网格

```c
GRE_Object4d YMGRE_MeshGener_RectPlane(float32 width, float32 depth,
    uint16 rows, uint16 columns, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Cube(float32 side, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Box(float32 width, float32 height,
    float32 depth, GRErgb24 color, char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Cylinder(float32 radius, float32 height,
    uint16 segments, GRErgb24 color, char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Cone(float32 radius, float32 height,
    uint16 segments, GRErgb24 color, char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Sphere(float32 radius, uint16 latitude,
    uint16 longitude, GRErgb24 color, char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Torus(float32 majorRadius, float32 tubeRadius,
    uint16 majorSegments, uint16 tubeSegments, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Capsule(float32 radius, float32 cylinderHeight,
    uint16 hemisphereSegments, uint16 longitude, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Tetrahedron(float32 radius, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Octahedron(float32 radius, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Dodecahedron(float32 radius, GRErgb24 color,
    char* name, char* materialName);
GRE_Object4d YMGRE_MeshGener_Icosahedron(float32 radius, GRErgb24 color,
    char* name, char* materialName);
```

- 所有生成器都返回只包含三角形的 Object，可以直接进入三角形 Context 管线；
- 平面 `rows >= 1`、`columns >= 1`，圆柱和圆锥 `segments >= 3`，球体 `latitude >= 2`、`longitude >= 3`；
- 圆环要求主半径大于管半径且两组分段数均不小于三；胶囊体要求圆柱段高度大于零、半球分段不小于一、经度分段不小于三；
- 网格以原点为中心，封闭体法线朝外，矩形平面法线朝 `+Y`，并设置 AABB 与包围球；
- `GRE_Object4d::wireFrame` 控制该模型是否叠加三角网格线；关闭时三角形入口只执行实体填充，开启时先填充该模型全部可见片元，再复用当前顶点和 Z-buffer 统一后画线框；
- 线框与填充深度相同时由后画线框覆盖；线框仍执行深度测试，不保证绘制被其他表面遮挡的边；
- 三角剖分产生的内部共享边两侧都存在填充，这是实体叠加网格线的预期显示；
- 返回对象由调用者使用 `YMGRE_Free_Object` 释放。

## 基础光栅化

```c
void YMGRE_Img_SetBrushColor(GRErgb24 color);
void YMGRE_Img_Line(GRE_FrameBuffer data, uint16 width, uint16 height,
    int16 x1, int16 y1, int16 x2, int16 y2);
void YMGRE_Img_Scanline_AreaFill(GRE_FrameBuffer data, uint16 width,
    uint16 height, GRE_LinesList ring, GRE_Fvector4d plane,
    float32* zbuff, GRErgb24 fillColor);
```

- `YMGRE_Img_Line` 不执行窗口裁剪，调用前必须保证两个端点位于 framebuffer 内；
- `YMGRE_Img_Scanline_AreaFill` 接收已经裁剪到图像范围内的闭合边集合；
- 自定义窗口流程为 `YMGRE_Polygon_clip2D` 裁剪边集合，再调用扫描线填充；
- 画笔颜色是兼容旧接口的全局绘线状态，修改后会影响后续线框和直线绘制。

## 多边形三角剖分

```c
uint16 YMGRE_Polygon_Triangulate(const gre_fvector4d* vertices,
    uint16 vertexNum, uint16* triangleIndices, uint16 triangleCapacity);
```

- 使用耳切法处理简单的二维多边形，顶点坐标取 `x/y` 分量；支持顺时针和逆时针绕序。
- `triangleIndices` 按三个顶点索引连续输出，最多生成 `vertexNum - 2` 个三角形。
- 不支持自交多边形、带孔多边形；输入退化或容量不足时返回 `0`。
- 这是边界约束的多边形剖分，不是无边界点集的 Delaunay 网格化。
