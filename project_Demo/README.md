# project_Demo

这里放使用 YMGRE 完成的动态小应用，用持续输入、场景更新和资源管理等真实工作流验证库边界。基础物体或静态观察放在 `Demo/`，可自动判定的模块验证放在 `tests/`。

每个项目至少包含：

- 独立 `CMakeLists.txt`，按 `build/.cache/configs/<色深>/<索引宽度>/project_Demo/<项目名>` 构建（Girl 使用独立入口）；
- 独立源文件；
- 目标与限制说明；
- 构建和运行命令；
- 无头退出方式；
- 该项目暴露的库问题记录。

当前项目：

- [`girl_viewer`](girl_viewer/README.md)：人物材质查看器，运行 `./project_Demo/girl_viewer/build.sh --run`，程序位于 `build/bin/rgb888/index32/girl_viewer`。

- `scene_roaming`：加载地图和障碍物，接受相机输入并持续更新两个观察视角。
- `scene_editor`：YMGUI 编辑器外壳与 YMGRE 实时视口，验证 RenderTarget、相机交互、辅助线
  几何、动态布局、基础物体放置、层级树和实际网格属性编辑。
- `scene_baker`：独立 RGB888 烘焙编辑器，支持 UV1 直接光照烘焙、导出、回贴预览和场景重开。
- [`nature_preview`](nature_preview/README.md)：高度图草地、全部 32 种草丛和 6 种树、天空与太阳，以及地球和坦克。展示植被分块加载、近处网格与远景缓存图切换、黄土到绿色远景的过渡；支持一键构建和测试 index16、index32，文档包含实际截图与性能记录。
- [`lod_mesh_preview`](lod_mesh_preview/README.md)：用真实草、树资源和球面生成三层全模型 LOD，渲染原模型／中级／远级的并排对照。

## 综合验收项目：scene_editor

场景编辑器作为当前 PC 渲染与场景系统的系统级验收项目。首版应形成以下最小闭环：

- 从运行时资源根目录浏览并加载现有 `.map`、`.mesh`、`.material` 和 BMP；
- 用场景树选择对象，并在属性区编辑位置、朝向、缩放、可见性和线框状态；
- 在主视口持续渲染，支持相机观察、对象新增、克隆和删除；
- 管理场景中的相机、灯光与材质，并能观察修改后的实际 framebuffer 结果；
- 将选中对象的编辑结果保存到文件，并在同一场景中恢复，验证状态序列化链路；
- 提供固定场景和有限帧运行方式，使关键结果可由测试或 framebuffer hash 自动判定。

首版不以复杂变换 gizmo、完整撤销栈、通用资产导入和多格式转换为验收条件。这些能力可以在
资源生命周期、场景序列化和基础编辑闭环稳定后继续增加。
