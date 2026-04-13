#!/usr/bin/env bash
# 프로젝트 빌드
# 사용법: scripts/build.sh [clean]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

if [[ "${1:-}" == "clean" ]]; then
    rm -rf "$BUILD_DIR"
    echo "[build] 빌드 디렉토리 초기화"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_ROOT" \
    -DLLVM_DIR="${LLVM_DIR:?env.sh 를 먼저 source 하세요}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build . -- -j"$(nproc)"

echo ""
echo "[build] 완료"
echo "  분석기: $BUILD_DIR/caii-analyzer"
echo "  플러그인: $BUILD_DIR/caii_plugin.so"