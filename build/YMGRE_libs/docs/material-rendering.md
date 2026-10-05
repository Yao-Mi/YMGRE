# 可裁剪材质渲染

高级 `_wN` 光栅化管线新增单通道透明、按材质双面、逐像素 GGX 材质及线性色彩输出。旧材质默认单面、不透明，PBR 和线性色彩需要相机显式开启。原有 Blinn-Phong、顶点光照、光线追踪 API 保留。

## 编译裁剪

五个独立 CMake 选项默认 ON（编译能力；不代表运行时自动启用）：

| 选项 | 关闭后移除 |
|---|---|
| `YMGRE_ENABLE_TRANSPARENCY` | 单通道透明图、透明三角形排序和相关材质／工作区字段 |
| `YMGRE_ENABLE_OPACITY_MIPMAP` | 透明图 Mip 生成、采样层级和 Mip 字段；依赖透明模块 |
| `YMGRE_ENABLE_PBR` | GGX 光照、MRS 参数图与相关字段 |
| `YMGRE_ENABLE_RASTER_DISPATCH` | 可选分带并行调度接口、工作区调度字段；依赖 PBR 和透明模块 |
| `YMGRE_ENABLE_LINEAR_COLOR` | sRGB LUT、浮点颜色缓冲、曝光和输出色调映射 |

基础 MCU 示例：

```sh
cmake -S . -B build/minimal -DYMGRE_BUILD_DEMOS=OFF \
  -DYMGRE_BUILD_YMGUI_HOST=OFF -DYMGRE_INDEX_BITS=16 \
  -DYMGRE_CAMERA_COLOR_DEPTH=16 \
  -DYMGRE_ENABLE_TRANSPARENCY=OFF -DYMGRE_ENABLE_OPACITY_MIPMAP=OFF \
  -DYMGRE_ENABLE_PBR=OFF -DYMGRE_ENABLE_LINEAR_COLOR=OFF -DYMGRE_ENABLE_RASTER_DISPATCH=OFF
cmake --build build/minimal
```

关闭透明时，首次配置的 Mip 默认值随之关闭；复用旧构建目录时需显式关闭已经缓存的 Mip 开关。不使用 CMake 时，可在整个库及应用的编译命令中一致定义这些宏为 0 或 1。

`./sdk/build.sh` 生成全功能 SDK，`./sdk/build.sh --minimal` 生成上述五项全部关闭的 SDK，输出均为 `build/YMGRE_libs`，后一次成功构建会替换前一次。SDK 附带功能配置并向 CMake 消费者传递宏；不匹配的功能配置会被拒绝。

**这些开关涉及公共结构体布局，必须一起更新库和头文件，并重新编译调用方。不能混用旧头文件、不同裁剪配置或不同 Index/颜色深度的二进制。**

## 材质文件

```text
material cloth
{
 technique
 {
  pass
  {
   diffuse 1 1 1
   cull_hardware none
   pbr_ior 1.45
   normal_strength 0.6
   pbr_parameters cloth_mrs.bmp
   texture_unit
   {
    texture cloth_color.bmp
    normal_map cloth_normal.bmp
    opacity_map cloth_opacity.bmp
   }
  }
 }
}
```

- `cull_hardware none` 为双面；`clockwise` 为原来的单面剔除。双面只改变剔除与背面法线，不复制网格。省略时单面。
- `opacity_map` 使用原有 UV0。支持无压缩 8 位调色板、24 位 RGB、32 位 RGB BMP；使用红色／灰度通道，32 位输入的 alpha 字节不参与。内存与 Mip 均为每像素 1 字节。0 透明，255 不透明，中间值半透明。
- `pbr_parameters` 的 R/G/B 分别为金属度、粗糙度、高光 IOR level；都是线性数据。加载该图会启用该材质的 PBR 标志。没有参数图时，可通过 API 启用 PBR，默认参数为 0 / 0.5 / 0.5。
- `pbr_ior` 范围 1～3，默认 1.45；`normal_strength` 范围 0～4，默认 1。
- 颜色贴图可乘 `diffuse` 因子和顶点颜色。透明、法线、MRS 不执行 sRGB 变换。
- 材质调色、顶点／纹理透明权重、人物专用补发算法均未并入。

程序生成透明图可用 `YMGRE_Material_SetOpacity(material, pixels, width, height, mipmaps)`。函数复制单通道输入，调用者仍持有原输入；材质拥有内部副本和 Mip，替换或 `YMGRE_Free_Material` 时释放。BMP 加载也有可检查返回值的 `YMGRE_Material_LoadOpacityBMP`。需要高级字段时用 `YMGRE_Material_EnsureAdvanced` 初始化。

## 启用与内存

```c
camera->pbrEnabled = 1;
camera->pbrNormalEnabled = 1;
camera->linearColorEnabled = 1;
camera->exposure = 1.0f;
object->renderMode = GRE_RenderMode_Pixel;
YMGRE_Camera_TanglePipline_wN(camera, lights, objects, materials, workspace);
if (workspace->materialStatus != 0) { /* 处理工作缓存不足 */ }
```

相机设置属于每台相机；缓存属于 `GRE_RenderWorkspace`，没有实验版全局开关。多相机可顺序共享工作区；并发渲染应各用独立工作区。只有首次使用或容量增加时分配，之后复用。未启用线性色彩不分配 HDR；未启用并行调度且没有透明图时不分配队列。

HDR 为每像素 3 个 float，即 RGB888 1200×800 时约 11 MiB。透明三角形缓冲按实际裁剪后的数量扩容。`YMGRE_Free_RenderWorkspace` 释放其拥有的缓存。固定工作区不会偷偷分配，可用 `YMGRE_RenderWorkspace_BindMaterialBuffers` 绑定调用方的 float 数组及对齐的三角形缓存；`YMGRE_Material_TransparentPacketSize()` 返回每条缓存所需字节。容量不足时 `materialStatus=-1`，本帧不可作为完整结果使用。

透明材质在无透明材质之后处理：先建立 alpha=255 部分的深度，再对所有半透明三角形按深度从远到近混合。半透明部分测试深度而不写深度。它是三角形排序，不是逐像素无序透明；任意相交半透明几何仍存在排序算法固有的限制。

## 色彩和性能

PBR 在浮点线性空间计算光照和透明混合，最后乘曝光并进行固定色调映射／sRGB 输出。输入表 256 个 float（1 KiB），RGB 共用；合并输出表 65537 个字节（约 64 KiB），不逐像素调用 pow。表是只读常量，不在运行时初始化，没有共享可变 LUT 状态。曝光更改不重建 LUT。

关闭相机线性色彩时，PBR 使用显示 RGB 直接计算、截断输出。旧 Blinn-Phong／顶点光照仍保留原算法；与线性输出混用时，仅把其已计算的 RGB 结果解码后混合，并不会自动升级为浮点线性光照。

优化包括：透明 LOD 每三角形计算一次；单通道采样；工作区缓存复用与预留索引数组上的透明排序（不反复搬动整条三角形记录）；PBR 按扫描行收紧三角形范围，避免细长发片包围盒中的大量空白像素；保留原有重心覆盖及深度判断。没有降低分辨率、减少几何或删减透明层。

详细验证和同场景计时见 [验证记录](material-rendering-validation.md)。

## 可选多核调度

`YMGRE_RenderWorkspace_SetRasterDispatcher(workspace, dispatch, user)` 绑定调用方的同步调度器。默认 NULL，保留单线程路径。回调签名为 `dispatch(user, job, context, count)`，必须对每个 `[0,count)` 索引恰好执行一次 `job(context,index)`，并在全部工作完成后才返回。可按任意顺序并行执行；禁止在未完成时返回、改变场景／材质／灯光或重入同一工作区。调用方拥有调度器及线程池，其生命期需覆盖绑定期间。库不创建线程，不链接 pthread/OpenMP/SDL。

当前加速用于高级管线中全部可见实体均采用 PBR、无光照贴图及线框的场景；混合旧着色模式自动回退原单线程路径。每个任务独占 16 行像素，不使用像素锁。几何变换与裁剪只做一次，不透明和透明像素保持原来的提交、深度及混合次序，任务之间只读共享数据。颜色 LUT 直接只读查表，避免每通道函数调用。

启用调度后需缓存全部裁剪后的三角形，固定工作区应按这个数量而非仅透明数量预留对齐的 packet 缓冲；尺寸（包含排序索引）继续通过 `YMGRE_Material_TransparentPacketSize()` 查询。动态工作区按需扩容后复用。关闭 `YMGRE_ENABLE_RASTER_DISPATCH` 可以保留全部材质能力并移除调度；关闭 PBR 或透明时也应关闭已缓存的调度选项。最小 SDK 自动关闭全部五项。

线程池参考实现见 demo 的 `raster_pool.c`。它一次处理一个同步批次，不支持多个相机同时复用同一个池；并发相机应使用不同池和工作区。

图片解码另有 `YMGRE_ENABLE_PNG`、`YMGRE_ENABLE_JPEG` 两个独立开关，源码默认关闭；完整 SDK 开启、最小 SDK 关闭。材质图可直接引用 `.png` / `.jpg`，数值与依赖约定见 [图片加载](image-loading.md)。
