# 统一构建入口与目录

从仓库任意工作目录调用 `build_all.sh` 都可以构建；默认 RGB888、16 位索引、Release，4 个并行任务。脚本先构建核心、基础 Demo 和根工程测试，再自动遍历 `project_Demo/*/CMakeLists.txt` 构建独立应用。任何构建失败都会进入末尾汇总，脚本返回非零。

```sh
./build_all.sh -t
./build_all.sh --depth 16 --index-bits 32 -t
./build_all.sh --core-only --depth 24 --index-bits 32 -t
./build_all.sh --jobs 8
```

`--depth` 可选 16（RGB565）、24（RGB888），`--index-bits` 可选 16、32。两者属于不同配置维度，每种组合使用独立 CMake 缓存与输出目录：

```text
build/
  rgb565/index16/
  rgb565/index32/
  rgb888/index16/
    Demo/                        核心库、基础 Demo、根工程测试
    core/                        --core-only 的无窗口构建
    project_Demo/
      scene_editor/scene_editor
      scene_baker/scene_baker
      scene_roaming/scene_roaming
      human/demo_human
  rgb888/index32/
  _ymgre_sdk/                    SDK 打包中间文件
  YMGRE_libs/                    可独立分发的 SDK，保留现有 Git 放行规则
releases/                        --release 验证后输出的压缩包与校验文件
```

四种组合内部采用相同目录结构。`scene_baker` 当前要求 RGB888：一键构建 RGB565 时会明确显示 SKIP；直接用 CMake 给它传 16 会报错，不再静默覆盖配置。其余应用如缺少依赖，按失败报告，不以成功或跳过掩盖。

桌面 Demo 需要仓库 YMGUI 子模块和 SDL2 开发包；编辑器/烘焙器还使用 mxml，烘焙器需要 PNG/JPEG 开发包与 C++ 编译器。人物 Demo 默认从仓库相邻的 `YMSKE/` 引入源码。`--core-only` 不构建这些窗口和应用依赖。

## 单独构建与测试

```sh
cmake -S project_Demo/scene_editor -B build/rgb888/index16/project_Demo/scene_editor \
  -DYMGRE_CAMERA_COLOR_DEPTH=24 -DYMGRE_INDEX_BITS=16 -DCMAKE_BUILD_TYPE=Release
cmake --build build/rgb888/index16/project_Demo/scene_editor -j4
./build/rgb888/index16/project_Demo/scene_editor/scene_editor

ctest --test-dir build/rgb888/index16/project_Demo/scene_baker --output-on-failure
ctest --test-dir build/rgb888/index16/project_Demo/human --output-on-failure
```

`build_all.sh -t` 只运行根工程测试。应用有自己的测试时，按上例单独运行；不带 `-t` 只编译、不执行测试。`project_Demo/human/run_demo.sh` 也使用 RGB888/index16 的统一目录。

## 配置维护

- 根 `CMakeLists.txt` 是 YMGRE 核心源码与公开编译宏的唯一构建定义，包含 `YMGRE_SceneManager.c`。应用和测试不再单独编译该文件。
- 独立应用统一包含 `project_Demo/ymgre_app.cmake`，由它接入根工程和 YMGUI 显示适配，各应用维护自己的业务源码及额外依赖。
- `sdk/library/CMakeLists.txt` 复用同一个核心目标，附加 SDK 的 色深与 PIC 配置。`./sdk/build.sh` 保持独立打包入口，继续提供 RGB565/RGB888 × 16/32 位索引四种配置。加 `--release` 后将验证过的压缩包和校验文件输出至可提交的 `releases/`，普通构建不修改该目录。
- 仓库内应用仍是源码构建；SDK 示例通过 `find_package(YMGRE)` 使用预编译库，窗口示例额外接入外部 YMGUI。统一源码构建不等于已经完成所有应用向 SDK 的迁移。

## 清理与旧产物

`./build_all.sh -c` 只删除当前色深、索引宽度下本次将构建的单元目录，再重建；`--core-only -c` 只清理对应的 `core/`。不会清空整个 `build/`，不会删除 SDK、其他配置、跳过的应用或旧构建目录。清理遇到符号链接重定向路径会拒绝。

历史 `build/dev`、`build888`、`build-human` 等目录不自动移动或删除。新入口不读取它们；使用新的路径启动程序。CMake 缓存包含绝对路径，应重新配置构建，不能直接移动旧构建目录。仍可自行指定 `cmake -B` 输出位置。
