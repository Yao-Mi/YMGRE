# YMGRE 架构记录

## 分层

```text
CONFIG    平台类型、颜色深度、内存后端
DEBUG     断言和错误报告
OPOBJ     Camera/Object/Material 等资源生命周期
IOFILE    模型、场景和图片加载
CORE      数学、变换、剔除、光照、光栅化、渲染上下文
Demo      单功能演示，以及 YMGUI/SDL 显示宿主
tests     自动化模块测试
project_Demo 真实应用场景验证
```

YMGRE 库不依赖 YMGUI 或 SDL，只产生配置指定格式的 framebuffer。PC 高级默认使用 RGB888，
MCU 基础构建使用 RGB565。`Demo/demo_host` 负责把
RenderTarget 绑定到 YMGUI Image，桌面 Demo 再通过 SDL 显示。

Demo 只组合少量 CORE API，展示静态或有限步骤结果。ProjectDemo 可以组合 IOFILE、
SceneManager、DemoHost 和输入事件，负责持续变化的应用状态。测试模块只做自动判定，
不依赖人工观察窗口。

## 多相机资源模型

相机资源拆成三部分：

```text
Camera
  位置、目标、镜头、视景体、世界到相机矩阵
  -> RenderTarget
  -> RenderWorkspace

RenderTarget
  width / height
  color framebuffer
  depth buffer

RenderWorkspace
  当前物体的变换顶点
  当前物体的多边形隐藏状态
  当前物体的多边形光照颜色
  当前相机的灯光位置
```

Workspace 只需容纳场景中最大的单个物体，因为管线完成一个物体的光栅化后才处理下一个物体。

Camera framebuffer 所有权由 `ownsImageBuffers` 显式记录：

- `YMGRE_Creat_Camera` 创建 Camera 和内部颜色/深度缓存，`YMGRE_Free_Camera` 配对释放三者；
- `YMGRE_Creat_CameraFromTarget` 只创建 Camera，引用调用者提供的 Target，释放 Camera 时不释放 Target；
- `YMGRE_Camera_BindRenderTarget` 只切换当前输出位置，不转移任何缓存所有权。

## 共享策略

| 场景 | Target | Workspace |
|---|---|---|
| PC 多视口、顺序渲染 | 每相机独立 | 所有相机共享 |
| PC 多线程渲染 | 每相机独立 | 每相机独立 |
| MCU 相机切换 | 可以共享 | 可以共享 |
| MCU 同屏多视口 | 独立或屏幕子区域 | 顺序渲染时共享 |

独立 Target 让 YMGUI 的多个 `Image` 控件可以同时引用各自 framebuffer。共享 Target 只适用于不需要同时保留多幅画面的场景。

## 新旧管线

旧管线把以下相机相关状态写在共享对象中：

- `pointList_`
- `object.isDelete`
- `polygon.ishide`
- `polygon.planeColor_`
- `light.proper.pos_`

它保留用于 API 兼容，只适合顺序渲染。`RenderingWithWorkspace` 管线不修改这些字段，支持共享或独立 Workspace。

## 基础与高级顶点管线

MCU 和低内存配置固定使用紧凑的 `gre_vertex4d`（位置和 UV）基础管线。高级管线面向 PC
或内存充足的平台，使用 `gre_vertex4d_wN`，附加顶点法线、切线、切线手性和 `GRErgb24`
顶点颜色。两套管线由不同函数集合组成，不在物体循环或片元热路径中按模式分支：

```text
基础：YMGRE_Camera_TanglePipline_RenderingWithWorkspace
高级顶点色：YMGRE_Camera_TanglePipline_VertexColor_wN
高级材质光照：YMGRE_Camera_TanglePipline_wN
```

高级顶点色流程不创建或访问材质和灯光。高级材质光照流程要求传入材质列表和灯光列表，
不使用 `lights == NULL` 在片元循环中切换渲染模式。MCU 不要求编译或运行这些高级入口。

`YMGRE_Object_GenerateVertexAttributes` 按需分配并从三角网格和 UV 生成高级属性。动态
Workspace 由高级管线按需扩容；固定 Workspace 必须调用
`YMGRE_RenderWorkspace_BindVertexAttributes` 绑定调用者持有的 `gre_vertex4d_wN` 缓冲。

`gre_vertex4d_wN.base` 位于结构首成员，单个顶点可以转换成 `GRE_Vertex4d`。扩展顶点数组
不能整体转换为基础顶点数组后索引，因为两种元素的步长不同。
Vertex 模式在该 PC 高级结构中分别缓存 `vertexLighting` 和 `vertexSpecular`：前者受颜色
纹理与顶点色调制，后者以独立 RGB888 三字节缓存并在最后相加。两个属性都必须通过
相机变换和视锥裁剪传播；MCU 基础 `gre_vertex4d` 不包含这些缓存。

高级三角形光栅器对 UV、颜色、法线和切线做透视校正插值，并执行逐片元光照。材质脚本
可用独立字段加载切线空间法线贴图：

```text
texture diffuse.bmp
normal_map normal.bmp
specular_power 30
```

没有 `normal_map` 时直接使用插值顶点法线；高级材质和高级顶点缓存均为独立分配，不增加
基础 MCU 管线的逐顶点内存。PC 高级流程使用 RGB888（`YMGRE_CAMERA_COLOR_DEPTH=24`），
MCU 基础流程使用 RGB565（`YMGRE_CAMERA_COLOR_DEPTH=16`）。

## 光线追踪边界

光线追踪是与上述光栅流程并列的新渲染管线，不复用三角形投影、光栅化和片元插值入口：

```text
光栅：顶点 -> 变换/裁剪 -> 三角形覆盖 -> 插值 -> 光照 -> framebuffer
光追：像素 -> 相机射线 -> 最近交点 -> 阴影/反射/折射 -> framebuffer
```

当前只实现确定性的 Whitted-style 光线追踪，目标为 PC/RGB888。它可以复用场景网格、相机、
材质资源和数学库，但使用独立的 `GRE_Ray`、交点记录、场景查询和递归工作区。首阶段覆盖
三角形求交、硬阴影、镜面反射、Snell 折射、Fresnel 和全反射。路径追踪不属于当前范围，
因此不添加随机采样、累积 framebuffer、降噪器或面光源概率采样；未来若需要，必须作为另一套
明确选择的渲染流程加入。

## YMGUI 接入

一个相机 Target 对应一个长期存活的 `GYimg` 和一个 `Image` 控件。`GYimg` 不拥有相机
framebuffer，更新一帧后只需 invalidate 对应控件。YMGUI 的 display、flush、input、
多相机数量和布局都属于 Demo 或 ProjectDemo；YMGRE 不包含显示端口。

`SceneManager` 通过 `gre_scene_host` 接收单帧推进、按键、指针和画面提交回调，不能直接
调用 YMGUI、SDL 或具体 LCD API。`scene_roaming` 是当前宿主回调的参考实现。

SceneManager 使用显式 `YMGRE_Scene_Init` / `YMGRE_Scene_Destroy` 生命周期。资源根目录、
相机交互状态和场景资源均属于场景实例；资源根目录由应用在运行时传入，不使用编译期绝对
路径。旧 worldmap/LCD 交互实验只保存在 `YMGRE/legacy/`，不进入任何默认构建目标。

## 三角形实体叠加线框

模型的 `wireFrame` 标志在物体层决定是否叠加三角网格，三角形填充热路径不执行逐片元的线框模式判断。实体叠线按以下顺序完成：

```text
遍历可见三角形并写入颜色/Z-buffer
  -> 全部片元填充结束
  -> 遍历可见三角形边
  -> 单像素线段进行深度测试并覆盖同深度填充
```

后画线框可以防止共享边被同一物体的后续片元覆盖。线框仍读取并更新当前 Target 的 Z-buffer，因此被其他可见表面遮挡的边不会直接穿透前景。

三角网格线和普通多边形轮廓采用独立的内部光栅化函数。三角网格线使用方向无关的对称 Bresenham，并在整数像素坐标计算平面深度；普通多边形轮廓使用与多边形扫描填充一致的像素中心覆盖规则。两类图元不能共用一套边界取整规则，否则会在普通多边形边缘产生空点，或让三角网格出现方向相关的端点和宽度差异。

三角化模型的内部共享边属于网格的一部分，边两侧都由片元填充。只有投影后的模型外轮廓要求线外为背景，调试截图时不能把内部三角剖分线误判为填充越界。

## 独立 3D 线段

`gre_line3d` 是不依附于 Polygon 的双端点世界空间图元。调用
`YMGRE_Camera_LineList_Rendering(camera, lines, count, depthTest)` 会把线列表叠加到 Camera
当前 Target，函数本身不清屏。典型顺序为：

```text
三角形/多边形场景管线（清屏并渲染实体）
  -> 独立 3D 线列表（复用当前颜色和深度缓存）
  -> 提交 RenderTarget
```

线段依次执行世界到相机变换、六个视景体平面裁剪、透视投影和窗口变换，光栅化阶段使用
`1/z` 插值进行可选深度测试。它不执行三角形背面剔除，适合场景网格、坐标轴、包围盒和
变换 Gizmo；模型三角边仍使用上一节的线框路径，两者语义不同。
