#!/usr/bin/env bash
# RTEMS 6.0 + GR740 정적 분석 개발환경 설정
# 사용법: source scripts/env.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# ── LLVM (빌드된 trunk) ──────────────────────────────────────────────
LLVM_BUILD=/workspace/llvm-project/build
export PATH="$LLVM_BUILD/bin:$PATH"
export LLVM_DIR="$LLVM_BUILD/lib/cmake/llvm"

# ── RTEMS 6 툴체인 ───────────────────────────────────────────────────
RTEMS_ROOT=/opt/rtems/6
export PATH="$RTEMS_ROOT/bin:$PATH"

# ── GR740 sysroot (헤더/라이브러리 참조용) ────────────────────────────
export RTEMS_SYSROOT="$RTEMS_ROOT/sparc-rtems6/gr740"

# ── 편의 alias ──────────────────────────────────────────────────────
export CAII_CLANG="$LLVM_BUILD/bin/clang"
export CAII_TARGET="sparc-unknown-rtems6"
export CAII_OPT="$LLVM_BUILD/bin/opt"

echo "[caii] LLVM  : $(llvm-config --version)"
echo "[caii] clang : $(clang --version | head -1)"
echo "[caii] target: $CAII_TARGET"
echo "[caii] sysroot: $RTEMS_SYSROOT"