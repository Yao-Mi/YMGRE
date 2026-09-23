# YMGRE 预编译 SDK

这里存放供 Git 提交和下载的分发包。当前提供 Linux x86_64、Release/PIC、RGB565 / RGB888 × 16 / 32 位索引的四份核心静态库。包内包含配套头文件、CMake 接口、30 个 Demo 源码、手册、效果图与验收日志；不捆绑 YMGUI 或 SDL。其他平台需使用对应工具链重新构建。

- [下载 SDK](YMGRE_libs-linux-x86_64.tar.gz)
- [SHA-256 校验文件](YMGRE_libs-linux-x86_64.tar.gz.sha256)

从仓库根目录执行：

```sh
(cd releases && sha256sum -c YMGRE_libs-linux-x86_64.tar.gz.sha256)
mkdir -p build/sdk-unpacked
tar -xzf releases/YMGRE_libs-linux-x86_64.tar.gz -C build/sdk-unpacked
cd build/sdk-unpacked/YMGRE_libs
sha256sum -c checksums.sha256
./build_demos.sh 16 --depth 16
./build_demos.sh 32 --depth 16
./build_demos.sh 16 --depth 24
./build_demos.sh 32 --depth 24
```

自己的工程通过 `find_package(YMGRE CONFIG REQUIRED)` 接入，将 `YMGRE_DIR` 指向解压目录中的 `cmake/`。无窗口示例无需 YMGUI/SDL；窗口 Demo 需另外准备架构和色深匹配的 YMGUI SDK。详见包内 `README.md`、`手册.md`，或仓库的 [接入手册](../sdk/MANUAL.md)。

## 维护分发包

```sh
./sdk/build.sh --release
# 同时验证与外部 YMGUI SDK 的窗口示例
./sdk/build.sh --release --ymgui-dir /absolute/path/YMGUI_libs/cmake
```

发布脚本先在临时目录组装 SDK，完成四种组合的独立消费者验收后，再更新本地 `build/YMGRE_libs/` 和这里的压缩包、校验文件。提供 `--ymgui-dir` 时还必须通过窗口示例验收。构建或验收失败不会替换已有 SDK 和分发包；压缩失败不会截断已有压缩包。

普通 `./sdk/build.sh` 只更新 `build/YMGRE_libs/` 和 `build/` 下的本地压缩包，不修改 `releases/`。可用 `--jobs 8` 调整并行度。发布不会自动执行 Git 提交、推送或创建远程 Release；应将压缩包、校验文件与对应源码和文档一起提交。

现有 `build/YMGRE_libs/` 的 Git 放行规则继续保留。`releases/` 为直接下载压缩包提供固定入口，两者都由同一脚本生成。
