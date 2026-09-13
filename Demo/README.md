# Demo

这里放单个模块或基础渲染能力的可视演示。Demo 应保持静态或有限步骤，不承担完整应用状态机。

## 约定

- 一个源文件只突出一个主题，尽量不依赖地图、模型等大型外部资源；
- 可以展示基础网格、光照、材质、裁剪、单相机或静态多相机结果；
- 必须支持 `YMGRE_MAX_FRAMES` 无头自动退出；
- 使用 `demo_host.h` 将 RenderTarget 直接绑定到 YMGUI Image，不从 YMGRE 引入显示端口；
- 可自动判定的结构与像素正确性仍放入 `tests/`，Demo 不替代模块测试；
- 持续输入、场景更新、资源管理和漫游流程放入 `project_Demo/`。

实体叠加网格线的 Demo 会显示三角剖分产生的内部共享边。内部边两侧都有实体颜色属于正常结果；检查填充越界时应观察模型真正的投影外轮廓。

## 当前 Demo

- `demo_basic_shapes.c`：以实体叠加网格线显示矩形平面、立方体、长方体、圆柱、圆锥和球体，构建目标为 `demo_basic_shapes`；
- `demo_extended_shapes.c`：显示圆环、胶囊体、正四面体、正八面体、正十二面体和正二十面体，构建目标为 `demo_extended_shapes`；
- `demo_stereo_view.c`：两个相机共享静态场景和 Workspace，各自保留 Target，构建目标为 `demo_stereo_view`。
- `demo_polygon_fill.c`：对照三角形、四边形、五边形的纯填充与填充叠线，构建目标为 `demo_polygon_fill`；
- `demo_fragment_stitching.c`：对照三角形、矩形和五边形片元的拼接结果，构建目标为 `demo_fragment_stitching`；
- `demo_depth_overlap.c`：对照不同绘制顺序下的深度遮挡结果，构建目标为 `demo_depth_overlap`；
- `demo_frustum_clipping.c`：展示多边形穿越近远平面和视锥侧面的裁剪结果，构建目标为 `demo_frustum_clipping`；
- `demo_shared_edge_stress.c`：以密集网格、辐射扇形和窄片元带检查共享边，构建目标为 `demo_shared_edge_stress`；
- `demo_backface_culling.c`：对照正反绕序和镜像修正后的背面剔除结果，构建目标为 `demo_backface_culling`；
- `demo_camera_viewport.c`：对照不同尺寸、宽高比和视场角的独立相机目标，构建目标为 `demo_camera_viewport`；
- `demo_window_clipping.c`：对照完整多边形和矩形窗口裁剪结果，构建目标为 `demo_window_clipping`。
- `demo_degenerate_geometry.c`：展示重复顶点、共线、极窄和近零面积等退化几何的处理结果，构建目标为 `demo_degenerate_geometry`。
- `demo_lighting.c`：并排对比全局光、点光源和聚光灯，构建目标为 `demo_lighting`；
- `demo_material_texture.c`：使用程序生成的棋盘纹理展示材质查找、UV 采样和纹理三角形光栅化，构建目标为 `demo_material_texture`；
- `demo_render_target.c`：两个相机分别绑定独立 RenderTarget，并共享场景和 Workspace，构建目标为 `demo_render_target`。
- `demo_polygon_pipeline.c`：使用多边形 Context 管线渲染基础立方体，构建目标为 `demo_polygon_pipeline`。
- `demo_raster_primitives.c`：集中验证直线、扫描线填充和二维多边形裁剪后的底层绘制结果，构建目标为 `demo_raster_primitives`。
- `demo_polygon_triangulation.c`：对照凹多边形直接填充与耳切三角剖分后的三角形填充，构建目标为 `demo_polygon_triangulation`。

运行基础物体 Demo：

```bash
cmake -S . -B build
cmake --build build --target demo_basic_shapes -j
./build/demo_basic_shapes
```

重新编译全部基础 Demo：

```bash
cmake -S . -B build -DYMGRE_BUILD_DEMOS=ON -DYMGRE_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

运行扩展基础形状 Demo：

```bash
cmake --build build --target demo_extended_shapes -j
./build/demo_extended_shapes
```

运行静态双目 Demo：

```bash
cmake --build build --target demo_stereo_view -j
./build/demo_stereo_view
```

运行多边形填充 Demo：

```bash
cmake --build build --target demo_polygon_fill -j
./build/demo_polygon_fill
```

每种形状左侧为纯填充，右侧为填充叠线。

运行片元拼接 Demo：

```bash
cmake --build build --target demo_fragment_stitching -j
./build/demo_fragment_stitching
```

三行依次为五个三角片元、四个矩形片元和五个五边形片元，左侧为纯填充，右侧为填充叠线。

运行深度遮挡 Demo：

```bash
cmake --build build --target demo_depth_overlap -j
./build/demo_depth_overlap
```

左侧按远到近绘制，右侧按近到远绘制；两侧交叠区域都应显示近处蓝色三角形。

运行视景体裁剪 Demo：

```bash
cmake --build build --target demo_frustum_clipping -j
./build/demo_frustum_clipping
```

六个面板依次展示完全可见、穿越近面、穿越远面、穿越左侧面、穿越上侧面和完全位于相机后方；最后一个面板应为空。

运行共享边压力 Demo：

```bash
cmake --build build --target demo_shared_edge_stress -j
./build/demo_shared_edge_stress
```

三行依次为密集三角网格、中心辐射扇形和不规则窄片元带；左侧为纯填充，右侧为填充叠线。

运行背面剔除 Demo：

```bash
cmake --build build --target demo_backface_culling -j
./build/demo_backface_culling
```

左列关闭剔除，右列开启剔除；三行依次为正向绕序、反向绕序和镜像后修正绕序，只有第二行右侧应为空。

运行多相机视口 Demo：

```bash
cmake --build build --target demo_camera_viewport -j
./build/demo_camera_viewport
```

左侧为五百乘三百宽视口，右侧为二百四十乘三百窄视口；两个相机使用不同视场角和观察位置，并共享渲染工作区。

运行窗口裁剪 Demo：

```bash
cmake --build build --target demo_window_clipping -j
./build/demo_window_clipping
```

画面左侧是不设置自定义窗口的完整多边形，右侧是裁剪到可见矩形窗口后的结果。

运行退化几何 Demo：

```bash
cmake --build build --target demo_degenerate_geometry -j
./build/demo_degenerate_geometry
```

六个面板从左到右、从上到下依次为有效重复顶点、全重复点、斜向共线、极窄三角形、近零面积和退化输入后的正常图元；第二、三、五个面板应为空。

运行光照对比 Demo：

```bash
cmake --build build --target demo_lighting -j
./build/demo_lighting
```

画面依次为全局光、点光源和聚光灯；点光与聚光共址，聚光强度为 `1.2`，方向指向球体中心，用于对比距离、法线和锥角衰减。

运行材质纹理 Demo：

```bash
cmake --build build --target demo_material_texture -j
./build/demo_material_texture
```

画面显示带棋盘纹理的平面。

运行 RenderTarget Demo：

```bash
cmake --build build --target demo_render_target -j
./build/demo_render_target
```

左右两个视口来自两个独立的 RenderTarget，并使用正面点光源；两个相机顺序共享同一个 Workspace。

资源加载、地形/模型材质和持续输入更新由 `project_Demo/scene_roaming` 覆盖，不在静态基础 Demo 中重复实现。

运行多边形管线 Demo：

```bash
cmake --build build --target demo_polygon_pipeline -j
./build/demo_polygon_pipeline
```

该示例使用正面点光源验证多边形管线的光照结果。

运行底层光栅化原语 Demo：

```bash
cmake --build build --target demo_raster_primitives -j
./build/demo_raster_primitives
```

画面依次展示直线方向与端点、扫描线填充的凸/凹多边形，以及裁剪窗口内的多边形填充。

运行多边形三角剖分 Demo：

```bash
cmake --build build --target demo_polygon_triangulation -j
./build/demo_polygon_triangulation
```

左侧为原始凹多边形，右侧为耳切法生成的三角网格并叠加内部边，用于检查边界、绕序和三角形拼接。

## 统一窗口与输入接口

`demo_host.h` 提供 `YMGRE_DemoHost_BindKeys`，应用只声明键与按住位、单次动作位、可选选择值的对应关系。`HeldKeys` 查询按住状态，`TakeActions` / `TakeValue` 取出并清除单次事件；`ClearInput` 清空输入。ASCII 字符键与 `YMGRE_KEY_*` 特殊键不依赖平台头文件。多个键可对应同一按住位，释放一个别名不会清除仍按下的另一个键。窗口失焦清除按住状态，平台线程同步全部在宿主内部完成。

```c
const YMGRE_DemoKeyBinding keys[] = {
    {'w', 1, 0, 0},
    {YMGRE_KEY_SPACE, 0, 1, 0}
};
YMGRE_DemoHost_BindKeys(&host, keys, 2);
unsigned held = YMGRE_DemoHost_HeldKeys(&host);
unsigned actions = YMGRE_DemoHost_TakeActions(&host);
```

宿主还提供 `Time`（单调秒）、`SetTitle`、`DefaultScale`、`LastError`。输入绑定在 `Destroy` 时自动清理，不需要应用注册平台事件观察器或使用原子变量。输入读取和每帧事件处理不分配堆内存。

SDL 实现留在 GRE 宿主与其窗口 HAL 内部；软件显示和 GLX 兼容默认值由宿主初始化。`YMGRE_HEADLESS=1` 选择无可见窗口的测试后端；`InjectKey` 和 `InjectFocusLost` 通过真实后端事件路径注入测试输入。测试 `demo_host_input` 覆盖按住、别名键、单次动作、重复按键抑制、选择值、失焦、外部窗口隔离和销毁。
