# 构建入口与产物目录

日常只需下面三个入口，均可从其他工作目录用绝对路径调用：

| 用途 | 命令 | 产物 |
| --- | --- | --- |
| Girl 示例 | `./project_Demo/girl_viewer/build.sh --run` | `build/bin/rgb888/index32/girl_viewer` |
| 完整 SDK | `./sdk/build.sh` | `build/YMGRE_libs/`、`build/archives/` |
| 核心、基础 Demo 和兼容应用（默认不含 Girl） | `./build_all.sh -t` | `build/bin/<色深>/<索引宽度>/` |

```text
build/
  README.md                          入口索引
  bin/                               所有 Demo 和应用程序
    rgb565/index16/
    rgb565/index32/
    rgb888/index16/
    rgb888/index32/
      demo_basic_shapes              基础 Demo
      demo_human                     骨骼人物 Demo
      nature_preview                 自然场景
      scene_editor                   编辑器
      scene_baker                    烘焙器
      scene_bake_cli                 烘焙命令行工具
      scene_roaming                  场景漫游
      girl_viewer                    Girl 示例
  YMGRE_libs/                         可独立分发的 SDK
  archives/                          本地 SDK 压缩包及 SHA256 文件
  .cache/                            CMake、对象文件、测试程序与日志
    sdk/                             SDK 四配置构建及消费者验证
    configs/<色深>/<索引宽度>/
      Demo/                          核心库、基础 Demo 的构建目录
      core/                          --core-only 无窗口构建
      project_Demo/<项目名>/          所有应用的构建目录（含 Girl）
releases/                            仅 --release 更新的正式分发包
```

`bin` 只包含已成功构建的程序；不同色深、索引宽度分开存放。Girl 只是 RGB888/index32 中的一个程序，不再设专属顶层目录。源码 CMake 默认使用上述输出位置，也可通过 `YMGRE_PROGRAM_OUTPUT_DIR` 指定其他程序目录。单元测试工具保留在 `.cache`，使用对应构建目录运行 CTest。

不再使用顶层 `build/girl_viewer`、`build/rgb565`、`build/rgb888`、`build/_ymgre_sdk` 和旧 `build/verification`。CMake 缓存含绝对路径，旧目录已清理；在新路径重新构建，不能直接搬动缓存继续使用。原始资源仍在 `Resource/`，不属于构建缓存。

## Girl 示例

```sh
./project_Demo/girl_viewer/build.sh
./project_Demo/girl_viewer/build.sh --test
./project_Demo/girl_viewer/build.sh --run -- --threads 8
./project_Demo/girl_viewer/build.sh --clean --jobs 8
```

脚本固定使用 RGB888/index32，启用完整材质和 PNG/JPEG 解码功能；需要 libpng、libjpeg、SDL2、SDL2_ttf 和字体。`--test` 执行核心测试及无窗口 Girl 截图检查，截图在 `build/.cache/configs/rgb888/index32/project_Demo/girl_viewer/captures/`。默认只编译查看器。可执行文件路径保持稳定，不会混入 CMake 文件。

SDK 用户使用包内 `examples/girl_viewer/CMakeLists.txt`，按对应 README 设置 `YMGRE_SDK_ROOT`；仓库的 `build.sh` 是源码构建入口，不随 SDK 打包。

## 核心和其他应用

```sh
./build_all.sh -t
./build_all.sh --depth 24 --index-bits 32 -t   # 同时构建 Girl
./build_all.sh --depth 16 --index-bits 32 -t
./build_all.sh --core-only --depth 24 --index-bits 32 -t
./build_all.sh --jobs 8
```

默认 RGB888、index16、Release、4 个并行任务。`--depth` 可选 16/24，`--index-bits` 可选 16/32。每个组合使用独立缓存。脚本汇总失败并返回非零；RGB565 明确跳过只支持 RGB888 的 `scene_baker`；`girl_viewer` 只在 RGB888/index32 的全应用构建中调用自己的构建脚本，其余组合明确跳过。`--core-only` 不需要窗口或应用依赖。

`-t` 运行根工程测试，其他应用测试另行执行。下面以 `scene_baker` 的构建和测试为例：

```sh
cmake -S project_Demo/scene_baker -B build/.cache/configs/rgb888/index16/project_Demo/scene_baker \
  -DYMGRE_CAMERA_COLOR_DEPTH=24 -DYMGRE_INDEX_BITS=16 -DCMAKE_BUILD_TYPE=Release
cmake --build build/.cache/configs/rgb888/index16/project_Demo/scene_baker -j4
ctest --test-dir build/.cache/configs/rgb888/index16/project_Demo/scene_baker --output-on-failure
```

其他桌面应用需要 YMGUI、SDL2，以及各自 README 中的附加依赖；`human` 默认从相邻的 `YMSKE/` 引入源码。

## SDK 与清理

`./sdk/build.sh` 复用根核心目标，构建 RGB565/RGB888 × index16/index32 四种配置并验证独立消费者。默认生成完整包；`--minimal` 裁掉七项可选能力并替换同一个 SDK 目录。两种模式都会更新 `build/YMGRE_libs/`；普通构建的压缩包和校验文件写入 `build/archives/`，`--release` 则写入 `releases/`。详细用法见 `sdk/README.md` 和 `sdk/MANUAL.md`。

`build/.cache/` 可在停止构建后删除，下次重新编译；已输出到 `build/bin/` 的 Demo、`build/YMGRE_libs/` 和分发包均保留。`build_all.sh --clean` 只清理当前组合中将要构建的单元，Girl 脚本的 `--clean` 只清理自己的构建目录。两者均拒绝通过符号链接重定向的清理路径。清理后若重建失败，`bin` 中可能仍是上一次成功构建的程序，应检查构建结果。

`build/YMGRE_libs` 中已跟踪的文件保持原有管理方式，其余构建输出仍忽略。不要把源文件或手工维护资源放入 `build/`。
