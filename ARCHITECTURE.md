# YMGRE 架构记录

## 分层

```text
CONFIG    平台类型、颜色深度、内存后端
DEBUG     断言和错误报告
OPOBJ     Camera/Object/Material 等资源生命周期
IOFILE    模型、场景和图片加载
CORE      数学、变换、剔除、光照、光栅化、渲染上下文
PORT      YMGUI 等显示适配
Demo      单功能演示
tests     自动化模块测试
project_Demo 真实应用场景验证
```

CORE 不应依赖 SDL。PORT 可以依赖 YMGUI，但多视口布局属于应用层，不属于 LCD 端口。

Demo 只组合少量 CORE API，展示静态或有限步骤结果。ProjectDemo 可以组合 IOFILE、SceneManager、PORT 和输入事件，负责持续变化的应用状态。测试模块只做自动判定，不依赖人工观察窗口。

## 多相机资源模型

相机资源拆成三部分：

```text
Camera
  位置、目标、镜头、视景体、世界到相机矩阵
  -> RenderTarget
  -> RenderWorkspace

RenderTarget
  width / height
  color framebuffer
  depth buffer

RenderWorkspace
  当前物体的变换顶点
  当前物体的多边形隐藏状态
  当前物体的多边形光照颜色
  当前相机的灯光位置
```

Workspace 只需容纳场景中最大的单个物体，因为管线完成一个物体的光栅化后才处理下一个物体。

## 共享策略

| 场景 | Target | Workspace |
|---|---|---|
| PC 多视口、顺序渲染 | 每相机独立 | 所有相机共享 |
| PC 多线程渲染 | 每相机独立 | 每相机独立 |
| MCU 相机切换 | 可以共享 | 可以共享 |
| MCU 同屏多视口 | 独立或屏幕子区域 | 顺序渲染时共享 |

独立 Target 让 YMGUI 的多个 `Image` 控件可以同时引用各自 framebuffer。共享 Target 只适用于不需要同时保留多幅画面的场景。

## 新旧管线

旧管线把以下相机相关状态写在共享对象中：

- `pointList_`
- `object.isDelete`
- `polygon.ishide`
- `polygon.planeColor_`
- `light.proper.pos_`

它保留用于 API 兼容，只适合顺序渲染。`RenderingWithWorkspace` 管线不修改这些字段，支持共享或独立 Workspace。

## YMGUI 接入

一个相机 Target 对应一个长期存活的 `GYimg` 和一个 `Image` 控件。`GYimg` 不拥有相机 framebuffer，更新一帧后只需 invalidate 对应控件。YMGUI 的 display/flush/input 留在 PORT，多相机数量、位置和布局留在 ProjectDemo 应用层。

## 三角形实体叠加线框

模型的 `wireFrame` 标志在物体层决定是否叠加三角网格，三角形填充热路径不执行逐片元的线框模式判断。实体叠线按以下顺序完成：

```text
遍历可见三角形并写入颜色/Z-buffer
  -> 全部片元填充结束
  -> 遍历可见三角形边
  -> 单像素线段进行深度测试并覆盖同深度填充
```

后画线框可以防止共享边被同一物体的后续片元覆盖。线框仍读取并更新当前 Target 的 Z-buffer，因此被其他可见表面遮挡的边不会直接穿透前景。

三角网格线和普通多边形轮廓采用独立的内部光栅化函数。三角网格线使用方向无关的对称 Bresenham，并在整数像素坐标计算平面深度；普通多边形轮廓使用与多边形扫描填充一致的像素中心覆盖规则。两类图元不能共用一套边界取整规则，否则会在普通多边形边缘产生空点，或让三角网格出现方向相关的端点和宽度差异。

三角化模型的内部共享边属于网格的一部分，边两侧都由片元填充。只有投影后的模型外轮廓要求线外为背景，调试截图时不能把内部三角剖分线误判为填充越界。
