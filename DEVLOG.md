# YMGRE 开发记录

## 2026-10-05：颜色纹理 Mip 第一版

- 材质可在加载颜色贴图后显式生成 Mip；准备阶段逐级平均，渲染按三角形的屏幕采样尺度选一层，片元保持单次颜色采样。顶点光照、逐像素材质、PBR 与旧扫描线入口共用该能力；未生成 Mip 的材质继续使用原图。
- 高层资源随材质释放；替换或修改基础贴图前需清理旧层，换图后重新构建。同尺寸透明度 Mip 存在时，颜色层按覆盖度加权以避免透明区底色造成暗边；透明图变化会清除颜色层。模型／图片 LOD 尚未实现。

## 2026-10-05：开发板通用优化回合并

- 合入跨平台头文件修复、顺序清屏、六平面裁剪快速判断、包围体提前剔除、灯光准备复用和有条件的共享顶点光照；补充可见顶点掩码、投影缓存、对角矩阵快速路径及 RGB565 光栅专用路径，保留新版 PBR、透明度和并行渲染。
- 高级线框采用新增浮点端点 API，保留旧整数 API；对象与 Workspace 增加连续索引和投影缓存字段，使用方需同步头文件并重编译。
- 连续索引池提供显式整理接口和默认关闭的构建选项；渲染缓冲区固定优先快内存、不足时回退慢内存，并使用匹配的释放器；图像与几何数据固定使用慢内存。
- 内存检查发现旧法线/高光 Demo 和测试没有初始化扩展材质的新增字段，统一改用现有初始化接口；点状浮点线段补充端点深度排序。
- 回归包含四种色深/索引组合、新增边界测试、基线裁剪差分和 Girl 画面对照；RGB565 合成球体主机基准约 498→256 µs/帧。具体结果与适用条件见 [移植优化兼容记录](docs/portable-optimizations.md)。
- 通用 LOD 仍留作后续能力；ARMCC 汇编/DSP 与 SDRAM/DMA 板级配置未带入。移植版历史板测结果不是本次性能承诺。


## 2026-08-27：Scene Editor 子网格、固定对象与安全导入

- 导入窗口常驻提示资源约束；进入 Ogre 加载器前检查同目录同名 `.material`，并解析材质脚本中
  的全部 `texture` 指令。网格、材质或贴图无法读取时显示具体文件名并停止，不再把不完整资源
  交给旧资源 IO 的致命错误路径；
- 多 submesh 模型在检查器中使用可滚动列表按材质名列出全部子网格，每项可独立控制显示；
  YMGRE 的普通/工作区、三角形/多边形四条对象渲染路径统一遵守 `isVisible`；
- 隐藏的 submesh 不参与视口拾取和选中包围盒；全部隐藏后不会回退到对象中心继续命中；
- 网格检查器增加“固定”。固定对象仍可从场景层级选中、修改属性和解除固定，但不会被视口
  拾取或拖动，适合地形、背景和其他只作为观察参照的对象；
- 检查器动态子网格控件与对象链保持对应，并在退出时释放其映射内存；Scene Editor 自测失败
  时会报告源码行号和条件，便于后续定位回归。

### 本轮验证

- YMGRE CTest `12/12`、YMGUI CTest `32/32` 通过；
- Scene Editor SDL dummy 自测通过，覆盖固定对象不可拾取/拖动、隐藏子网格不可拾取；
- ASan/LeakSanitizer Scene Editor 自测通过；`test_render_context` 新增隐藏 submesh 保持背景色的
  framebuffer 回归；
- 普通、Sanitizer 和严格警告构建通过；严格构建仅保留历史 pragma、未用参数和旧缩进告警；
- `git diff --check` 通过。本轮未进行截图检查，由使用者直接运行程序验收界面。

## 2026-08-27：Scene Editor 网格旋转、导入导航与外部网格拾取

- 网格检查器的变换区增加 Y 轴旋转，支持手动输入和正负 `5` 度连续步进；旋转以 Scene Object
  的世界位置为轴心，逐 submesh 更新实际世界顶点，重新网格化时继续保留当前旋转；
- 导入浏览器增加“上一级”和“进入目录”，目录仍支持双击进入；文件列表以当前目录重新载入，
  不再只能从初始树根向下展开；
- 外部网格拾取不再依赖导入文件头部单一包围球，改为投影所有 submesh 的实际世界顶点并使用
  屏幕包围区和最近深度命中；即使模型枢轴不在几何中心，仍可直接在视口选择；
- 导入窗口增加“显示渲染线框”，默认关闭并同步到所有 submesh；此前自动显示的是渲染模型
  自身的三角线框，不是碰撞网格。当前编辑器没有独立碰撞体数据或碰撞网格渲染能力。

### 本轮验证

- Scene Editor 自测覆盖 90 度 Y 轴旋转顶点结果、导入线框默认关闭，以及移动后的外部网格
  通过实际投影顶点在视口命中；普通与 ASan/LeakSanitizer 自测通过；
- YMGRE CTest `12/12`、YMGUI CTest `32/32` 通过，`git diff --check` 通过。

## 2026-08-27：Scene Editor 平面、灯光阴影开关与外部网格导入

- 放置菜单增加 YMGRE 矩形平面，默认尺寸 `48x48`，支持行/列网格细分；平面记录生成参数，
  可在 Inspector 中修改细分并重新生成；
- 点光源与聚光灯增加“启用阴影”属性，放置时默认关闭，Inspector 可实时切换；该属性控制
  引擎已有的 `shadowK` 背光面近似补偿，并不等同于 shadow map 或物体间真实遮挡投影；
- 新增独立 `scene_editor_import` 模块，复用 YMGUI 文件管理 Demo 的 TreeView 懒加载方案，
  以只读方式浏览目录并过滤 `.mesh` 文件；默认根目录为启动目录，可通过 `YMGRE_IMPORT_ROOT`
  指定；
- 顶部工具栏增加“导入”，选中 Ogre `.mesh` 后检查同目录同名 `.material`，调用
  `YMGRE_LoadOgreMeshAndMaterial`，把材质接入编辑器渲染材质列表，并将含 submesh 的 Object
  完整应用落地位置、加入层级和渲染列表；
- Scene Editor 目标按资源测试的既有做法单独编入 `YMGRE_SceneManager.c`，满足资源 IO 的材质
  管理链接依赖，不改变核心 `ymgre` 静态库的目标边界；
- 多 submesh 的统一变换入口现会逐个处理子网格，删除和退出仍由一次 `YMGRE_Free_Object`
  沿 `nextObject` 链完整释放。

### 本轮验证

- Scene Editor 自测新增平面 `3x4 -> 5x6` 重建、灯光阴影默认关闭/切换同步，以及仓库自带
  `Resource/obj/Tank1_Body.mesh` 的网格、材质、加入、删除和退出释放链路；
- Scene Editor 普通及 ASan/LeakSanitizer 自测通过，YMGRE CTest `12/12`、YMGUI CTest
  `32/32` 通过，`git diff --check` 通过。

## 2026-08-27：Scene Editor 网格重建与放置基准统一

- 修正选中对象后左键水平拖动的相机 right 方向映射，使物体在屏幕上跟随鼠标移动；空白区域
  平移仍维持既有观察操作方向；
- 聚光灯辅助锥增加灯位到面板照射目标的中心线和目标点三轴十字，锥底圆周不再被误认为目标；
  默认照射目标仍为世界原点，Light 的归一化方向由灯位与该目标计算；
- Scene Object 记录基础形状类型及两项原始细分数；球、圆柱、圆锥、圆环和胶囊的检查器增加
  对应网格细分输入，修改有效数值后重新调用 YMGRE 基础网格生成器，并恢复颜色、旋转、缩放、
  世界位置和线框状态；渲染列表原位替换新 Object 后释放旧 Object，Create/Free 保持配对；
- 放置网格时把 Y 字段明确为“底面 Y”，默认值为 `0`；创建后依据生成网格的实际最低顶点和
  scale 计算对象中心，因此所有基本形状在任意初始 scale 下都以底面贴合零平面；
- Inspector 批量回填属性时暂停细分回调，避免切换不同形状时用上一对象的输入意外触发重建。

### 本轮验证

- Scene Editor 自测覆盖球体 `6x12 -> 8x16` 重建、列表句柄替换、旧网格释放后的数量状态、
  `scale=2` 球体最低顶点落在 `Y=0`，以及聚光锥中心线终点等于面板目标；
- YMGRE CTest `12/12`、Scene Editor 普通与 ASan/LeakSanitizer 自测通过；
- `git diff --check` 通过。

## 2026-08-27：Scene Editor 灯光与选择交互修正

- 全局光照正式应用 Light 的 `strength`，Scene Editor 默认环境强度改为 `0.2`，避免满强度
  白色环境光先把物体底色推到饱和、掩盖点光源和聚光灯颜色；光照回归覆盖全局光强度以及
  红/蓝点光和绿色聚光对表面颜色的实际影响；
- 点光源和聚光灯计算补充零长度法线、零长度方向、灯位与受光点重合以及无效衰减分母保护，
  退化输入不再通过除零污染最终颜色；
- 检查器长按步进改由单调时钟的真实帧间毫秒数驱动，保持首次 400 ms 延迟、之后每 80 ms
  连续变化，不再把渲染耗时错误地固定折算为每帧 16 ms；
- 修正视口左键拖动的水平映射；启动、删除对象及左击视口空白处均保持无选择状态，检查器和
  场景树同步清空；
- 场景树右键菜单增加透明顶层遮罩，点击菜单外任意空白处即可关闭，不再强制执行某个命令；
- 右键开始轨道旋转时，若当前选择是有效且不与活动相机重合的场景对象，以该对象位置作为
  相机环绕中心；未选择对象时继续使用当前相机目标。

### 本轮验证

- YMGRE CTest `12/12`、YMGUI CTest `32/32` 通过；
- Scene Editor SDL dummy 自测和 ASan/LeakSanitizer 自测通过；
- `git diff --check` 通过。

## 2026-08-26：Scene Editor 聚光锥、相机视锥与属性长按步进

- 对照 `demo_lighting`，选中聚光灯时通过 YMGRE 3D 线段管线显示八边线框光锥；锥顶取灯位，
  锥底中心取照射目标，半径来自 Light 的实际内锥角，修改位置或目标后逐帧同步；
- 选中相机显示机位外框、位置到注视目标的视线，以及按实际左右/上下视场构造的近远视锥；
  编辑器显示深度取目标距离，避免使用上千单位的实际 far 面妨碍场景观察；
- 修复工作区渲染只转换灯光位置、不转换聚光方向的问题：逐多边形光照使用 Light 的临时副本，
  按当前相机转换世界空间方向，不修改共享 Light；`demo_lighting` 删除手动预转换；
- 检查器的位置、缩放和目标步进按钮启用 YMGUI Button 可选连发，短按一次，按住 400 ms 后
  每 80 ms 连续变化，松开不会再多跳一次；
- Scene Editor 自测增加聚光锥 16 条线和相机辅助线 25 条线的数量检查。

## 2026-08-26：修复 Scene Editor 拖动物体巨值与灯光 NaN

- 根因是 `YMGRE_UVNCamera_PositionInit` 只用局部 `viewU/viewV/viewN` 构造变换矩阵，没有同步
  写入公开的 `camera->move.cu/cv/cn`；编辑器读取这三个字段换算鼠标位移时得到无效相机基；
- 相机初始化现在保存实际使用的正交基，并对位置与目标重合、默认 up 与视线平行两类退化
  视角选择稳定的备用方向；相机公开运动状态与 `TMat` 保持一致；
- 编辑器按横纵投影范围分别把像素位移换算到相机 right/up 世界方向，投影、位移和对象同步入口
  均拒绝非有限参数，异常输入不会再污染 Object 或 Light 坐标；
- `test_camera_viewport` 增加普通及竖直相机的单位基回归，Scene Editor 自测增加点光源拖动后
  位移幅度与 Light 坐标有限性检查。

## 2026-08-26：Scene Editor 对象选择、层级操作与多类型资源

- 场景对象模型区分 Mesh、Light 和 Camera，并保存对应 YMGRE 句柄；创建、删除与退出释放按
  Object/Light/Camera 类型完全配对，场景树、渲染列表和检查器共享同一对象状态；
- 曲面放置参数接入基础网格生成器：球体、圆柱、圆锥、圆环和胶囊分别暴露适用的细分数量；
- 场景树接入右键上下文菜单，支持重命名、删除以及相机视角切换；主相机保留删除保护，删除
  当前活动的附加相机时先切回主相机；
- 视口增加对象拾取与拖动：命中对象后在相机视平面内移动，空白区域保持相机平移，右键继续
  轨道旋转；选中网格由 YMGRE 3D 线段管线绘制橙色 AABB，灯光和相机绘制橙色定位标记；
- 放置菜单增加点光源、聚光灯和相机；聚光灯目标同步为归一化方向，多相机共享 RenderTarget
  但各自保持位置和注视目标；
- 检查器改为按对象类型显示真实可编辑属性。全局光照颜色/强度直接写入活动 Light，主相机
  位置与目标直接同步到活动 Camera，不再显示无效的通用材质字段。

### 本轮验证

- Scene Editor 内置自测覆盖曲面细分、两类灯光、附加相机、视角切换、全局光颜色同步、
  重命名、删除、列表数量、选中包围线与退出释放；
- Scene Editor 普通构建及 SDL dummy 自测通过，ASan/UBSan/LeakSanitizer 自测无报告；
- YMGRE CTest `12/12`、YMGUI CTest `32/32` 通过，`git diff --check` 通过。

## 2026-08-26：Scene Editor 基础物体放置与属性检查器

- Scene Editor 按职责拆分 `scene_editor_place`、`scene_editor_color`、`scene_editor_inspector`、
  `scene_editor_font` 和共享对象模型，避免放置流程、弹窗状态与视口相机继续堆在主文件；
- 顶部工具栏和编辑菜单改为中文；放置菜单支持立方体、长方体、球体、圆柱、圆锥、圆环
  与胶囊，确认属性后创建真实 YMGRE Object，并同步加入渲染列表、场景树和当前选择；
- 放置弹窗支持名称、位置、Y 旋转、缩放、ColorPicker 颜色以及默认开启的实体叠加线框；
- 检查器 X/Y/Z 采用红/绿/蓝轴色，每行提供前置加按钮、可编辑数值和后置减按钮；位置、
  缩放、颜色和线框均直接修改实际网格状态，不再只是静态占位文本；
- 完整中文复用 YMGUI 官方 Demo 的 `gb2312_glyphs.bin` 外部字模方案：应用打开 blob，使用
  `YMGUI_GB2312_cps[]` 建立运行期字体并挂到全局 fallback，退出时先解除 fallback 再关闭文件；
  未修改精简 `PRESET_CJK` 和内嵌 `YMGUI_FontDataCJK.c`；
- 颜色选择弹窗在放置弹窗之后创建，保证 top layer 兄弟顺序正确，颜色轮盘不会被属性弹窗遮挡。

### 本轮验证

- Scene Editor 普通构建和有限帧运行通过；
- 普通 CTest `12/12`、ASan/UBSan CTest `12/12` 通过；
- Scene Editor 在 SDL dummy 驱动下完成 ASan/UBSan/LeakSanitizer 有限帧运行，无报告；
- Xvfb/X11 驱动退出时有两笔来自匿名外部模块的 56 字节残留，dummy 驱动复跑无残留，
  已确认不是 YMGRE/YMGUI Create/Free 链路泄漏；
- `git diff --check` 通过。

## 2026-08-26：Scene Editor 空场景视口与相机交互

- `project_Demo/scene_editor` 建立 YMGUI 编辑器框架：顶部工具栏、场景树、Inspector、状态栏和
  中央实时视口；左右侧栏可独立收起，视口、Image、鼠标命中层和 RenderTarget 随可用宽度
  在 560 至 1024 像素之间同步调整，镜头横向视场按宽高比更新；
- 视口使用 YMGUI 通用 Image 零拷贝显示 YMGRE RGB565 RenderTarget。定位并修复透明事件层
  使用基础 Obj 默认灰色绘制、从而遮住下层 Image 的问题；外部 framebuffer 每帧更新后显式
  invalidate Image；
- 左键拖动平移观察中心，右键拖动绕中心旋转，滚轮采用无交互边界的指数缩放；near/far
  随轨道距离调整，Reset View 恢复初始相机状态；
- 原三角平面线框会经过背面剔除，绕到反面后消失。新增独立 `gre_line3d` 和
  `YMGRE_Camera_LineList_Rendering`，完成世界到相机变换、六面视景体裁剪、透视投影及带
  1/z 深度插值的线光栅化；编辑器网格以及红 X、绿 Y、蓝 Z 轴均改由该管线渲染；
- `YMGRE_Creat_CameraFromTarget` 现在同步兼容 `camera.img` 视图；Camera 新增显式
  `ownsImageBuffers`，普通 Camera 释放自身创建的颜色/深度缓存，FromTarget Camera 只释放
  Camera 头，重新绑定 Target 不转移所有权，落实 Creat/Free 配对；
- Demo 暴露的根因包括：主循环漏调 overlay render、Camera 新旧 Target/img 契约不一致、
  外部缓存释放权不明确、三角线框无法承担双面编辑器辅助几何，以及 UI 尺寸变化未传递到
  RenderTarget。

### 本轮验证

- 正常视角、网格下方视角和双侧栏收起后的 1024 像素宽视口均完成截图检查；
- `test_render_context` 增加外部 Target 所有权、清屏、重新绑定和独立 3D 线段输出检查；
- 普通及 ASan/UBSan CTest `12/12` 通过，Scene Editor sanitizer 有限帧运行通过；
- YMGUI 联调与后续同步范围记录在 `extern_lib/YMGUI/DEVLOG.md`。

## 2026-08-26：资源与材质回归测试

- 新增 `test_resource_material`，覆盖材质默认值、按名查找、UV 重复采样、禁用回退和
  2x2 纹理片元输出；
- 使用 3 像素宽图片验证 24 位 BMP 行补齐、像素方向和逐像素读写一致；
- 直接解析仓库 `Tank1` 材质并加载 512x512 BMP，验证颜色参数、真实图像数据和重复材质去重；
- 测试发现 `getPixel` 错用整数绝对值导致浮点 UV 被截断，现已与光栅器统一使用浮点采样，
  并为零尺寸纹理增加回退保护；
- 将材质脚本解析入口正式声明为 `YMGRE_ParseMaterialScript`；
- 明确后续 `project_Demo/scene_editor` 作为资源、场景、渲染、交互与序列化的系统级验收项目。

### 本轮验证

- 纯核心构建完成，CTest `12/12` 通过；
- ASan/UBSan 构建 CTest `12/12` 通过；
- `git diff --check` 通过。

## 2026-08-26：YMGUI 刷新与 SceneManager 工程化收口

- YMGUI SDL 后端改为每个 band 只更新 texture，一轮 `YMGUI_Refresh` 完成后通过
  `frame_done` 回调统一执行一次 `SDL_RenderPresent`，消除逐 band 上屏造成的块状刷新和延迟；
- 新增 `YMGUI_Disp_SetFrameDoneCb` / `YMGUI_Disp_FrameDone`，并由
  `test_invalidate` 验证多 band、多脏区每轮只完成一次 present；
- 将核心渲染管线末尾的 worldmap 球体实验和直接 `LCD_*` 交互代码移至
  `YMGRE/legacy/`；legacy 不参与默认构建，核心目录不再依赖 YMGUI、SDL 或 LCD API；
- SceneManager 增加 `YMGRE_Scene_Init` / `YMGRE_Scene_Destroy`，统一管理场景列表、相机、
  灯光、材质、地形和资源根目录；相机位置、目标和旋转角改为场景实例状态；
- 删除编译期 `YMGRE_RESOURCE_DIR`，`scene_roaming` 支持从首个命令行参数接收资源根目录，
  默认使用 `Resource`；加载前检查关键 map/mesh，错误目录会输出原因并正常返回失败；
- SceneManager 通过 `gre_scene_host` 获取帧推进、键盘、指针和画面提交，窗口、YMGUI
  Image、事件及 SDL 生命周期全部归属 ProjectDemo。

### 本轮验证

- YMGRE CTest `11/11` 通过，18 个基础 Demo 无头运行通过；
- `scene_roaming` 的默认路径、显式 `Resource` 路径和错误资源路径均已验证；
- 关闭 `YMGRE_BUILD_YMGUI_HOST` 后，纯核心库独立构建及 CTest `11/11` 通过；
- `git diff --check`、legacy 构建排除和核心依赖扫描通过。

## 2026-08-22：移除 YMGRE 显示端口

- `ymgre` 不再编译或链接 `YMGRE/PORT`、YMGUI、SDL；纯核心测试无需桌面显示依赖；
- 新增 Demo 层 `demo_host`，RenderTarget 直接绑定长期存活的 `GYimg` 和 YMGUI Image；
- 18 个基础 Demo 全部迁移到 DemoHost，删除 `LCD_*` 兼容接口；
- SceneManager 改用 `gre_scene_host` 回调，`scene_roaming` 自行管理 YMGUI 输入和显示；
- `YMGRE_Material_Find` 从 SceneManager 拆到独立核心模块。

## 2026-08-22：基础 Demo 补全与多边形三角剖分

- 完善 `demo_lighting`：点光源漫反射和高光统一乘光源强度；聚光灯补充 Blinn-Phong 高光，并叠加距离、锥角和入射角衰减；聚光灯线框改为与内锥角联动的圆锥体；修正工作区光照计算错误使用 `tpoints[0]` 导致大模型光源标记与高光位置错位的问题。
- 新增 `demo_raster_primitives`，集中验证直线、扫描线填充和二维多边形裁剪后的底层绘制。
- 新增独立的 `YMGRE_PolygonTriangulation.h/.c` 模块，使用耳切法将简单二维多边形转换为三角形索引；支持顺/逆时针绕序，不支持自交和带孔多边形。
- 新增 `demo_polygon_triangulation`，对照凹多边形直接填充与耳切三角网格填充，并显示内部共享边。
- 更新 `API.md`、`Demo/README.md` 和 `PROJECT.md`，当前根构建包含 18 个基础 Demo。

### 本轮验证

- 全部 18 个 Demo 编译通过；新增 Demo 均支持 `YMGRE_MAX_FRAMES=1` 无头运行；
- CTest `11/11` 全部通过；
- `git diff --check` 通过。

## 2026-08-21：基础光栅化与线框叠加收敛

- 三角形快速填充从 `YMGRE_Rasterization.c` 拆到独立的 `YMGRE_TriangleRaster.c/.h`，保留原有上下半三角扫描结构和中文注释；
- 模型级 `wireFrame` 使用两遍渲染：第一遍填充全部可见三角片元，第二遍统一后画三角网格，避免后续片元覆盖先画共享边；
- 线框仍参与 Z-buffer 测试，相同深度允许后画线框覆盖填充；深度容差按屏幕平面斜率补偿像素取整误差，避免用固定大偏移造成隐藏边穿透；
- 三角网格使用独立的对称 Bresenham 单像素线段，并与三角形填充统一在整数像素坐标计算深度；普通多边形轮廓继续使用与扫描填充配套的像素中心采样，两条路径互不改变覆盖规则；
- 三角网格内部共享边两侧都存在填充是正常结果，不表示填充越过模型外轮廓；模型真正的外轮廓外侧应保持背景色；
- 清理了扫描线端点直接着线框颜色等试验方案，底层填充热路径不包含 `drawWire` 判断。

### 本轮验证

- 12 个基础 Demo 已统一在根 `build/` 重新编译；
- CTest `11/11` 通过；
- 三角片元共享边压力测试覆盖无裂缝、线框后画保留和单像素宽度；
- 普通多边形测试继续覆盖三角形、四边形、五边形填充边界无空点；
- `git diff --check` 通过，临时截图和诊断输出未写入项目目录。
- 按项目原有中文注释风格补齐 RenderTarget/Workspace 所有权、Context 管线阶段、基础网格几何约定、对偶拓扑和外部线框路径说明；不为普通赋值和简单循环增加复述式注释。

## 2026-08-21：YMGUI 接入与跨平台渲染上下文

对比基线：`f3ffdc6 Integrate YMGUI demo and stabilize YMGRE runtime`

### 本轮约定

- YMGRE 默认相机 framebuffer 使用 RGB565，保留其他颜色深度源码兼容能力。
- YMGUI 只作为显示与输入工具，YMGRE 的 Demo、测试、文档和构建产物不放入 YMGUI。
- 构建产物统一放在根 `build/`。
- 新增文件使用 UTF-8、LF；修改旧文件不删除原注释，不做无关整文件格式化。
- PC Demo 用于开发验证，不要求以相同分辨率或场景运行在 STM32。

### 渲染上下文

新增 `RenderTarget` 管理颜色与深度输出，新增 `RenderWorkspace` 管理相机相关临时数据。Context 管线不再把变换顶点、剔除结果和光照结果写回共享 Mesh。

推荐组合：

- PC 顺序多相机：Target 独立，Workspace 共享；
- PC 并行多相机：Target 独立，Workspace 独立；
- MCU 相机切换：Target 和 Workspace 均可共享；
- MCU 静态内存：通过 `Init` API 绑定调用者提供的固定容量缓存。

### 性能记录

扫描线填充改为流式活动边工作区，不再为整张图建立边表。此前相同场景测量结果：

- `-O2` 约为原实现的 `1.18x`；
- `-Os` 约为原实现的 `2.65x`；
- `.text` 减少 2882 字节；
- 10000 组随机 framebuffer/Z-buffer 差分一致。

向量、矩阵和渲染热路径的临时结果改为栈对象或输出参数，减少帧内小块动态分配。

### 回归结果

- RGB565 Release：库、基础物体 Demo、静态双目 Demo、场景漫游 Project 和 CTest 通过；
- 历史 RGB888 兼容构建曾通过；当前日常构建固定 RGB565；
- ASan、UBSan、LeakSanitizer：模块测试和两个 Demo 正常退出，无报告；
- 三角形与普通多边形 Context 管线均纳入模块测试；
- 动态共享、动态独立、外部静态 Workspace 和外部 Target 均已覆盖；
- RGB565 场景漫游截图 hash：`7ebb5e707ca14cdd3ed8b811ef28517b0613bd4e78d7c28bb65979ef59164400`。

### 已知问题

- YMGUI 端口目前固定创建 main/aux 两个 Image，动态视口列表应移到 ProjectDemo 应用层。
- 文件读取模块有多处未检查 `fread/fgets` 返回值，损坏或截断资源的错误路径不完整。
- YMGRE 与 YMGUI 的公共基础类型重复 typedef，严格 `-Wpedantic` 构建会报警。
- 部分旧文件包含 MSVC 专用 pragma、空函数参数和反斜杠结尾注释告警，后续按模块处理。
- 尚未加入 STM32 工具链编译、cycle 计数和峰值栈测试。

## 2026-08-21：Demo 与 ProjectDemo 边界整理

- `Demo/demo_basic_shapes.c` 静态显示矩形平面、立方体、长方体、圆柱、圆锥和球体；
- `Demo/demo_extended_shapes.c` 静态显示圆环、胶囊体以及正四、正八、正十二、正二十面体；
- `Demo/demo_stereo_view.c` 使用两个独立 Target 和共享 Workspace 显示静态双目视角；
- 原地图入口迁到 `project_Demo/scene_roaming`，用于动态场景漫游；
- 基础网格生成器正式启用，统一输出三角网格、外向法线、AABB 和包围球；
- 新增 `test_basic_mesh`，自动检查数量、索引、退化三角形与法线方向。
- 历史 RGB565/RGB888 截图逐字节一致；当前只保留 RGB565 截图作为日常基准。
- 当前 Object 析构要求每个 Polygon 独立释放索引，基础网格初始化仍会产生多个索引分配；后续可设计连续索引池，减少 MCU 堆碎片。

## 2026-08-21：构建与资源目录收敛

- PC 日常构建固定 RGB565，不再生成 RGB565/RGB888 两套并行构建目录；
- 根 Demo 目标按 YMGUI 方式命名为 `demo_basic_shapes`、`demo_stereo_view`；
- `scene_roaming` 改为带独立 CMakeLists 的 ProjectDemo，输出到 `build/project_Demo/scene_roaming`；
- 地图、模型和图片统一从根 `Resource/` 加载，Demo 目录只保留演示源码和说明。
- 早期 EGE 入口和 2 MB `worldmap.h` 归档到 `Demo/legacy`，历史球体测试块不再编入 YMGRE 库。

## 2026-10-08 原生漫游端口经验回合并

从 ymgre_roam_perf320 提取紧凑不透明实例、精确索引/位置准备、预分配八桶面序、普通不透明批追加、按行背景、可配置内存放置、ARMCC 兼容修整及默认关闭的串行阶段计数。保持现有透明/PBR/并行渲染和光栅规则，未接入硬件容量修改、资源删面或已撤回实验；详见 docs/roam-port-integration.md。
