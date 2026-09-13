# README 效果图来源

所有图片均由本仓库 Demo 实际运行后，通过 YMGUI 显示层的 `YMGUI_SHOT` 接口保存。
图像只从 BMP 转码为 PNG，未裁剪、重绘、修饰光照或生成替代效果。

| 文件 | Demo | 捕获配置 |
|---|---|---|
| `earth.png` | `demo_advanced_earth_normal_map` | RGB888，32 位索引，仓库 Resource 地球纹理，初始画面 |
| `advanced_perspective.png` | `demo_advanced_perspective` | RGB565，16 位索引 |
| `advanced_normal_map.png` | `demo_advanced_normal_map` | RGB565，16 位索引 |
| `advanced_raytrace_mirror.png` | `demo_advanced_raytrace_mirror` | RGB565，16 位索引 |
| `advanced_raytrace_refraction.png` | `demo_advanced_raytrace_refraction` | RGB565，16 位索引，光标固定在窗口中心 |

地球 Demo 是仓库级展示，需要 `Resource/` 中的纹理，不包含在 SDK 的 30 个独立示例里。
其余四幅来自仅链接 YMGRE 静态库、外部 YMGUI 测试依赖的窗口示例。

复现截图需要 Pillow、C 编译器和已编译窗口 Demo；这些不是 YMGRE SDK 默认构建依赖：

```sh
python3 docs/capture_demos.py \
  --demo-dir /path/to/window-demo/bin \
  --earth-demo /path/to/demo_advanced_earth_normal_map
```

也可以单独设置截图环境变量：

```sh
YMGUI_SHOT=/tmp/perspective.bmp YMGRE_HEADLESS=1 \
YMGRE_MAX_FRAMES=3 YMGRE_WINDOW_SCALE=1 \
/path/to/demo_advanced_perspective
```

动态 Demo 必须运行到实际完成绘制；玻璃 Demo 根据光标位置移动球体，脚本固定其输入以保证截图可重复。
SDK 内的相同 PNG 由 `sdk/package.py` 复制，不引入 YMGUI 库或头文件。
