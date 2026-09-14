# YMGRE 预编译库使用手册

## 1. 本包提供什么

本包提供 YMGRE 核心静态库的两种索引配置、配套公开头文件、独立示例工程及已编译的无窗口示例。
日常使用只需阅读本手册和头文件，链接 `lib/` 中的静态库，不需要编译或阅读引擎实现。

| 项目 | 本包配置 |
|---|---|
| 系统和架构 | Linux x86_64，具体编译器见 `BUILD_INFO.json` |
| 构建 | Release，位置无关代码，C99 接口 |
| 颜色输出 | RGB565，`YMGRE_CAMERA_COLOR_DEPTH=16` |
| 顶点索引 | `GRE_Index` 可选 16 位或 32 位，每个进程选择一种配置 |
| 核心依赖 | C 标准库和数学库 `libm` |
| 窗口示例依赖 | 外部 YMGUI SDK（RGB565）及其 SDL 平台适配，系统 SDL2 |
| 使用边界 | 本机 CPU 软件渲染；实际 MCU 必须使用目标芯片工具链另行编译 |

核心库包含基础网格、坐标变换、相机、裁剪、深度缓冲、光栅化、材质与光照、光追求交与着色函数、Ogre 网格及材质读取、场景管理。
`YMGRE_SceneManager.c` 的实现已经封装到库中，应用不需要另加这个源文件。
编辑器的 UI、UV 自动展开/编辑、画板、场景烘焙生成器，以及编辑器的递归光追视口属于 `scene_baker` 应用层，本包没有把它们发布为核心库 API。
核心可以渲染已有的光照贴图；完整编辑器工程仍在原仓库。

## 2. 目录与选型

```text
YMGRE_libs/
  lib/libymgre_index16.a           # 默认，GRE_Index = uint16
  lib/libymgre_index32.a           # 大网格，GRE_Index = uint32
  include/YMGRE/                  # 保持原目录层级的公开头文件
  cmake/YMGREConfig.cmake          # 预编译库的 CMake 接入入口
  examples/                       # 示例源码及 host 接入代码；不含引擎实现
  bin/index16/                    # 16 位索引无窗口示例程序
  bin/index32/                    # 32 位索引无窗口示例程序
  verification/                   # 自动验收记录
  BUILD_INFO.json                 # 平台、编译器、配置、源码摘要
  checksums.sha256                # 包内文件完整性校验
```

16 位索引占 2 字节，单子网格顶点数上限保留为 65535；多个子网格、多个物体可以累计超过此数量。
32 位索引占 4 字节，可引用超过 65535 编号的顶点，但对象数量字段仍是 `int`，实际规模受内存、算法和文件格式限制。
三角形每个面有三个索引，索引本体分别占 6 或 12 字节；这不包含多边形结构和分配器管理开销。
两份库的顶点坐标、法线、贴图和像素格式相同。

推荐 MCU 继续使用 16 位索引。PC 大网格可用 32 位；不要同时把两份核心库链接进同一个可执行文件。
使用本包时无需修改 typedef，CMake 会根据选中的库传递正确配置。

## 3. 先运行一个 Demo

从本包目录执行已编译的纯核心示例：

```sh
./bin/index16/demo_sdk_minimal
./bin/index32/demo_sdk_minimal
```

它不需要 SDL 或 YMGUI，会输出当前索引大小、像素大小和有效像素数量，并在当前目录生成 `cube.ppm`。

29 个窗口 Demo 以源码形式提供。本包不携带 YMGUI 头文件、静态库，也不携带已经静态嵌入 YMGUI 的窗口可执行文件。
准备独立的 YMGUI SDK 后，按第 5 节构建，再运行：

```sh
./examples-build16/bin/demo_basic_shapes
./examples-build32/bin/demo_extended_shapes
./examples-build16/bin/demo_advanced_raytrace_mirror
```

关闭窗口退出。示例窗口默认放大显示，可以设置 `YMGRE_WINDOW_SCALE=1` 使用原始尺寸。

## 4. 只链接库的新工程

安装 C 编译器、CMake 即可使用核心库；窗口示例另需独立的 YMGUI SDK、`pkg-config` 和 SDL2 开发包。
新建一个应用目录，把 `examples/demo_sdk_minimal.c` 复制为 `main.c`，加入：

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_ymgre_app C)
set(CMAKE_C_STANDARD 99)
find_package(YMGRE CONFIG REQUIRED)
add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE YMGRE::ymgre)
```

配置时传入本包位置：

```sh
cmake -S . -B build16 -DYMGRE_DIR=/absolute/path/YMGRE_libs/cmake -DYMGRE_INDEX_BITS=16
cmake --build build16
./build16/my_app
```

32 位版本用新的构建目录：

```sh
cmake -S . -B build32 -DYMGRE_DIR=/absolute/path/YMGRE_libs/cmake -DYMGRE_INDEX_BITS=32
cmake --build build32
```

`YMGRE::ymgre` 是预编译的导入目标，自动配置头文件路径、索引位宽、RGB565 和 `libm`。
这里没有 `add_subdirectory` 引擎源码，也没有把 `YMGRE/*.c` 加到应用里。

窗口接入属于应用层。示例工程会先 `find_package(YMGRE CONFIG REQUIRED)`，再按需执行 `find_package(YMGUI CONFIG REQUIRED)`，
并编译本包自己的 `examples/host/demo_host.c` 接入代码。
这个接入文件属于 Demo，不是 YMGRE 核心依赖；其 YMGUI 头文件和库由外部 SDK 提供。
自有窗口或 LCD 项目可以直接使用核心库的颜色缓冲，无需这个适配层。
公开 API 使用 C 接口；C++ 调用方应在 `extern "C" { ... }` 中包含 YMGRE 头文件。

## 5. 重新编译整套示例

下载并解压本包后，在本包目录一键编译和验收 Demo：

```sh
./build_demos.sh 16
./build_demos.sh 32
```

默认只构建无窗口示例，不查找 YMGUI/SDL，也不编译引擎。
有外部 YMGUI SDK 时，把其 `YMGUIConfig.cmake` 所在目录作为第二个参数：

```sh
./build_demos.sh 16 /absolute/path/YMGUI_libs/cmake
./build_demos.sh 32 /absolute/path/YMGUI_libs/cmake
```

这会显式启用窗口示例。未提供有效外部包时应报依赖错误，不会退回仓库里的 YMGUI 源码。
目前约定外部包导出 `YMGUI::ymgui` 和 `YMGUI::sdl` 两个 CMake 目标，
分别提供 GUI 核心和包含 `SDL_LCD.h` 的 SDL 适配层，以及各自的头文件路径和依赖。
这是给未来独立 YMGUI 包预留的接口约定，不表示该正式包已经发布。
外部包的像素配置应是 `YMGUI_COLOR_DEPTH=16`，并与本包架构和 ABI 匹配。
如果正式包使用其他目标名，可通过 CMake 的 `YMGRE_YMGUI_TARGET`、`YMGRE_YMGUI_SDL_TARGET` 指定实际目标。

下面是默认无窗口构建的等价 CMake 命令：

```sh
cmake -S examples -B examples-build16 -DYMGRE_INDEX_BITS=16
cmake --build examples-build16 -j4
ctest --test-dir examples-build16 --output-on-failure

cmake -S examples -B examples-build32 -DYMGRE_INDEX_BITS=32
cmake --build examples-build32 -j4
ctest --test-dir examples-build32 --output-on-failure
```

窗口测试会由 CTest 自动设置 `YMGRE_HEADLESS=1` 和 `YMGRE_MAX_FRAMES=1`，用 SDL 的无窗口驱动完成一次运行；纯核心示例不用 SDL。
测试可检查链接、执行、退出码，以及有数值判据的 Demo 自检；视觉效果仍可手动打开窗口确认。
新程序位于各构建目录的 `bin/`。只构建无需 SDL 的最小示例：

```sh
cmake -S examples -B core-build -DYMGRE_WINDOWED_DEMOS=OFF
cmake --build core-build
ctest --test-dir core-build --output-on-failure
```

## 6. 一帧渲染的调用顺序

完整可编译代码见 `examples/demo_sdk_minimal.c`。

1. 用基础形状生成器或 `YMGRE_Creat_Object` 创建网格，设置材质和物体位置。
2. 对本地顶点调用 `YMGRE_Object_LocalToWorld` 完成世界变换；该函数会变更顶点，不要对同一份顶点重复累计应用同一变换。
3. 把物体、光源、材质加入各自的 `gre_list`。
4. 用 `YMGRE_Creat_Camera` 创建相机，再设置视锥近远平面和 UVN 位置/朝向。
5. 创建 `GRE_RenderWorkspace`，选择基础或高级管线渲染。
6. 从 `YMGRE_Camera_GetRenderTarget` 取得颜色和深度缓冲，显示或保存。
7. 退出时释放工作区、相机和场景资源。

基础管线调用：

```c
YMGRE_Camera_TanglePipline_RenderingWithWorkspace(
    camera, &lights, &objects, &materials, workspace);
```

高级管线需要对象具有法线/切线等扩展属性。先调用
`YMGRE_Object_GenerateVertexAttributes(object)`，再设置 `object->renderMode` 并调用：

```c
YMGRE_Camera_TanglePipline_wN(camera, &lights, &objects, &materials, workspace);
```

`GRE_RenderMode_Face`、`GRE_RenderMode_Vertex`、`GRE_RenderMode_Pixel` 分别表示逐面、逐顶点和逐像素光照。
光追相关函数独立于光栅管线，通过 `YMGRE_Ray_FromCameraPixel` 生成射线，求交后着色；镜面和玻璃示例展示反射/折射二次射线的组织方法。

## 7. 常用 API 按任务查找

| 任务 | 公开头文件 | 常用入口 |
|---|---|---|
| 平台类型与像素格式 | `YMGRE_PubType.h` | `GRE_Index`、`GRE_FramePixel`、RGB565 转换函数 |
| 创建与释放 | `YMGRE_Creat.h`、`YMGRE_Free.h` | `YMGRE_Creat_Camera/Object/Light/Material` 和对应释放接口 |
| 基础网格 | `YMGRE_BasicMesh_Gener.h` | Cube、Box、RectPlane、Sphere、Cylinder、Cone、Torus、Capsule |
| 相机 | `YMGRE_Camera.h` | `YMGRE_Camera_Frustum_Init`、`YMGRE_UVNCamera_PositionInit` |
| 变换 | `YMGRE_Coordinates_Transform.h` | 本地/世界/相机/投影变换 |
| 渲染目标和工作区 | `YMGRE_RenderContext.h` | `YMGRE_Creat_RenderTarget/RenderWorkspace`、绑定外部缓冲 |
| 主管线 | `YMGRE_Rendering_Pipeline.h` | 三角形、多边形、高级顶点、顶点色、独立线段 |
| 光栅与线框 | `YMGRE_Rasterization.h`、`YMGRE_TriangleRaster.h` | 多边形填充、三角形填充、深度线段 |
| 裁剪 | `YMGRE_CullingAndClipping.h` | 视锥/窗口裁剪、背面剔除 |
| 光线追踪 | `YMGRE_RayTracing.h` | 相机射线、网格/球/平面/AABB/圆柱求交、反射折射、Blinn–Phong |
| 材质与文件 | `YMCS_File_IO.h` | `YMGRE_LoadOgreMeshAndMaterial`、BMP 读取 |
| 场景 | `YMGRE_ScenceManager.h` | 场景与资源管理，注意保留的历史拼写 `Scence` |
| 容器 | `YMGRE_List.h` | `YMGRE_List_Append/Clear` |
| 窗口示例 | `demo_host.h` | `YMGRE_DemoHost_Init/AddTarget/Run/Destroy` |

具体参数、结构体字段和完整函数声明以包内头文件为准。低层光栅函数通常接受投影后的顶点；
日常绘制世界空间物体优先使用相机主管线，避免绕过裁剪和坐标变换。

## 8. 内存、深度与资源约定

- `YMGRE_List_Append` 挂载传入指针，不会复制对象。`YMGRE_List_Clear` 调用传入的释放回调；不要再重复释放同一对象。
- `nextObject` 用于多个子网格的链，释放入口的所有权约定见 `YMGRE_Free.h`；共享资源不得交给多个所有者重复释放。
- 原始顶点 `pointList` 与工作区中的变换顶点用途不同，不要把屏幕坐标覆盖到永久模型上。
- `GRE_Index` 的申请使用 `sizeof(GRE_Index)` 或 `sizeof(*index)`，不要硬编码每个三角形 6 字节。
- 外部帧缓冲使用 `YMGRE_RenderTarget_Init` 绑定，不转移所有权；只对 `Creat` 返回的目标调用相应 `Free`。
- 深度缓冲是 `float32` 相机空间 `z`。透视光栅化内部插值 `1/z` 后恢复 `z`；不要把射线欧氏距离直接和这个缓冲比较。
- 本包是 RGB565 输出，但材质纹理数据常使用 `GRErgb24`；读取帧像素请用 `GRE_FramePixel_To_RGB24`。
- 静态场景可以只渲染一次，多相机可复用工作区；需要避免并发修改同一对象、全局画笔或工作区。
- 使用固定内存时，按 `YMGRE_RenderContext.h` 绑定帧缓冲及工作区容量。动态分配器封装位于库内；MCU 内存池策略属于对应平台库的构建配置。

Ogre 读入支持的格式是现有静态模型加载路径，并非所有 Ogre 版本/顶点布局都兼容。
32 位构建可以读入 16 位文件；文件索引会检查范围再转换，16 位库不能加载超出自身容量的单子网格。
部分非法输入沿用库的诊断终止机制，文件和参数应在应用边界验证，不应把加载函数当作任意文件的容错解析器。

## 9. Demo 导航

| 学习方向 | Demo |
|---|---|
| 最小无窗口程序 | `demo_sdk_minimal` |
| 基础和扩展形状 | `demo_basic_shapes`、`demo_extended_shapes` |
| 填充与三角化 | `demo_polygon_fill`、`demo_polygon_triangulation` |
| 深度和裁剪 | `demo_depth_overlap`、`demo_frustum_clipping`、`demo_window_clipping`、`demo_backface_culling` |
| 公共边和拼接 | `demo_fragment_stitching`、`demo_shared_edge_stress` |
| 贴图和光照 | `demo_material_texture`、`demo_lighting` |
| 多相机和输出 | `demo_render_target`、`demo_camera_viewport`、`demo_stereo_view` |
| 高级材质 | `demo_advanced_vertex`、`demo_advanced_texture`、`demo_advanced_perspective`、`demo_advanced_normal_map`、`demo_advanced_specular` |
| 着色模式和高级裁剪 | `demo_advanced_render_modes`、`demo_advanced_clipping_planes` |
| 光追基础 | `demo_advanced_raytrace`、`demo_advanced_raytrace_object`、`demo_advanced_raytrace_shading` |
| 阴影、镜面和折射 | `demo_advanced_raytrace_shadow`、`demo_advanced_raytrace_mirror`、`demo_advanced_raytrace_refraction`、`demo_advanced_raytrace_materials` |

## 10. 发布配置与问题定位

开发项目固定本包版本，同时保留 `BUILD_INFO.json`。更新时整体替换库、头文件、CMake 配置，重新编译应用；
不混用不同索引位宽、颜色格式或不同版本的头文件与二进制库。
手工链接时，必须给应用定义匹配的 `YMGRE_INDEX_BITS` 和 `YMGRE_CAMERA_COLOR_DEPTH=16`，并设置各公开头文件目录。
优先使用导入的 CMake 目标减少配置遗漏。

在本包目录运行 `sha256sum -c checksums.sha256` 可核对文件完整性。
出现问题时先记录库配置、调用参数、模型和最小 Demo，以及是否能在两种索引配置下复现，再回到源码修复。
维护库时，在原仓库运行 `./sdk/build.sh` 即可一键重新发布（也可直接运行 `python3 sdk/package.py`）。
脚本依次编译两种索引库、复制 YMGRE 头文件和示例、从独立 SDK 副本编译并验收无窗口 Demo，再生成：

```text
build/YMGRE_libs/
build/YMGRE_libs-linux-x86_64.tar.gz
build/YMGRE_libs-linux-x86_64.tar.gz.sha256
```

过程日志保存在 `build/_ymgre_sdk/`；任一步失败，脚本返回非零状态，不生成新的压缩包。
默认发布只需要 C 编译器、CMake、Python 3 和 binutils（ar/nm），不需要 YMGUI 或 SDL2。
如需同时验证窗口示例，使用 `./sdk/build.sh --ymgui-dir /absolute/path/YMGUI_libs/cmake`。
该选项需要外部 YMGUI SDK、pkg-config、SDL2；只增加验收，不会把外部包或窗口二进制塞入 YMGRE 发布包。
脚本不会自动安装依赖。
正常应用构建只需包内的 `build_demos.sh` 或 CMake 导入目标，不调用源码发布脚本。
Git 只保留 `build/YMGRE_libs/` 作为 SDK，其他构建目录及旁边的发布压缩包仍被忽略。
下载使用者在 SDK 内生成的 `examples-build*`、`core-build` 和 `cube.ppm` 也会被忽略。
原生构建不等于跨芯片二进制兼容。MCU 发布应另外确认 CPU、ABI、浮点选项、工具链和内存实现。

## 授权与分发

个人及非商业组织以非营利目的学习、研究或自用 YMGRE，可免费使用；商业用途必须事先说明并取得著作权人的书面授权。对外分发修改版或使用修改版对外提供服务时，须按许可条款公开本库的对应源码及必要构建文件。独立应用不因仅调用或链接本库而必须公开业务代码。第三方资源及外部依赖按各自原许可使用。完整条款位于 SDK 根目录 `LICENSE`，并在 `licenses/YMGRE-LICENSE` 保留副本，许可标识为 `LicenseRef-YMGRE-Noncommercial-1.2`。

商业授权通过项目发布页公布的联系方式申请，说明使用主体、用途、规模与交付方式，并以双方确认的书面文件为准。分发时须保留完整许可及已有版权、第三方许可说明；修改版本须注明已作修改。具体源码公开范围与例外以许可原文为准。

本声明不撤回使用者依据此前有效许可已经取得的权利。YMGUI 的许可及发布由其独立 SDK 管理；SDL2 为窗口示例的外部系统依赖。
