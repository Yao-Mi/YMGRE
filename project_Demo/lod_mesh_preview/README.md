# 全模型 LOD 示例

本示例加载一株草和一棵树，另由代码生成一个球面（代表地球等连续网格）。库从每个原模型一次性生成中距离和远距离网格，再组装三层**全模型** LOD。输出图从左到右是原模型、中级、远级；从上到下是草、树、球面。球面叠加线框，便于检查三角形变化。草和树使用原有贴图与材质。

```bash
cmake -S project_Demo/lod_mesh_preview -B /tmp/ymgre-lod-mesh-preview -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ymgre-lod-mesh-preview -j4
ctest --test-dir /tmp/ymgre-lod-mesh-preview --output-on-failure
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview /tmp/ymgre-lod-mesh-preview/result.bmp
```

本机示例资源的三角形数：草 `384 → 308 → 231`，树 `2937 → 2497 → 2057`，球面 `2320 → 1508 → 812`。三者现在都主要通过受约束 QEM 折边减面，草的成对表面不再被整块删去。示例检查面数严格递减、三层选择、图片文件写出；实际外观仍应在目标场景、目标距离下验收。切换时只绘制选中的一个模型，没有双模型过渡开销。

基础运行示例让两株草共享一套三级原始资源、两棵树共享另一套；四个实例各自保留层级状态和一份当前网格克隆。相机向后再向前移动，连续输出 9 帧；只有层级变化时才克隆并替换网格，切换死区由库中的 `hysteresis=0.10` 控制。加载材质后一次性构建颜色 Mip，渲染时按三角形选择采样层级。命令会打印每帧的层级、重建情况和本机 CPU 耗时：

```bash
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --basic /tmp/ymgre-lod-mesh-preview/basic
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --basic-no-mip /tmp/ymgre-lod-mesh-preview/basic-no-mip
```

第一条命令输出 `basic_00.bmp` 至 `basic_08.bmp`；第二条以相同场景关闭颜色 Mip，便于对比。草、树资源只有颜色贴图，没有 `opacity_map`；透明度 Mip 能力在库中，使用带透明图的材质时由加载接口生成。示例里的 CPU 时间仅用于观察切换峰值，本机结果不代表开发板帧率。

密集场景模式把 1600 株草、24 棵树和一颗球面放在同一片草地里，根据各实例在相机中的投影尺寸实时选择三级模型，检查草和树的近、中、远层都被实际选中，再输出场景图：

```bash
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --scene /tmp/ymgre-lod-mesh-preview/scene.bmp
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --scene-original /tmp/ymgre-lod-mesh-preview/scene-original.bmp
```

第二张图使用完全相同的实例位置与机位，但全部使用原模型，可直接比较远近层的疏密与轮廓。程序同时打印各层实例数和提交给渲染器的模型三角形数；该数值是几何负载，不等同于实测帧率。

观察单株时，推荐使用距离示例，而不是密集场景。草和树各有一张只包含三个实例的场景图，从右到左依次是近、中、远，三层都由真实相机投影选择：

```bash
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --distance-grass /tmp/ymgre-lod-mesh-preview/distance-grass.bmp
/tmp/ymgre-lod-mesh-preview/lod_mesh_preview --distance-tree /tmp/ymgre-lod-mesh-preview/distance-tree.bmp
```

也可以生成单株从近到远移动的短动画；需要 Python 和 Pillow。每帧标出当前层级、三角形数和距离，脚本会验证动画经过三级模型：

```bash
python3 project_Demo/lod_mesh_preview/make_distance_gifs.py \
  /tmp/ymgre-lod-mesh-preview/lod_mesh_preview /tmp/ymgre-lod-mesh-preview
```
