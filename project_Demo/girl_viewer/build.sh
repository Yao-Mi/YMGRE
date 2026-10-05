#!/usr/bin/env bash
# Source-repository entry. SDK consumers use the adjacent standalone CMake project.
set -euo pipefail
ymgre_girl_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
ymgre_girl_cache="$ymgre_girl_root/build/.cache/configs/rgb888/index32/project_Demo/girl_viewer"
ymgre_girl_output="$ymgre_girl_root/build/bin/rgb888/index32"
ymgre_girl_jobs="${YMGRE_BUILD_JOBS:-4}"
ymgre_girl_test=OFF
ymgre_girl_run=0
ymgre_girl_clean=0
usage() {
 cat <<'HELP'
Usage: ./project_Demo/girl_viewer/build.sh [--jobs N] [--clean] [--test] [--run [-- viewer-options]]
Builds RGB888/index32 with all material and image decoder features enabled.
Output: build/bin/rgb888/index32/girl_viewer; cache: build/.cache/configs/rgb888/index32/project_Demo/girl_viewer.
--test runs core tests and a headless girl_viewer smoke check.
--clean removes only this viewer's cache; --run starts the built viewer.
HELP
}
while (($#)); do
 case "$1" in
  --jobs) if (($#<2)); then usage >&2; exit 2; fi; ymgre_girl_jobs="$2"; shift 2 ;;
  --test) ymgre_girl_test=ON; shift ;;
  --run) ymgre_girl_run=1; shift ;;
  --clean) ymgre_girl_clean=1; shift ;;
  -h|--help) usage; exit 0 ;;
  --) shift; break ;;
  *) usage >&2; exit 2 ;;
 esac
done
[[ "$ymgre_girl_jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'Jobs must be a positive integer' >&2; exit 2; }
if (($# && !ymgre_girl_run)); then echo 'Viewer options require --run' >&2; exit 2; fi
if ((ymgre_girl_clean)); then
 [[ "$(realpath -m -- "$ymgre_girl_cache")" == "$ymgre_girl_cache" ]] || { echo 'Refusing redirected cache path' >&2; exit 2; }
 rm -rf -- "$ymgre_girl_cache"
fi
cmake -S "$ymgre_girl_root" -B "$ymgre_girl_cache" -DCMAKE_BUILD_TYPE=Release \
 -DYMGRE_BUILD_DEMOS=OFF -DYMGRE_BUILD_YMGUI_HOST=OFF -DYMGRE_BUILD_TESTS="$ymgre_girl_test" \
 -DYMGRE_INDEX_BITS=32 -DYMGRE_CAMERA_COLOR_DEPTH=24 \
 -DYMGRE_ENABLE_PBR=ON -DYMGRE_ENABLE_TRANSPARENCY=ON -DYMGRE_ENABLE_OPACITY_MIPMAP=ON \
 -DYMGRE_ENABLE_LINEAR_COLOR=ON -DYMGRE_ENABLE_RASTER_DISPATCH=ON \
 -DYMGRE_ENABLE_PNG=ON -DYMGRE_ENABLE_JPEG=ON -DYMGRE_BUILD_GIRL_VIEWER=ON \
 -DYMGRE_PROGRAM_OUTPUT_DIR="$ymgre_girl_output"
if [[ "$ymgre_girl_test" == ON ]]; then
 cmake --build "$ymgre_girl_cache" -j "$ymgre_girl_jobs"
 ctest --test-dir "$ymgre_girl_cache" --output-on-failure
 mkdir -p "$ymgre_girl_cache/captures"
 SDL_VIDEODRIVER=dummy "$ymgre_girl_output/girl_viewer" --smoke-test --capture-dir "$ymgre_girl_cache/captures"
else
 cmake --build "$ymgre_girl_cache" --target girl_viewer -j "$ymgre_girl_jobs"
fi
printf '\nViewer: %s/girl_viewer\n' "$ymgre_girl_output"
if ((ymgre_girl_run)); then exec "$ymgre_girl_output/girl_viewer" "$@"; fi
