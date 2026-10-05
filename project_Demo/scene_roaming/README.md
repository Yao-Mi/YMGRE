# scene_roaming

加载地图、地形、障碍物和两个相机，验证完整动态场景工作流：

- 主相机接受键盘和指针输入，在场景中改变观察位置；
- 辅相机保留另一个场景观察视角；
- 两个相机保留独立 RenderTarget，顺序共享 RenderWorkspace；
- 场景资源默认从根目录 `Resource/` 加载，也可在运行时指定其他根目录。

构建与运行：

```bash
cmake -S project_Demo/scene_roaming -B build/.cache/configs/rgb888/index16/project_Demo/scene_roaming
cmake --build build/.cache/configs/rgb888/index16/project_Demo/scene_roaming -j
./build/bin/rgb888/index16/scene_roaming
```

无头运行三帧：

```bash
SDL_VIDEODRIVER=dummy YMGRE_MAX_FRAMES=3 ./build/bin/rgb888/index16/scene_roaming
```

可选地把资源根目录作为第一个参数传入：

```bash
./build/bin/rgb888/index16/scene_roaming /path/to/Resource
```

程序会在场景初始化前检查关键 map 和 mesh。资源根目录无效时会输出错误并以非零状态退出。

SceneManager 只通过 `gre_scene_host` 回调请求帧推进、输入和画面提交；YMGUI Image、
事件状态和 SDL 生命周期由本应用持有。场景资源通过 `YMGRE_Scene_Init` 加载，场景拥有的
所有资源最终由 `YMGRE_Scene_Destroy` 统一释放。当前限制是固定场景内容的装配仍集中在
SceneManager，后续可继续拆成由应用组合的加载与更新 API。
