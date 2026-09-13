#!/usr/bin/env bash
# Run from any directory; produces build/YMGRE_libs and a .tar.gz release.
set -euo pipefail
ymgre_sdk_source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$ymgre_sdk_source_dir/package.py" "$@"
