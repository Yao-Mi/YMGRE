# GRE / YMSKE 人体演示

直接运行 `../../build/bin/rgb888/index16/demo_human`；需要重建时执行本目录的 `./run_demo.sh`。

演示从 GRE 的 Resource/human 加载 human.mesh/material 和 human.ske/ska，使用六槽完整蒙皮权重。SKE/SKA 默认二进制，传 `--text` 可切换对应文本文件。可用 `--assets /path/to/human` 指定资源目录。默认资源绝对路径在构建时写入，因此可以从文件管理器直接启动。

## 操作

- 鼠标拖动旋转视角，滚轮缩放，双击或 VIEW / V 重置视角。
- PREV / NEXT 或 [ / ] 选择骨骼；AXIS / X 切换局部旋转轴。
- 滑块或左右方向键调节所选骨骼角度（-90° 到 +90°）。每次切换骨骼恢复绑定姿态。
- MOTION / Space 自动摆动当前关节；BIND / R 恢复绑定姿态。
- BONES / B 显示骨架（所选骨骼为金色），WIRE / W 显示线框。
- Esc 或关闭窗口退出。

源 SKA 只有单帧绑定姿态。MOTION 是演示层的正弦关节测试，不是原资源自带动作，也不模拟 IK、关节限位或物理。

## 构建和验证

```sh
cmake -S /home/yaomi/文档/0502_YMGRE/project_Demo/human -B /home/yaomi/文档/0502_YMGRE/build/.cache/configs/rgb888/index16/project_Demo/human -DCMAKE_BUILD_TYPE=Release
cmake --build /home/yaomi/文档/0502_YMGRE/build/.cache/configs/rgb888/index16/project_Demo/human --target demo_human -j2
ctest --test-dir /home/yaomi/文档/0502_YMGRE/build/.cache/configs/rgb888/index16/project_Demo/human --output-on-failure
```

独立 CMake 项目，默认引用相邻 YMSKE 目录（可通过 YMSKE_ROOT 改写），不修改 GRE/YMSKE 的主构建配置。

在输出目录运行 `demo_human --headless` 生成绑定姿态、关节变形、骨架叠加三个 PPM。`--capture-ui` 保存带控件的 human_controls.ppm；无显示环境可设置 SDL_VIDEODRIVER=dummy。`--smoke-test` 自动检查摆动、复位、骨架、线框、拖动、缩放、骨骼选择、旋转轴、角度滑块和退出。

已通过离屏与窗口事件测试；二进制和文本资产生成的三个画面逐字节一致。初始化核对骨架/动作配对及 GRE 网格指纹；实时使用已加载数据和预分配渲染工作区。
