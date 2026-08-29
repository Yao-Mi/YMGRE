# YMGRE

#### 介绍
C语言从零实现跨平台渲染引擎

#### 软件架构
软件架构说明


#### 安装教程

1.  xxxx
2.  xxxx
3.  xxxx

#### 使用说明

1.  xxxx
2.  xxxx
3.  xxxx

#### Demo 覆盖

基础流程 Demo 按单一职责覆盖以下层次：

| 类别 | Demo | 观察目标 |
|---|---|---|
| 网格 | `demo_basic_shapes`、`demo_extended_shapes` | 基础和扩展网格生成 |
| 光栅 | `demo_polygon_fill`、`demo_raster_primitives` | 线、三角形、多边形填充 |
| 稳定性 | `demo_fragment_stitching`、`demo_shared_edge_stress`、`demo_degenerate_geometry` | 共享边、裂缝和退化输入 |
| 深度/裁剪 | `demo_depth_overlap`、`demo_frustum_clipping`、`demo_window_clipping`、`demo_backface_culling` | Z-buffer 和各级剔除裁剪 |
| 材质/光照 | `demo_material_texture`、`demo_lighting` | UV 纹理及环境光、点光、聚光 |
| 相机/输出 | `demo_camera_viewport`、`demo_stereo_view`、`demo_render_target` | 视口、多相机和输出目标 |
| 拓扑/管线 | `demo_polygon_triangulation`、`demo_polygon_pipeline` | 三角化和多边形主管线 |

高级顶点流程使用独立的小型 Demo：

分阶段画面、数值公式、容差和失败判据见 [`tests/README.md`](tests/README.md#高级顶点流程验收标准)。
这些 Demo 面向 PC 开发、差分和性能评估；MCU 产品路径继续使用原有基础管线，不要求运行高级流程。

| Demo | 观察目标 |
|---|---|
| `demo_advanced_vertex` | 单个 RGB 三角形，验证 `GRErgb24` 顶点颜色插值 |
| `demo_advanced_stages` | 两排八个大三角形，逐项验证高级顶点材质流程 |
| `demo_advanced_point_light` | 两排六个大三角形，逐项验证点光位置、入射角、衰减、棋盘和顶点颜色 |
| `demo_advanced_perspective` | 大角度倾斜棋盘，验证透视校正 UV 和深度 |
| `demo_advanced_perspective_verify` | 倾斜棋盘加固定采样点，输出仿射与透视 UV 数值对照 |
| `demo_advanced_clipping` | 近裁剪面穿越三角形，观察裁剪后四边形和顶点颜色连续性 |
| `demo_advanced_clipping_planes` | 两排六格，验证近远左右上下六个视锥面的高级属性裁剪 |
| `demo_advanced_depth` | 两种提交顺序的重叠高级平面，验证深度结果一致 |
| `demo_advanced_cube` | 高级立方体，观察多面硬边、共享顶点和逐面光照 |
| `demo_advanced_lighting` | 三格高级光照：环境光、高光点光、彩色双点光源 |
| `demo_advanced_specular` | 三格高光专项：关闭高光、开启高光、移动光源 |
| `demo_advanced_spot_light` | 三格聚光灯专项：点光基准、正向聚光、改变聚光方向 |
| `demo_advanced_material_channels` | 三格材质通道专项：环境色、漫反射色、镜面色独立验证 |
| `demo_advanced_specular_texture` | 三格黑白棋盘，验证镜面高光不被颜色纹理或顶点色吞掉 |
| `demo_advanced_gloss` | 三格对比高光指数 8/30/96，验证材质高光由宽变窄 |
| `demo_advanced_spot_specular` | 三格验证聚光灯镜面高光受锥角约束并跟随灯位 |
| `demo_advanced_light_accumulation` | 红光、蓝光及红蓝同时开启，验证多光源逐像素加法 |
| `demo_advanced_light_order` | 交换红蓝灯提交顺序并倍增灯光，验证顺序无关和饱和钳位 |
| `demo_advanced_backlight` | 正面光、无背面补光、有背面补光三格验证 `shadowK` |
| `demo_advanced_normal_specular` | 关闭高光、平坦法线高光、扰动法线高光三格对照 |
| `demo_advanced_specular_sampling` | 粗顶点、逐片元、细分顶点三格验证高光采样频率 |
| `demo_advanced_render_modes` | Face/Vertex/Pixel 三种高级渲染模式对照 |
| `demo_advanced_normals` | 同一低面数圆柱左右对照，验证逐面和顶点法线光照差异 |
| `demo_advanced_normal_map` | 两块正视平面左右对照，验证切线生成、TBN 和法线贴图 |
| `demo_advanced_normal_map_detail` | 三块大平面，验证平坦/扰动法线贴图及混合 `tangentW` 的受光细节 |
| `demo_advanced_mirror_uv` | 左右对照镜像 U，验证切线手性与法线方向整体镜像 |

构建后可以分别运行，例如：

```bash
./build/demo_advanced_vertex
./build/demo_advanced_point_light
./build/demo_advanced_perspective
./build/demo_advanced_normals
./build/demo_advanced_normal_map
```

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
