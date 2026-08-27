# YMGRE 模块测试

模块测试默认随根工程构建：

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

关闭测试：

```bash
cmake -S . -B build -DYMGRE_BUILD_TESTS=OFF
```

## test_render_context

验证：

- 两个相机使用独立 Target 时互不覆盖；
- 两个相机共享 Workspace 时输出一致；
- 相机使用独立 Workspace 时输出一致；
- MCU 风格外部静态 Workspace 输出一致；
- 外部 Target 不需要相机内部分配 framebuffer，并能通过绑定查询接口取得；
- 普通多边形 Context 管线在共享和独立 Workspace 下输出一致；
- Context 管线不修改对象和灯光上的兼容临时状态。

新增图形测试应优先比较 framebuffer 和 Z-buffer，不使用肉眼截图作为唯一判据。

## test_degenerate_geometry

验证重复顶点、零长度边、共线三角形、近零面积多边形和完全位于窗口外的图元不会产生异常像素；同时检查颜色与深度缓存哨兵、深度范围，以及退化输入后的正常图元仍可绘制。

## test_basic_mesh

验证全部基础网格生成器，包括平面、常用立体、圆环、胶囊体和规则多面体：

- 顶点和三角形数量符合分段参数；
- 所有索引都位于顶点范围内；
- 三角形无退化，凸体法线朝向物体外部，圆环内外壁法线方向正确；
- 每个基础网格都设置有效包围球。

## test_resource_material

验证材质与资源从文件到片元的完整链路：

- 材质默认参数、按名查找、UV 重复采样和禁用材质回退；
- 2x2 纹理经过三角形光栅器后能在 framebuffer 中产生对应 texel；
- 24 位 BMP 在存在行补齐时仍能逐像素无损写入并读回；
- 仓库中的坦克材质脚本能解析颜色并加载同目录 BMP；
- 重复材质脚本不会在场景材质表中产生重复项。
