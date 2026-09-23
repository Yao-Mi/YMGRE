# grass_green_strand3_001 低模

498 顶点，394 三角面（含背面），2 个子网格。原版 880 面。

用 GRE 的 `YMGRE_LoadOgreMeshAndMaterial` 加载同名 `.mesh`，同目录 `.material` 和 BMP 须一起保留。Y 向上，原比例与原点。静态模型，无骨架和动作。

本版本有意减少细枝、叶片细节；草轮廓先简化再三角化，树冠采用分布均匀的代表叶簇，树干保留主要分枝。近看会比原版更块状。所有可见背面计入面数，无需透明着色器。

已通过实际 GRE 加载和三个视角的离屏渲染检查。`preview.png` / `preview_1.png` / `preview_2.png` 是实际渲染。来源和原始许可见 `SOURCE_LICENSE.html`，详细数据见 `manifest.json`。
