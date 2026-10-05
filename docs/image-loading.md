# BMP / PNG / JPEG 贴图加载

`YMGRE_Image_LoadRGB(path, &pixels, &width, &height)` 按文件签名读取 BMP、可选 PNG/JPEG，输出 `GRErgb24`。`YMGRE_Image_LoadGray` 输出单通道数据：RGB 图取红色通道，灰度图保持原值，与透明 BMP 的既有含义一致。成功返回 1，缓冲由调用方用 `GRE_ImageBuff_Free` 释放；失败返回 0，三个输出参数保持原值。成功不自动释放调用方原先持有的缓冲。

材质解析器的 `texture`、`normal_map`、`pbr_parameters` 和 `opacity_map` 已统一使用新接口。`YMGRE_Material_LoadOpacity` 读取灰度图并建立可选 mip；原 `YMGRE_Bmp_File_LoadTo_Image`、`YMGRE_Material_LoadOpacityBMP` 接口保留兼容。

## 模块边界

解码实现位于 `extern_lib/image_decode`，可独立构建为 `ImageDecode::image_decode`，不依赖引擎。`YMGRE/IOFILE/YMGRE_Image.c` 只负责分配器、颜色布局和材质接口适配；核心 SDK 将模块对象纳入现有静态库，不额外要求一个解码静态库。详细接口见该模块 README。所有解码只在加载资源时发生。

## 独立裁剪与依赖

源码构建默认关闭新增的两个外部解码器：

```sh
-DYMGRE_ENABLE_PNG=ON -DYMGRE_ENABLE_JPEG=ON
```

PNG 使用系统 libpng（其依赖 zlib），JPEG 使用系统 libjpeg 或兼容的 libjpeg-turbo。CMake 查找 `PNG::PNG`、`JPEG::JPEG`，自动传递静态库的链接依赖。可分别关闭，关闭后不查找对应依赖、不编译该解码器；BMP 继续可用。Linux 构建通常需要 `libpng-dev`、`libjpeg-dev`，部署需相应运行库。核心库不包含第三方解码器源码。

完整 SDK 启用两种解码器，消费者的 CMake 配置会查找依赖；最小 SDK 全部关闭。手动链接完整静态库时，除 `m` 外需要 png、jpeg、z 库。交叉编译应提供目标平台的依赖，或者裁掉解码器。解码器不在每帧渲染路径上。

## 数值与格式约定

- 支持不压缩的 8 位调色板／24 位／32 位 BMP，支持正、负高度。
- PNG 支持 RGB、灰度、调色板、alpha、低位深灰度以及 Adam7 交错。16 位样本取高 8 位。不会根据 gAMA、sRGB 或 ICC 元数据做颜色变换，避免改变法线、粗糙度、透明度等数据；颜色空间处理仍由渲染器负责。
- JPEG 支持常规／渐进 RGB、YCbCr 和灰度图；不接受 CMYK/YCCK。不会应用 EXIF 方向或 ICC；贴图导出时应固定方向。JPEG 的 YCbCr 到 RGB 是正常图像解码的一部分。
- 内嵌 alpha 不作为单独透明图；RGB 加载忽略它，灰度加载仍取红色通道。透明图继续通过 `opacity_map` 显式指定。
- 尺寸最多 65535×65535，单个输出缓冲默认最多 512 MiB，可在编译库时覆盖 `IMAGE_DECODE_MAX_BYTES`。交错 PNG 需要额外 RGB 暂存；普通单通道 PNG 只需一行 RGB 暂存。
- 解码错误、截断及不支持格式返回失败，不退出进程；BMP 原始接口仍保留其旧错误处理方式。

## Girl 示例

颜色主要使用 JPEG 90，法线／MRS 使用 JPEG 95，均为 4:4:4；两张更小的 MRS 和所有透明遮罩保留 PNG。资源直接位于 `Resource/girl`，没有分卷包和运行时 Python。未降低分辨率，整套约 41.6 MiB。JPEG 颜色和数据图存在少量有损误差，透明图保持无损。运行内存仍是原来的 RGB／灰度缓冲，不会按磁盘压缩比例减少。入口为 `project_Demo/girl_viewer`，构建输出 `build/bin/rgb888/index32/girl_viewer`。
