# YMGRE Legacy

这里存放不属于当前核心 API 的旧交互式管线和实验代码。Legacy 源文件不参与默认构建，
仅用于历史对照和迁移参考。新代码应使用 `YMGRE_Camera_*RenderingWithWorkspace`，并由
应用层负责窗口、输入和帧循环。

- `YMGRE_Rendering_Pipeline_legacy.c`：原球体/worldmap 实验以及直接 `LCD_*` 交互循环。
