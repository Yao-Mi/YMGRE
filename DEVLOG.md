# YMGRE 开发记录

## 2026-08-22：基础 Demo 补全与多边形三角剖分

- 完善 `demo_lighting`：点光源漫反射和高光统一乘光源强度；聚光灯补充 Blinn-Phong 高光，并叠加距离、锥角和入射角衰减；聚光灯线框改为与内锥角联动的圆锥体；修正工作区光照计算错误使用 `tpoints[0]` 导致大模型光源标记与高光位置错位的问题。
- 新增 `demo_raster_primitives`，集中验证直线、扫描线填充和二维多边形裁剪后的底层绘制。
- 新增独立的 `YMGRE_PolygonTriangulation.h/.c` 模块，使用耳切法将简单二维多边形转换为三角形索引；支持顺/逆时针绕序，不支持自交和带孔多边形。
- 新增 `demo_polygon_triangulation`，对照凹多边形直接填充与耳切三角网格填充，并显示内部共享边。
- 更新 `API.md`、`Demo/README.md` 和 `PROJECT.md`，当前根构建包含 18 个基础 Demo。

### 本轮验证

- 全部 18 个 Demo 编译通过；新增 Demo 均支持 `YMGRE_MAX_FRAMES=1` 无头运行；
- CTest `11/11` 全部通过；
- `git diff --check` 通过。

## 2026-08-21：基础光栅化与线框叠加收敛

- 三角形快速填充从 `YMGRE_Rasterization.c` 拆到独立的 `YMGRE_TriangleRaster.c/.h`，保留原有上下半三角扫描结构和中文注释；
- 模型级 `wireFrame` 使用两遍渲染：第一遍填充全部可见三角片元，第二遍统一后画三角网格，避免后续片元覆盖先画共享边；
- 线框仍参与 Z-buffer 测试，相同深度允许后画线框覆盖填充；深度容差按屏幕平面斜率补偿像素取整误差，避免用固定大偏移造成隐藏边穿透；
- 三角网格使用独立的对称 Bresenham 单像素线段，并与三角形填充统一在整数像素坐标计算深度；普通多边形轮廓继续使用与扫描填充配套的像素中心采样，两条路径互不改变覆盖规则；
- 三角网格内部共享边两侧都存在填充是正常结果，不表示填充越过模型外轮廓；模型真正的外轮廓外侧应保持背景色；
- 清理了扫描线端点直接着线框颜色等试验方案，底层填充热路径不包含 `drawWire` 判断。

### 本轮验证

- 12 个基础 Demo 已统一在根 `build/` 重新编译；
- CTest `11/11` 通过；
- 三角片元共享边压力测试覆盖无裂缝、线框后画保留和单像素宽度；
- 普通多边形测试继续覆盖三角形、四边形、五边形填充边界无空点；
- `git diff --check` 通过，临时截图和诊断输出未写入项目目录。
- 按项目原有中文注释风格补齐 RenderTarget/Workspace 所有权、Context 管线阶段、基础网格几何约定、对偶拓扑和外部线框路径说明；不为普通赋值和简单循环增加复述式注释。

## 2026-08-21：YMGUI 接入与跨平台渲染上下文

对比基线：`f3ffdc6 Integrate YMGUI demo and stabilize YMGRE runtime`

### 本轮约定

- YMGRE 默认相机 framebuffer 使用 RGB565，保留其他颜色深度源码兼容能力。
- YMGUI 只作为显示与输入工具，YMGRE 的 Demo、测试、文档和构建产物不放入 YMGUI。
- 构建产物统一放在根 `build/`。
- 新增文件使用 UTF-8、LF；修改旧文件不删除原注释，不做无关整文件格式化。
- PC Demo 用于开发验证，不要求以相同分辨率或场景运行在 STM32。

### 渲染上下文

新增 `RenderTarget` 管理颜色与深度输出，新增 `RenderWorkspace` 管理相机相关临时数据。Context 管线不再把变换顶点、剔除结果和光照结果写回共享 Mesh。

推荐组合：

- PC 顺序多相机：Target 独立，Workspace 共享；
- PC 并行多相机：Target 独立，Workspace 独立；
- MCU 相机切换：Target 和 Workspace 均可共享；
- MCU 静态内存：通过 `Init` API 绑定调用者提供的固定容量缓存。

### 性能记录

扫描线填充改为流式活动边工作区，不再为整张图建立边表。此前相同场景测量结果：

- `-O2` 约为原实现的 `1.18x`；
- `-Os` 约为原实现的 `2.65x`；
- `.text` 减少 2882 字节；
- 10000 组随机 framebuffer/Z-buffer 差分一致。

向量、矩阵和渲染热路径的临时结果改为栈对象或输出参数，减少帧内小块动态分配。

### 回归结果

- RGB565 Release：库、基础物体 Demo、静态双目 Demo、场景漫游 Project 和 CTest 通过；
- 历史 RGB888 兼容构建曾通过；当前日常构建固定 RGB565；
- ASan、UBSan、LeakSanitizer：模块测试和两个 Demo 正常退出，无报告；
- 三角形与普通多边形 Context 管线均纳入模块测试；
- 动态共享、动态独立、外部静态 Workspace 和外部 Target 均已覆盖；
- RGB565 场景漫游截图 hash：`7ebb5e707ca14cdd3ed8b811ef28517b0613bd4e78d7c28bb65979ef59164400`。

### 已知问题

- YMGUI 端口目前固定创建 main/aux 两个 Image，动态视口列表应移到 ProjectDemo 应用层。
- 文件读取模块有多处未检查 `fread/fgets` 返回值，损坏或截断资源的错误路径不完整。
- YMGRE 与 YMGUI 的公共基础类型重复 typedef，严格 `-Wpedantic` 构建会报警。
- 部分旧文件包含 MSVC 专用 pragma、空函数参数和反斜杠结尾注释告警，后续按模块处理。
- 尚未加入 STM32 工具链编译、cycle 计数和峰值栈测试。

## 2026-08-21：Demo 与 ProjectDemo 边界整理

- `Demo/demo_basic_shapes.c` 静态显示矩形平面、立方体、长方体、圆柱、圆锥和球体；
- `Demo/demo_extended_shapes.c` 静态显示圆环、胶囊体以及正四、正八、正十二、正二十面体；
- `Demo/demo_stereo_view.c` 使用两个独立 Target 和共享 Workspace 显示静态双目视角；
- 原地图入口迁到 `project_Demo/scene_roaming`，用于动态场景漫游；
- 基础网格生成器正式启用，统一输出三角网格、外向法线、AABB 和包围球；
- 新增 `test_basic_mesh`，自动检查数量、索引、退化三角形与法线方向。
- 历史 RGB565/RGB888 截图逐字节一致；当前只保留 RGB565 截图作为日常基准。
- 当前 Object 析构要求每个 Polygon 独立释放索引，基础网格初始化仍会产生多个索引分配；后续可设计连续索引池，减少 MCU 堆碎片。

## 2026-08-21：构建与资源目录收敛

- PC 日常构建固定 RGB565，不再生成 RGB565/RGB888 两套并行构建目录；
- 根 Demo 目标按 YMGUI 方式命名为 `demo_basic_shapes`、`demo_stereo_view`；
- `scene_roaming` 改为带独立 CMakeLists 的 ProjectDemo，输出到 `build/project_Demo/scene_roaming`；
- 地图、模型和图片统一从根 `Resource/` 加载，Demo 目录只保留演示源码和说明。
- 早期 EGE 入口和 2 MB `worldmap.h` 归档到 `Demo/legacy`，历史球体测试块不再编入 YMGRE 库。
