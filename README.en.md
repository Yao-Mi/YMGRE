# YMGRE

**A CPU 3D rendering engine written in C.**

[中文介绍](README.md) · [SDK manual (Chinese)](sdk/MANUAL.md) · [Precompiled SDK](build/YMGRE_libs/) · [Demos](Demo/README.md)

![Earth texture, lighting, normal mapping and displacement](docs/images/earth.png)

YMGRE implements vertex transforms, clipping, rasterization, depth buffering, texture sampling and lighting in C99.
Ray intersection and shading APIs support demonstrations of shadows, reflection and refraction.
The core writes color and depth buffers and can be integrated with desktop windows or a platform-specific display backend.

| Reflection | Refraction |
|:---:|:---:|
| ![Mirror rendering](docs/images/advanced_raytrace_mirror.png) | ![Glass sphere rendering](docs/images/advanced_raytrace_refraction.png) |

These are actual Demo captures, not generated illustrations. See [capture notes](docs/images/README.md).

## Use the SDK

The SDK includes two static core libraries, public headers, a manual and 30 example sources.
The current binaries target **Linux x86_64, Release, RGB565**. `GRE_Index` is configurable as `uint16` or `uint32`.
MCU targets require a separate build with the appropriate toolchain and ABI.

From the downloaded SDK directory:

```sh
./build_demos.sh 16
./examples-build16/bin/demo_sdk_minimal
```

The default example writes `cube.ppm` without requiring engine sources, YMGUI or SDL.
For your own CMake project, use `find_package(YMGRE CONFIG REQUIRED)` and link `YMGRE::ymgre`.
Pass `-DYMGRE_DIR=/absolute/path/YMGRE_libs/cmake` and `-DYMGRE_INDEX_BITS=16` or `32` when configuring.

## Windowed examples

YMGUI is an external SDK, distributed separately. YMGRE does not bundle its headers, archives or statically linked window binaries.
With a compatible external YMGUI package:

```sh
./build_demos.sh 16 /absolute/path/YMGUI_libs/cmake
./examples-build16/bin/demo_basic_shapes
```

The expected CMake interface and pixel configuration are documented in the SDK manual.

## Build a release

From this repository:

```sh
./sdk/build.sh
```

This builds both core variants, verifies standalone consumers and produces `build/YMGRE_libs/`, a `.tar.gz` archive and a SHA256 file.
Add `--ymgui-dir /path/to/YMGUI/cmake` to validate windowed examples against an external package.

The core includes geometry, rasterization, material/lighting, ray tracing functions and scene/resource APIs.
The scene editor, UV tools, painting and baking generators are application-layer projects; they are not part of these core archives.
See [project applications](project_Demo/README.md) and [test criteria](tests/README.md).

## License

[Apache License 2.0](LICENSE). External dependencies are maintained and licensed by their respective projects.
