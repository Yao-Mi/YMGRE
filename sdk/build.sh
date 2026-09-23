#!/usr/bin/env bash
# Run from any directory; --release writes the verified archive to releases/.
set -euo pipefail
ymgre_sdk_source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$ymgre_sdk_source_dir/package.py" "$@"
