![YMGRE · C99 CPU 3D Rendering](docs/images/banner.svg)

**A CPU 3D rendering engine written in C.**

[中文介绍](README.md) · [SDK manual (Chinese)](sdk/MANUAL.md) · [Precompiled SDK](build/YMGRE_libs/) · [Demos](Demo/README.md)

![Earth texture, lighting, normal mapping and displacement](docs/images/earth.png)

YMGRE implements vertex transforms, clipping, rasterization, depth buffering, texture sampling and lighting in C99.
Ray intersection and shading APIs support demonstrations of shadows, reflection and refraction.
The core writes color and depth buffers and can be integrated with desktop windows or a platform-specific display backend.

| Reflection | Refraction |
|:---:|:---:|
| ![Mirror rendering](docs/images/advanced_raytrace_mirror.png) | ![Glass sphere rendering](docs/images/advanced_raytrace_refraction.png) |

| Perspective-correct textures | Normal mapping |
|:---:|:---:|
| ![Perspective texture sampling](docs/images/advanced_perspective.png) | ![Flat and perturbed normals](docs/images/advanced_normal_map.png) |

| Ray-traced shadows | Mesh geometry |
|:---:|:---:|
| ![Occlusion between a surface and its light](docs/images/advanced_raytrace_shadow.png) | ![Generated primitive meshes](docs/images/basic_shapes.png) |

## Edit scenes, create textures, bake assets

![Scene hierarchy, viewport, properties and model export](docs/images/scene-baker.png)

The repository's `scene_baker` application provides scene editing, raster/ray rendering selection, UV editing, texture preview and model export.

| UV layout and model preview | Layer-based texture painting |
|:---:|:---:|
| ![Imported tank UV and texture preview](docs/images/uv-tank.png) | ![Texture painting with YMGUI](docs/images/texture-paint.png) |

Move, rotate and scale UV selections, inspect checker patterns, or create a texture with the integrated YMGUI canvas. Baking can include lighting; export produces mesh, material and BMP texture files. Baked highlights retain the reference view, and the current baker does not include indirect lighting or occlusion shadows.

These screenshots are actual Demo outputs and saved editor verification captures. The title banner is vector artwork. See [capture notes](docs/images/README.md) and the [full illustrated tour in Chinese](README.md#场景编辑与贴图创作).

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

YMGRE uses a custom **noncommercial-use license with separate commercial authorization** (`LicenseRef-YMGRE-Noncommercial-1.2`). Individuals and noncommercial organizations may use it free of charge for nonprofit learning, research, or personal/internal use. Commercial use requires prior written authorization from the copyright holder.

Distributing modified versions or using modified versions to provide services to third parties requires publishing the corresponding library source and necessary build files under the license terms. Independent application logic does not have to be disclosed merely because it calls or links this library. Third-party content retains its own licenses, and rights already obtained under earlier valid licenses are not withdrawn. See [LICENSE](LICENSE) for the full Chinese terms.
