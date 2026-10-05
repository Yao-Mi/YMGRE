# Girl 渲染示例资源

供 `project_Demo/girl_viewer` 使用，直接加载，无需解压或 Python。包含 11 组静态网格及材质，不依赖 YMSKE。

- `.mesh`：GRE 可读取的 Ogre 网格。
- `.material`：GRE 材质脚本，贴图路径相对于本目录。
- `*_color.jpg`：颜色贴图，主要使用 JPEG 质量 90、4:4:4，从原始 BMP 重新编码；个别已有更小文件保留。分辨率不变。
- `*_normal.jpg`、`*_mrs.jpg`：法线与材质参数图，JPEG 质量 95、4:4:4，允许少量有损误差；加载时不做 gamma 或 ICC 颜色变换。
- `*_opacity*.png`：8 位单通道透明图，无损保留，避免改变发丝、眉毛和衣物遮罩边缘。
- `eyesbrow_mrs.png`、`hair_mrs.png`：PNG 比 JPG 更小，继续保留。
- `.pbr`：demo 使用的 IOR、法线强度等参数。
- `hair_alpha_edited.material`：引用已接受的修改后头发透明图。

整套资源约 41.6 MiB，比前一版 JPG95 + PNG 的 70.5 MiB 减少约 41%。共 29 张 JPG、8 张 PNG，未缩小分辨率，透明图像素保持原样。JPEG 并非无损；已比较半身、脸部近景、全身和不同渲染模式，未观察到明显外观变化。

需要启用库的 `YMGRE_ENABLE_PNG` 和 `YMGRE_ENABLE_JPEG`。只在加载时解码，渲染仍读取内存中的 RGB／灰度像素；磁盘减少不等于运行内存或每帧计算量等比例降低。

资源来自用户提供的人物模型，不改变原有授权。未包含 Blender 源文件、骨骼动画、废弃的贴图调色及权重实验。可在本目录运行 `sha256sum -c checksums.sha256` 校验当前 71 个资源文件。使用方式见 `../../project_Demo/girl_viewer/README.md`。
