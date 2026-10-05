#!/usr/bin/env bash
# Build each standalone project in a configuration-specific directory.
set -uo pipefail
ymgre_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ymgre_depth=24
ymgre_bits=16
ymgre_jobs="${YMGRE_BUILD_JOBS:-4}"
ymgre_clean=0
ymgre_test=0
ymgre_core_only=0
usage() {
    cat <<'HELP'
Usage: ./build_all.sh [--depth 16|24] [--index-bits 16|32] [--jobs N]
                      [-c|--clean] [-t|--test] [--core-only]
Defaults: RGB888, 16-bit indices, Release, 4 jobs (or YMGRE_BUILD_JOBS).
Builds the core, basic demos, and compatible project_Demo applications.
--core-only builds just the core and its tests, without YMGUI/SDL or apps.
--test runs root/core CTest; application tests remain separate.
--clean rebuilds only the selected units; keeps SDKs and other configurations.
RGB565 skips scene_baker; girl_viewer runs only for RGB888/index32.
Executables: build/bin/<format>/index<bits>; caches: build/.cache/configs/.
HELP
}
while (($#)); do
    case "$1" in
        --depth|--index-bits|--jobs)
            if (($# < 2)); then echo "Missing value for $1" >&2; exit 2; fi
            case "$1" in
                --depth) ymgre_depth="$2" ;;
                --index-bits) ymgre_bits="$2" ;;
                --jobs) ymgre_jobs="$2" ;;
            esac
            shift 2 ;;
        -c|--clean) ymgre_clean=1; shift ;;
        -t|--test) ymgre_test=1; shift ;;
        --core-only) ymgre_core_only=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done
case "$ymgre_depth" in 16) ymgre_rgb=rgb565 ;; 24) ymgre_rgb=rgb888 ;; *) echo 'Depth must be 16 or 24' >&2; exit 2 ;; esac
case "$ymgre_bits" in 16|32) ;; *) echo 'Index bits must be 16 or 32' >&2; exit 2 ;; esac
if ! [[ "$ymgre_jobs" =~ ^[1-9][0-9]*$ ]]; then echo 'Jobs must be a positive integer' >&2; exit 2; fi
ymgre_base="$ymgre_root/build/.cache/configs/$ymgre_rgb/index$ymgre_bits"
ymgre_pass=()
ymgre_fail=()
ymgre_skip=()
build_unit() {
    local label="$1" src="$2" bdir="$3"
    shift 3
    echo "==> $label -> $bdir"
    if ((ymgre_clean)); then
        # Refuse redirected paths before any removal; never clear build/ itself.
        if [[ "$(realpath -m -- "$bdir")" != "$bdir" || "$bdir" != "$ymgre_base/"* ]]; then
            echo "Refusing to clean redirected path: $bdir" >&2
            ymgre_fail+=("$label (clean)"); return 1
        fi
        if ! rm -rf -- "$bdir"; then ymgre_fail+=("$label (clean)"); return 1; fi
    fi
    if ! cmake -S "$src" -B "$bdir" -DCMAKE_BUILD_TYPE=Release \
        -DYMGRE_CAMERA_COLOR_DEPTH="$ymgre_depth" -DYMGRE_INDEX_BITS="$ymgre_bits" "$@"; then
        ymgre_fail+=("$label (configure)"); return 1
    fi
    if ! cmake --build "$bdir" -j "$ymgre_jobs"; then
        ymgre_fail+=("$label (build)"); return 1
    fi
    ymgre_pass+=("$label")
}
ymgre_unit=Demo
ymgre_top_args=(-DYMGRE_BUILD_DEMOS=ON -DYMGRE_BUILD_YMGUI_HOST=ON -DYMGRE_BUILD_TESTS=ON)
if ((ymgre_core_only)); then
    ymgre_unit=core
    ymgre_top_args=(-DYMGRE_BUILD_DEMOS=OFF -DYMGRE_BUILD_YMGUI_HOST=OFF -DYMGRE_BUILD_TESTS=ON)
fi
if build_unit "$ymgre_unit" "$ymgre_root" "$ymgre_base/$ymgre_unit" "${ymgre_top_args[@]}"; then
    if ((ymgre_test)); then
        if ctest --test-dir "$ymgre_base/$ymgre_unit" --output-on-failure; then
            ymgre_pass+=("core tests")
        else
            ymgre_fail+=("core tests")
        fi
    fi
fi
if ((!ymgre_core_only)); then
    for ymgre_cml in "$ymgre_root"/project_Demo/*/CMakeLists.txt; do
        [[ -f "$ymgre_cml" ]] || continue
        ymgre_src="${ymgre_cml%/CMakeLists.txt}"
        ymgre_name="${ymgre_src##*/}"
        if [[ "$ymgre_name" == girl_viewer ]]; then
            if [[ "$ymgre_depth" != 24 || "$ymgre_bits" != 32 ]]; then
                ymgre_skip+=("girl_viewer (requires RGB888/index32)"); continue
            fi
            ymgre_girl_args=(--jobs "$ymgre_jobs")
            if ((ymgre_clean)); then ymgre_girl_args+=(--clean); fi
            if "$ymgre_src/build.sh" "${ymgre_girl_args[@]}"; then
                ymgre_pass+=("girl_viewer")
            else
                ymgre_fail+=("girl_viewer (build)")
            fi
            continue
        fi
        if [[ "$ymgre_name" == scene_baker && "$ymgre_depth" != 24 ]]; then
            ymgre_skip+=("scene_baker (requires RGB888)"); continue
        fi
        build_unit "$ymgre_name" "$ymgre_src" "$ymgre_base/project_Demo/$ymgre_name"
    done
fi
printf '\nBuild summary (%s / index%s)\n' "$ymgre_rgb" "$ymgre_bits"
for ymgre_item in "${ymgre_pass[@]}"; do printf 'PASS %s\n' "$ymgre_item"; done
for ymgre_item in "${ymgre_skip[@]}"; do printf 'SKIP %s\n' "$ymgre_item"; done
for ymgre_item in "${ymgre_fail[@]}"; do printf 'FAIL %s\n' "$ymgre_item"; done
if ((!ymgre_core_only)); then
    printf 'Programs: %s/build/bin/%s/index%s\n' "$ymgre_root" "$ymgre_rgb" "$ymgre_bits"
fi
((${#ymgre_fail[@]} == 0))
