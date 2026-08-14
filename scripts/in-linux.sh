#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "usage: $0 <machine> <command>" >&2
    exit 2
fi

machine="$1"
shift
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
orbctl start "$machine" >/dev/null 2>&1 || true
orb -m "$machine" -u root bash -lc "cd '$repo' && $*"
