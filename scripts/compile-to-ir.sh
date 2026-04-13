#!/usr/bin/env bash
# RTEMS 6.0 / GR740(SPARC V8) 대상 소스를 LLVM IR(.bc/.ll)로 컴파일
#
# 사용법:
#   source scripts/env.sh          # 환경변수 설정 먼저
#   scripts/compile-to-ir.sh <src.c> [<out.bc>]
#
# 결과: <out.bc> (bitcode) + <out.ll> (human-readable IR)

set -euo pipefail

if [[ -z "${CAII_CLANG:-}" ]]; then
    echo "ERROR: env.sh 를 먼저 source 하세요."
    exit 1
fi

SRC="${1:?Usage: $0 <src.c> [out.bc]}"
BASE="${SRC%.c}"
OUT="${2:-${BASE}.bc}"
OUT_LL="${OUT%.bc}.ll"

RTEMS_ROOT=/opt/rtems/6
# pkg-config 에서 ABI 플래그(-mcpu=leon3 -isystem<bsp_include>) 추출
ABI_FLAGS=$(pkg-config --cflags "${RTEMS_ROOT}/lib/pkgconfig/sparc-rtems6-gr740.pc")

# 헤더 탐색 경로 (순서 중요)
#   1. Clang 리소스 헤더 (stddef.h, stdatomic.h 등 Clang 자체 구현)
#   2. GCC include-fixed (limits.h 보정값)
#   3. RTEMS sparc-rtems6 공통 헤더 (limits.h, sys/*.h 등)
# ※ GCC include 디렉토리(stdatomic.h 포함)는 제외 →
#    Clang의 stdatomic.h 가 has_include_next 실패 → 자체 구현 사용
CLANG_INC="${LLVM_BUILD:-/workspace/llvm-project/build}/lib/clang/23/include"
GCC_INC_FIX=$("${RTEMS_ROOT}/bin/sparc-rtems6-gcc" -mcpu=leon3 -print-file-name=include-fixed)
SPARC_SYS_INC="${RTEMS_ROOT}/sparc-rtems6/include"

echo "[compile-to-ir] ${SRC} → ${OUT}"

"$CAII_CLANG" \
    --target="$CAII_TARGET" \
    $ABI_FLAGS \
    -isystem "$CLANG_INC" \
    -isystem "$GCC_INC_FIX" \
    -isystem "$SPARC_SYS_INC" \
    -D__leon__ -D__sparc_v8__ \
    -g -O1 \
    -emit-llvm -c \
    -o "$OUT" \
    "$SRC"

# 사람이 읽을 수 있는 IR도 함께 생성
"${LLVM_BUILD:-/workspace/llvm-project/build}/bin/llvm-dis" "$OUT" -o "$OUT_LL"

echo "[compile-to-ir] 완료: ${OUT}  ${OUT_LL}"