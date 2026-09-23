#!/usr/bin/env bash
# Consumer entry: compiles only example code against precompiled archives.
set -euo pipefail
ymgre_sdk_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
usage() { echo 'Usage: ./build_demos.sh [16|32] [path/to/YMGUI/cmake] [--depth 16|24]'; }
if [[ "${1:-}" == --help || "${1:-}" == -h ]]; then usage; exit 0; fi
ymgre_sdk_bits="${1:-16}"
if (($#)); then shift; fi
case "$ymgre_sdk_bits" in 16|32) ;; *) usage >&2; exit 2 ;; esac
ymgre_sdk_depth=16
ymgre_external_gui=
while (($#)); do
    case "$1" in
        --depth)
            if (($# < 2)); then usage >&2; exit 2; fi
            ymgre_sdk_depth="$2"; shift 2 ;;
        -*) usage >&2; exit 2 ;;
        *)
            if [[ -n "$ymgre_external_gui" ]]; then usage >&2; exit 2; fi
            ymgre_external_gui="$1"; shift ;;
    esac
done
case "$ymgre_sdk_depth" in
    16) ymgre_sdk_format=rgb565 ;;
    24) ymgre_sdk_format=rgb888 ;;
    *) usage >&2; exit 2 ;;
esac
ymgre_demo_build="$ymgre_sdk_dir/examples-build/$ymgre_sdk_format/index$ymgre_sdk_bits"
ymgre_window_args=(-DYMGRE_WINDOWED_DEMOS=OFF)
if [[ -n "$ymgre_external_gui" ]]; then
    ymgre_window_args=(-DYMGRE_WINDOWED_DEMOS=ON "-DYMGUI_DIR=$ymgre_external_gui" "-DYMGUI_COLOR_DEPTH=$ymgre_sdk_depth")
fi
cmake -S "$ymgre_sdk_dir/examples" -B "$ymgre_demo_build" \
    -DYMGRE_INDEX_BITS="$ymgre_sdk_bits" -DYMGRE_CAMERA_COLOR_DEPTH="$ymgre_sdk_depth" \
    -DCMAKE_BUILD_TYPE=Release "${ymgre_window_args[@]}"
cmake --build "$ymgre_demo_build" -j "${YMGRE_BUILD_JOBS:-4}"
ctest --test-dir "$ymgre_demo_build" --output-on-failure
printf '\nDemo programs: %s/bin\n' "$ymgre_demo_build"
