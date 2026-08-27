# scene_editor

场景编辑器是 YMGRE 的系统级验收 ProjectDemo，用来同时验证网格生成、对象/灯光/相机生命周期、
实时场景编辑、RenderTarget、3D 辅助线以及 YMGUI 输入和控件组合。

## 开发记录（2026-08-27）

- 完成场景编辑闭环：新建、打开、保存、未保存确认、退出保护和资源错误弹窗；
- 接入 Mini-XML 的 Ogre/DotScene 风格 XML，并支持 Blender 导出插件；
- 增加缺失网格、材质、材质引用和贴图的具体错误提示，以及可选的错误物体过滤；
- 增加 24 级撤销/重做、对象克隆和 `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` 快捷键；
- 增加移动、旋转、缩放模式，支持二次点击取消约束；移动轴和三轴旋转参考环采用动态 Stack 菜单与加宽命中区域；
- 层级右键菜单改用 `YMGUI_Layout_Stack`，根据对象类型动态排列重命名、删除、切换相机和克隆命令；
- 正式构建目录和无头自测均通过：`build/project_Demo/scene_editor/scene_editor`。

## 开发记录（2026-08-28）

- Gizmo 尺寸改为以导入/创建时的基础包围盒为基准，默认为 `1.3` 倍，并随对象缩放同步变化；旋转和移动不会因相机距离改变世界尺寸；
- 移动轴、缩放轴和旋转环使用加粗 3D 线段；旋转环增加多层旋转扇形和灰色原始参考轴；
- 三轴旋转拖拽改为鼠标射线与对应世界旋转平面求交，再计算平面内有符号角度；旋转后同步刷新多边形法向，避免立方体出现空片元；
- 检查器增加 `Rx`、`Ry`、`Rz` 输入和步进按钮，三轴旋转可直接编辑；
- 旋转参考扇形与对象实际绝对旋转值同步，移除临时白色投影调试标记；
- 线段图元增加可选线宽，普通网格线保持单像素，Gizmo 使用更粗线宽；
- 已重新编译并通过无头功能自测。

## 当前功能

- 中文顶部工具栏、场景层级、检查器、状态栏和中央视口；
- 编辑菜单可打开/保存 Ogre DotScene 风格的 `.scene` XML、切换左右侧栏、复位视图，并控制
  坐标轴和参考网格的显示，默认开启；
- 新建、打开其他场景或退出程序时，如果当前场景有未保存修改，会统一询问保存、不保存或取消；
  保存失败或取消文件选择时不会丢弃当前场景；
- 编辑菜单提供 24 级撤销/重做；场景层级右键菜单提供“克隆”选中对象；视口聚焦时可用 `Ctrl+Z` 撤销，
  `Ctrl+Shift+Z` 或 `Ctrl+Y` 重做，历史恢复沿用场景序列化和资源校验链路；
- 场景文件使用 DotScene 常用的 `environment/nodes/node/entity/light/camera` 结构，由仓库内置的 Mini-XML 4.0.5 读写，保存基本网格参数、导入网格的相对资源路径、灯光、相机、当前视口相机和可编辑属性；
- 打开场景时会先校验文件并在临时场景中创建全部资源，成功后才替换当前场景；
- 收起侧栏后，视口、Image、鼠标命中层和 YMGRE RenderTarget 自动占用释放出的宽度；
- YMGRE 实时渲染空场景参考网格，以及红色 X、绿色 Y、蓝色 Z 三轴；
- 网格和坐标轴使用独立 3D 线段图元，不是三角平面线框，从网格下方观察仍然可见；
- 左键点中对象会选中并在相机视平面内拖动物体；左击空白区域取消选择，继续拖动则平移视图；
- 右键拖动在有选中对象时绕对象旋转，否则绕世界原点旋转；滚轮连续缩放，编辑 / 重置视图
  恢复初始视角；
- 顶部提供可切换的移动、旋转、缩放三种变换模式，再次点击当前按钮可取消约束并恢复自由移动；
  移动操作轴按红色 X、绿色 Y、蓝色 Z 约束位置，三轴旋转环分别绕 X/Y/Z 旋转，灰色原始坐标轴
  与彩色当前旋转参考环同时显示，缩放操作轴执行统一缩放；轴线命中区域加宽，一次拖动对应一次撤销记录；
- 视口按投影中心和包围半径拾取对象；选中网格显示橙色世界空间包围盒，灯光与相机显示
  橙色三轴十字标记；
- 选中聚光灯显示由灯位、照射目标和实际内锥角生成的橙色线框光锥，并显示灯位到目标的中心线
  与目标十字；选中相机显示机位外框、注视线和按真实横纵视场生成的视锥外框；
- 放置菜单支持平面、立方体、长方体、球体、圆柱、圆锥、圆环、胶囊、点光源、聚光灯和相机；
- 平面支持行/列细分，并和其他曲面一样可在创建后通过检查器重新生成网格；
- 球体可设置纬向/经向细分，圆柱与圆锥可设置径向细分，圆环可设置主环/管截面细分，
  胶囊可设置半球/经向细分；创建后仍可在检查器修改这些属性并重新生成实际 YMGRE 网格；
- 放置网格的 Y 表示底面高度，默认 `0`；编辑器按照实际最低网格顶点和初始 scale 自动计算
  对象中心，所有基本形状在不同缩放下均以底面贴合参考平面；
- 新物体默认实体叠加线框，并立即加入 YMGRE 对象列表和场景树；
- 场景树右键菜单支持重命名和删除，点击菜单外空白处可取消；相机节点还可切换为当前视口相机，
  主相机禁止删除；编辑器启动和对象删除后默认不选中任何对象；
- 检查器的 X/Y/Z 使用与场景坐标轴一致的红/绿/蓝，位置和缩放均支持前后步进按钮及
  手动输入；短按步进一次，按住 400 ms 后每 80 ms 连续步进；修改会同步到真实网格顶点、
  世界坐标和包围体使用的 scale；
- 检查器与放置弹窗共用 YMGUI ColorPicker；颜色修改会更新多边形材质色或实际 Light 颜色，
  `显示线框`复选框直接更新 Object 的 `wireFrame`；默认全局环境光强度为 0.2，给点光和聚光
  保留可见的颜色叠加范围；
- 检查器按对象类型显示属性：网格显示位置/缩放/材质，灯光显示实际支持的颜色/强度/目标，
  相机显示位置与注视目标；修改全局光照颜色会直接更新参与渲染的 YMGRE Light；
- 点光源和聚光灯可选择“启用阴影”，默认关闭；当前开关控制 YMGRE 已有的背光面近似补偿
  `shadowK`，不是基于遮挡关系的 shadow map 或真实投影阴影；
- 顶部“导入”使用 YMGUI FileDialog 浏览真实目录，通过显示过滤回调只显示目录和 `.mesh` 文件
  （扩展名大小写不敏感）；材质或贴图缺失不会隐藏 mesh，而是在选择后弹出具体错误；
  选中的 Ogre 二进制 `.mesh` 会通过 YMGRE 资源读取器加载，材质加入编辑器材质列表，网格加入
  场景对象列表并以最低顶点贴合 `Y=0`。
- 导入对话框支持路径编辑、上一级、目录树展开和新建目录；底部嵌入“显示渲染线框”选项，
  默认关闭。该线框是实际可见模型的三角网格叠加，不是碰撞网格；当前
  YMGRE Scene Editor 尚未建立独立碰撞体或碰撞网格系统。
- 导入前会预检同目录同名 `.material` 以及材质脚本中每条 `texture` 引用；缺失时在导入窗口
  显示具体文件名并停止加载，避免把不完整资源交给旧加载器导致进程退出。
- 多子网格模型会在检查器的可滚动列表中按材质名列出全部子网格，并可分别控制显示；隐藏部分
  不参与渲染、视口拾取和选中包围盒。网格对象可设为“固定”，固定后仍可从场景层级选中并
  解锁，但不会被视口拾取或拖动，适合地形和背景模型。
- 所有网格对象（包括导入对象）在检查器中提供 Y 轴旋转；按钮按 `5` 度步进，也支持手动输入。
- 中文使用 YMGUI Demo 已有的 `gb2312_glyphs.bin`、`YMGUI_GB2312_cps[]` 和
  `YMGUI_Font_SetFallback()`，不扩展库内精简 CJK 字集。字库文件由应用打开并在退出时关闭。

视口链路：

```text
YMGRE Camera
  -> 外部 RenderTarget（RGB565 + depth）
  -> 场景管线清屏/渲染对象
  -> 独立 3D 辅助线列表叠加
  -> YMGUI GYimg / Image 零拷贝显示
```

## 构建运行

```bash
cmake -S project_Demo/scene_editor -B build/project_Demo/scene_editor
cmake --build build/project_Demo/scene_editor -j
./build/project_Demo/scene_editor/scene_editor
```

外部网格导入默认从程序启动时的当前目录开始浏览，也可以指定其他只读浏览根目录：

```bash
YMGRE_IMPORT_ROOT=/path/to/assets ./build/project_Demo/scene_editor/scene_editor
```

打开/保存场景使用 YMGUI FileDialog：可在目录树中选择路径，再在底部输入文件名；
保存时未填写 `.scene` 后缀会自动补齐。默认目标为当前目录的 `scene.scene`，也可指定默认路径：

```bash
YMGRE_SCENE_PATH=/path/to/example.scene ./build/project_Demo/scene_editor/scene_editor
```

打开场景对话框底部的“强制忽略资源错误”默认关闭。启用后，缺少网格、材质或贴图的对象会被
过滤，其他对象继续加载，并在完成后弹窗列出被过滤的对象；损坏的 XML、相机或灯光仍会阻止打开。

## Blender 导出

`tools/blender_ymgre_exporter` 是 Blender 3.6+ Add-on。Blender 3.6 可安装源码目录，Blender 4.2+
可通过 Extensions / Install from Disk 安装仓库提供的 `tools/ymgre_scene_exporter.zip`。启用后使用
`File > Export > YMGRE Scene (.scene)`。

推荐流程：

1. 使用 Blender2Ogre 将网格导出为同目录的 `.mesh`、`.material` 和 BMP 贴图；
2. 网格资源默认取对象名，例如 Blender 对象 `Tank` 对应 `Tank.mesh`；资源名不同时，在对象的
   Custom Properties 中添加字符串 `ymgre_mesh_file`，值可为绝对路径、相对导出目录路径或 Blender
   的 `//` 相对路径；
3. 从 YMGRE 导出面板选择 `.scene` 目标。导出器会转换 Blender Z-up 坐标到 YMGRE Y-up，写出网格、
   相机、点光、聚光、活动相机和环境光，并检查 Ogre 资源依赖；
4. 在场景编辑器中直接打开生成的 `.scene`。资源警告会显示在 Blender 状态栏并输出到控制台。

对象 Custom Properties 还支持 `ymgre_fixed`、`ymgre_wireframe`、`ymgre_detail_a`、
`ymgre_detail_b`。设置 `ymgre_primitive` 为 `plane/cube/box/sphere/cylinder/cone/torus/capsule`
之一时，会导出为编辑器内置基本体，不依赖外部 `.mesh`。

当前导入格式要求：

- 文件为 YMGRE 已支持的 Ogre 二进制 `.mesh`，不是 Wavefront OBJ、FBX 或 glTF；
- 同目录存在同名 `.material`，例如 `model.mesh` 对应 `model.material`；
- `.material` 引用的 BMP 贴图位于同目录；
- 文件选择器不删除或重命名已有文件；可使用 FileDialog 的“New dir”创建资源目录。

有限帧无头验证：

```bash
SDL_VIDEODRIVER=dummy YMGRE_MAX_FRAMES=3 \
  ./build/project_Demo/scene_editor/scene_editor
```

功能自测：

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  YMGRE_SCENE_EDITOR_SELFTEST=1 \
  ./build/project_Demo/scene_editor/scene_editor
```

## 本阶段暴露并修复的引擎问题

- `CameraFromTarget` 的 `target` 与兼容 `img` 字段未同步，旧清屏入口会写空指针；
- Camera 销毁逻辑没有区分内部缓存和外部 Target，存在错误释放风险；
- 通过指针关系猜测所有权不可靠，现由 `ownsImageBuffers` 显式记录 Creat/Free 责任；
- 三角平面线框受背面剔除影响，不能作为双面编辑器参考网格；
- YMGRE 缺少独立世界空间线段图元，现已补充带视景体裁剪和深度测试的线列表管线；
- 工作区光照此前只按相机转换灯光位置，没有转换聚光方向；现在每个相机使用灯光世界方向的
  临时相机空间副本，不修改共享 Light，也不再要求 Demo 手动预转换；
- 全局光此前忽略 `strength`，默认满强度白光会掩盖局部彩色灯；现在全局、点光和聚光统一
  应用强度语义，并保护局部灯光计算中的零长度与零衰减分母；
- 固定尺寸 Target 无法支撑编辑器侧栏收起后的动态视口，现由应用同步更新 Target 和镜头宽高比。
- Scene Editor 首次接入 `.mesh` 读取器时暴露出资源 IO 对场景材质管理符号的链接依赖；当前
  ProjectDemo 按资源测试的既有方式显式编入 `YMGRE_SceneManager.c`，没有扩大核心库链接面。

## 当前边界与后续验收项

- 场景格式尚未保存每个导入子网格的独立显隐状态；
- 网格拾取使用所有可见 submesh 的投影顶点屏幕包围区，不是逐三角形射线拾取；
- 补充 `.map` 场景导入；当前外部导入已覆盖 `.mesh`、同名 `.material` 和 BMP 贴图预检；
- 为相机轨道、动态视口和辅助线遮挡增加可自动判定的 framebuffer 回归测试。
