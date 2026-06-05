#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

scripts/rebuild_docs_site.py

if [[ -n "${HUGO_BIN:-}" ]]; then
  HUGO=("$HUGO_BIN")
elif command -v hugo >/dev/null 2>&1; then
  HUGO=(hugo)
else
  echo "hugo was not found. Install it with: brew install hugo" >&2
  exit 127
fi

"${HUGO[@]}" --gc --minify --cleanDestinationDir --source docs-site --destination ../public "$@"
