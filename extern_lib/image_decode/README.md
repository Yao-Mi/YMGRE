# image_decode

独立 C99 图片解码模块，不依赖 YMGRE、YMGUI 或渲染接口。BMP 解码实现随模块提供；PNG 引用系统 libpng（及 zlib），JPEG 引用系统 libjpeg / libjpeg-turbo，不在此目录复制第三方源码。

```sh
cmake -S extern_lib/image_decode -B build/image-decode \
  -DIMAGE_DECODE_ENABLE_PNG=ON -DIMAGE_DECODE_ENABLE_JPEG=ON
cmake --build build/image-decode
```

其他项目通过 `add_subdirectory` 引用并链接 `ImageDecode::image_decode`。两个解码开关默认 OFF，分别开启时才查找对应库；只用 BMP 时无第三方依赖。`IMAGE_DECODE_MAX_BYTES` 设置单个解码缓冲的上限，默认 512 MiB。

```c
#include "image_decode.h"
#include <stdlib.h>
uint8_t *pixels = NULL;
uint16_t width = 0, height = 0;
if (image_decode_load("texture.jpg", 3, NULL, &pixels, &width, &height)) {
    /* 使用 width * height * 3 个 RGB 字节。 */
    free(pixels);
}
```

通道数 3 输出紧密 RGB，1 输出红色通道（灰度输入保持原值）。按文件签名识别格式，图像首行为顶部，不转换 gamma / ICC / EXIF，不返回内嵌 alpha。透明图应作为单通道图片单独加载。JPEG 是有损格式，不建议用于透明、法线或材质参数图。

成功返回 1，调用者负责释放；失败返回 0，输出参数保持不变。可通过 `ImageDecodeAllocator` 为输出和模块临时缓冲提供分配/释放函数；NULL 使用 malloc/free。回调需允许释放 NULL；libpng/libjpeg 的内部缓冲仍由相应库管理。分配器按调用传入，无全局分配器状态。

YMGRE 用 `YMGRE/IOFILE/YMGRE_Image.c` 做薄适配，使用引擎图片分配器，并转换引擎颜色布局。CMake 将解码对象纳入 `libymgre.a`，SDK 无需额外携带本模块头文件或另一个静态库；启用 PNG/JPEG 时仍需链接对应系统依赖。解码和颜色布局转换只在加载时执行，不进入逐帧渲染路径。
