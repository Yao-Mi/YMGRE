#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
for bits in 16 32; do
    build_dir="$repo_root/build/.cache/configs/rgb888/index$bits/project_Demo/nature_preview"
    cmake -S "$repo_root/project_Demo/nature_preview" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release -DYMGRE_INDEX_BITS="$bits" -DYMGRE_CAMERA_COLOR_DEPTH=24
    cmake --build "$build_dir" -j "${JOBS:-4}"
    ctest --test-dir "$build_dir" --output-on-failure
done
