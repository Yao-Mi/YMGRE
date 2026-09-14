<h1 align="center">YMGRE SDK</h1>
<p align="center"><strong>下载库，链接它，开始绘制三维场景。</strong></p>
<p align="center">C99 · CPU Rendering · uint16 / uint32 索引 · RGB565</p>

<p align="center"><a href="手册.md">使用手册</a> · <a href="examples/">Demo 源码</a> · <a href="BUILD_INFO.json">构建配置</a> · <a href="verification/">验收记录</a></p>

| 镜面反射 | 玻璃折射 |
|:---:|:---:|
| ![真实渲染的镜面与光源倒影](docs/images/advanced_raytrace_mirror.png) | ![真实渲染的玻璃球折射](docs/images/advanced_raytrace_refraction.png) |

| 透视校正贴图 | 法线贴图 |
|:---:|:---:|
| ![透视棋盘格](docs/images/advanced_perspective.png) | ![法线贴图受光对照](docs/images/advanced_normal_map.png) |

*图片为窗口 Demo 的实际渲染输出。显示层使用外部 YMGUI/SDL；本包不捆绑它们的头文件或静态库。*

## 包里有什么

**两份核心静态库、一套公开头文件、一本使用手册、30 个 Demo 源码。**
核心包含几何生成、相机与裁剪、光栅化、材质光照、光追求交/着色函数及场景资源管理。

| 路径 | 用途 |
|---|---|
| `lib/libymgre_index16.a` | `GRE_Index = uint16`，每个索引 2 字节 |
| `lib/libymgre_index32.a` | `GRE_Index = uint32`，支持更大的单个网格 |
| `include/YMGRE/` | 和库匹配的公开头文件 |
| `cmake/` | 自动选择位宽和依赖的 CMake 配置 |
| `examples/` | 无窗口示例、29 个窗口 Demo 和应用侧 `host` 接入代码 |
| `bin/index16/`、`bin/index32/` | 可直接运行的无窗口示例 |
| `手册.md` | 调用流程、API 导航、内存约定、Demo 分类和平台说明 |

当前二进制目标为 **Linux x86_64 / Release / RGB565**。核心只需要 C 标准库和 `libm`。
MCU 使用相同接口，但需按目标芯片和工具链另行构建库。

## 一分钟开始

解压后进入本目录：

```sh
./build_demos.sh 16
./examples-build16/bin/demo_sdk_minimal
```

示例会生成 `cube.ppm` 并输出 `PASS`。无需引擎源码、YMGUI 或 SDL。
需要 32 位索引时执行 `./build_demos.sh 32`，两套构建互不混用。

## 在自己的工程里使用

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_demo C)
set(CMAKE_C_STANDARD 99)
find_package(YMGRE CONFIG REQUIRED)
add_executable(my_demo main.c)
target_link_libraries(my_demo PRIVATE YMGRE::ymgre)
```

```sh
cmake -S . -B build \
  -DYMGRE_DIR=/absolute/path/YMGRE_libs/cmake \
  -DYMGRE_INDEX_BITS=16
cmake --build build
```

把 [`examples/demo_sdk_minimal.c`](examples/demo_sdk_minimal.c) 作为第一个 `main.c` 即可。
导入目标会自动设置头文件、索引位宽、RGB565 和数学库；应用不编译任何引擎实现。

## 窗口 Demo 与 YMGUI

YMGRE 和 YMGUI 分开发布。准备独立的 YMGUI SDK 后：

```sh
./build_demos.sh 16 /absolute/path/YMGUI_libs/cmake
./examples-build16/bin/demo_basic_shapes
./examples-build16/bin/demo_advanced_raytrace_mirror
```

外部包需要匹配架构、RGB565，以及提供兼容的 CMake 目标。当前预留的包接口约定和完整 Demo 导航见 [使用手册](手册.md)。
未提供外部包时只构建无窗口示例，不会自动寻找或编译仓库里的 YMGUI 源码。

## 配置、验证与版本

- 16 位库保留单子网格 65535 顶点上限；32 位库仍受 `int` 计数、内存和具体算法约束。
- 切换配置或升级版本时，库与头文件一起更新并重新编译应用。
- `BUILD_INFO.json` 记录构建平台、编译器和源码摘要；`verification/` 保存本次验收结果。
- 在本目录运行 `sha256sum -c checksums.sha256` 可验证文件完整性。
- 场景编辑器、UV 编辑和烘焙生成器是应用层工具，不包含在这两份核心库中。

维护库时可在原仓库运行 `./sdk/build.sh` 一键重新发布；下载本包进行 Demo 开发不需要它。

## 授权声明

个人及非商业组织以非营利目的学习、研究或自用 YMGRE，可免费使用；商业用途必须事先说明并取得著作权人的书面授权。对外分发修改版或使用修改版对外提供服务时，须按许可条款公开本库的对应源码及必要构建文件。独立应用不因仅调用或链接本库而必须公开业务代码。第三方资源及外部依赖按各自原许可使用。完整条款见 [LICENSE](LICENSE)，同一份许可也保存在 [licenses/YMGRE-LICENSE](licenses/YMGRE-LICENSE)。

许可标识：`LicenseRef-YMGRE-Noncommercial-1.2`。本声明不撤回使用者依据此前有效许可已经取得的权利。
