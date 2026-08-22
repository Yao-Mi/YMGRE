# legacy

这里归档早期 PC/EGE 入口和生成式贴图数组，仅用于历史对照，不参与当前 CMake 构建。

- `1、main.c`：早期 EGE 场景入口；
- `worldmap.h`：早期球体贴图的 RGB565 C 数组。

当前显示统一使用 YMGUI/SDL，基础演示使用 `Demo/demo_*.c`，动态场景使用 `project_Demo/`。
