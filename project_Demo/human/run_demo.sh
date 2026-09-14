#!/usr/bin/env bash
set -euo pipefail
human_demo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
human_demo_build="$human_demo_root/../../build-human"
cmake -S "$human_demo_root" -B "$human_demo_build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$human_demo_build" --target demo_human -j2
cd "$human_demo_build"
exec ./demo_human "$@"
