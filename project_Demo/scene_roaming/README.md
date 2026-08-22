# scene_roaming

加载地图、地形、障碍物和两个相机，验证完整动态场景工作流：

- 主相机接受键盘和指针输入，在场景中改变观察位置；
- 辅相机保留另一个场景观察视角；
- 两个相机保留独立 RenderTarget，顺序共享 RenderWorkspace；
- 场景资源统一从根目录 `Resource/` 加载。

构建与运行：

```bash
cmake -S project_Demo/scene_roaming -B build/project_Demo/scene_roaming
cmake --build build/project_Demo/scene_roaming -j
./build/project_Demo/scene_roaming/scene_roaming
```

无头运行三帧：

```bash
SDL_VIDEODRIVER=dummy YMGRE_MAX_FRAMES=3 ./build/project_Demo/scene_roaming/scene_roaming
```

当前限制：场景加载、相机输入和渲染循环仍集中在 SceneManager，后续应拆成可由应用组合的场景 API。
