#!/usr/bin/env bash
# Consumer entry: compiles only example code against precompiled archives.
set -euo pipefail
ymgre_sdk_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ymgre_sdk_bits="${1:-16}"
case "$ymgre_sdk_bits" in
    16|32) ;;
    *) echo 'Usage: ./build_demos.sh [16|32] [path/to/YMGUI/cmake]' >&2; exit 2 ;;
esac
ymgre_demo_build="$ymgre_sdk_dir/examples-build$ymgre_sdk_bits"
ymgre_window_args=(-DYMGRE_WINDOWED_DEMOS=OFF)
if [[ -n "${2:-}" ]]; then
    ymgre_window_args=(-DYMGRE_WINDOWED_DEMOS=ON "-DYMGUI_DIR=$2")
fi
cmake -S "$ymgre_sdk_dir/examples" -B "$ymgre_demo_build" \
    -DYMGRE_INDEX_BITS="$ymgre_sdk_bits" -DCMAKE_BUILD_TYPE=Release "${ymgre_window_args[@]}"
cmake --build "$ymgre_demo_build" -j "${YMGRE_BUILD_JOBS:-4}"
ctest --test-dir "$ymgre_demo_build" --output-on-failure
printf '\nDemo programs: %s/bin\n' "$ymgre_demo_build"
