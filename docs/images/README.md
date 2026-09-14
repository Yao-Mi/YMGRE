# README 图片来源与复现

首页包含 17 张实际运行画面和 1 张标题封面。`banner.svg` 是仓库内编写的矢量标题设计，其中的立方体是装饰图形；其余 PNG 均来自 YMGRE Demo 或 scene_baker 编辑器的真实输出。
新捕获的画面仅从 BMP 转码为 PNG；编辑器图片直接复制已有验收截图。没有重绘、修饰光照或合成替代渲染效果。编辑器截图记录对应验收时的界面，后续版本的控件位置可能变化。

## 渲染 Demo

| 文件 | Demo | 捕获配置 |
|---|---|---|
| `earth.png` | `demo_advanced_earth_normal_map` | RGB888，32 位索引，仓库地球纹理，初始画面 |
| `advanced_perspective.png` | `demo_advanced_perspective` | RGB565，16 位索引 |
| `advanced_normal_map.png` | `demo_advanced_normal_map` | RGB565，16 位索引 |
| `advanced_raytrace_mirror.png` | `demo_advanced_raytrace_mirror` | RGB565，16 位索引 |
| `advanced_raytrace_refraction.png` | `demo_advanced_raytrace_refraction` | RGB565，16 位索引，光标固定在窗口中心 |
| `advanced_raytrace_shadow.png` | `demo_advanced_raytrace_shadow` | RGB565，16 位索引，运行 3 帧 |
| `basic_shapes.png` | `demo_basic_shapes` | RGB565，16 位索引，运行 3 帧 |
| `extended_shapes.png` | `demo_extended_shapes` | RGB565，16 位索引，运行 3 帧 |

除地球 Demo 外，上述截图来自仅链接 YMGRE 静态库、外部 YMGUI 测试依赖的窗口示例。地球展示需要仓库 `Resource/` 纹理，不包含在 SDK 的 30 个独立示例里。

## 编辑器验收截图

下面的源路径相对于仓库 `baked_scene/`。该目录属于本地验收输出，因此把用于首页的图片单独收录到 `docs/images/`；克隆仓库后无需生成验收文件即可查看首页。

| 首页文件 | 已有验收图片 | 展示内容 |
|---|---|---|
| `scene-baker.png` | `export_preview_fix/editor.png` | 场景层级、视口、检查器与统一导出按钮 |
| `uv-tank.png` | `automatic_uv/tank-material-preview.png` | 坦克头原始 UV、贴图与模型预览 |
| `uv-capsule.png` | `primitive_uv/capsule.png` | 胶囊自动展开与棋盘格 |
| `texture-paint.png` | `texture_tools/paint-editor.png` | YMGUI 图层画板 |
| `texture-preview.png` | `texture_tools/paint-model-preview.png` | 画板图案在 UV 与模型上的对应关系 |
| `bake-before.png` | `export_preview_fix/before-bake.png` | 烘焙前的材质和实时光照 |
| `bake-no-lights.png` | `export_preview_fix/no-lights-preview.png` | 导出模型在无灯光场景中的预览 |
| `sphere-mesh.png` | `smooth_sphere/mesh.png` | 三角网格球体的光追结果 |
| `sphere-analytic.png` | `smooth_sphere/analytic.png` | 解析球面的光追结果 |

源资源及其许可仍按各自原有说明使用，图片不会改变第三方模型和纹理的授权。

## 复现与更新

需要 Pillow、C 编译器和已经编译的窗口 Demo；这些不是 YMGRE SDK 默认构建依赖：

```sh
python3 docs/capture_demos.py \
  --demo-dir /path/to/window-demo/bin \
  --earth-demo /path/to/demo_advanced_earth_normal_map
```

如需同时收录已有编辑器验收图片，追加 `--editor-captures-dir /path/to/baked_scene`。脚本只复制这些图片，不启动编辑器或修改场景。新环境没有验收截图时，省略此选项即可。

也可以单独捕获：

```sh
YMGUI_SHOT=/tmp/shadow.bmp YMGRE_HEADLESS=1 \
YMGRE_MAX_FRAMES=3 YMGRE_WINDOW_SCALE=1 \
/path/to/demo_advanced_raytrace_shadow
```

动态 Demo 必须运行到完成实际绘制；玻璃 Demo 根据光标位置移动球体，脚本固定其输入以保证截图可重复。
`sdk/package.py` 自动携带封面和 7 张独立 Demo 截图。编辑器画面与地球图留在源码仓库；SDK 不引入 YMGUI 的库或头文件。
