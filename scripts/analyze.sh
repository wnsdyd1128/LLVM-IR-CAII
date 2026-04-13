#!/usr/bin/env bash
# 분석 실행 래퍼
# 사용법: scripts/analyze.sh <file.bc> [--pass NullDerefChecker] ...

set -euo pipefail

ANALYZER="${PROJECT_ROOT:-$(dirname "$0")/..}/build/caii-analyzer"

if [[ ! -x "$ANALYZER" ]]; then
    echo "ERROR: 먼저 빌드하세요 (scripts/build.sh)"
    exit 1
fi

"$ANALYZER" "$@"