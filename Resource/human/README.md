# GRE 人体资源

来源：Rigged Base Mesh，作者 Eliber。原包内声明为 CC BY 3.0，原授权保存在 LICENSE.html，使用时保留作者署名。来源链接保留在授权文件中。

## 加载

将 human.mesh 与 human.material 放在同一目录，使用 GRE 的 YMGRE_LoadOgreMeshAndMaterial(scene, absolute_mesh_path) 加载 human.mesh。

- human.mesh：GRE 支持的 Ogre v1.8 二进制子集，绑定姿态。
- human.material：原模型灰色材质；源文件没有图像贴图。
- human.ske：YMSKE v1 二进制骨架、绑定顶点、法线和完整六槽归一化权重。
- human.ska：YMSKE v1 单帧绑定姿态，帧率 1、非循环；源文件没有动画。
- human.ske.txt / human.ska.txt：对应可读文本版本，同样可由 YMSKE_IO_Read 读取。
- human.ske.json：保留的中间数据备份。
- manifest.json：数量、包围盒和网格 SHA-256。
- export_blender.py：本次离线导出脚本。

## 数据

原网格 1,797 顶点，导出 7,168 顶点、3,584 三角面、63 根骨骼。顶点增加来自原有面角法线及 UV 边界拆分，没有细分网格。只导出 Base mesh；不导出相机、灯光与骨架控制形状。

坐标从 Blender (x,y,z) 转为 GRE (x,z,-y)，保留原始比例及原点；Y 向上。UV 的 V 已翻转。源文件没有动画片段。

JSON 的 bind_local 为行主序 3×4 仿射矩阵，使用列向量；parents=-1 表示根。vertices 的顺序与 GRE 网格完全一致；original_weights 保留全部原始非零权重，weights 提供归一化的前四项。部分顶点截为四项会丢弃最多约 8.02% 权重，后续接入时应按需要选用完整权重或重新拟合。骨架控制约束没有转换成运行时求解器。

## 验证

已通过 GRE 原生加载器实际读取，逐顶点检查位置、法线、UV，逐三角形检查索引并核对材质绑定。交互演示见 ../../project_Demo/human/README.md，可运行 ../../build-human/demo_human。

## SKE/SKA 加载配置

启用 `YMSKE_BUILD_IO=ON`，并将 `YMSKE_MAX_INFLUENCES` 设置为 6（或更大，最高 8）。库及调用方必须使用一致配置。默认四槽构建无法加载此六槽 SKE 文件。

`YMSKE_IO_Probe` 获取容量，调用方提供数组后用 `YMSKE_IO_Read` 加载，再用 `YMSKE_IO_Match` 检查 SKE/SKA 配对。meshId 是 human.mesh 全文件的 FNV-1a 64 指纹。SKE 顶点顺序与 GRE 网格一致。

SKE 从 JSON 的 original_weights 生成，保留全部非零影响并归一化，没有截断为四项。SKA 当前只有绑定姿态，播放器应直接使用这一帧，不能按多帧循环访问下一帧。

二进制及文本版本均通过原生 Save/Read 往返、逐字段/逐浮点位比较，以及加载数据与源数组的蒙皮结果逐位比较；报告见 ske-verification.log。
