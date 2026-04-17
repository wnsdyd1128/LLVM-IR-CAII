# llvm-ir-caii

RTEMS 6.0 + GR740(SPARC V8, LEON4FT) 환경을 대상으로 하는 **LLVM IR 기반 캐시 지표 정적 분석 도구**.

C 소스를 SPARC RTEMS6 타겟 LLVM IR로 변환하고, IR 레벨 Analysis Pass로 다음 목적을 달성한다:

**CAII 지표 추출** — Reuse Distance·CA_i·CAII_i^static 을 완전 정적으로 계산하여
CAAS 스케줄링 아키텍처(global / clustered / partitioned) 선택 기준으로 제공한다.

## 사전 요구사항

| 항목 | 경로 |
|---|---|
| LLVM 빌드 | `/workspace/llvm-project/build` (23.0.0git, Sparc 타겟 포함) |
| RTEMS 6 툴체인 | `/opt/rtems/6` (`sparc-rtems6-gcc` 13.3.0) |
| GR740 BSP | `/opt/rtems/6/sparc-rtems6/gr740` |

## 빠른 시작

```bash
# 1. 환경 변수 설정
source scripts/env.sh

# 2. 빌드
scripts/build.sh

# 3. RTEMS C 소스 → LLVM IR 변환
scripts/compile-to-ir.sh examples/hello_rtems.c
# → examples/hello_rtems.bc (bitcode)
# → examples/hello_rtems.ll (human-readable IR)

# 4. 분석 실행
build/caii-analyzer examples/hello_rtems.bc
```

## 프로젝트 구조

```
├── CMakeLists.txt
├── docs/
│   └── ARCHITECTURE.md          # CAII 파이프라인 아키텍처 설계서
├── include/
│   ├── CacheConfig.hpp           # GR740 L2 캐시 HW 상수 (N_LLC=4, S=16384, b=32B)
│   ├── IRLoader.hpp              # .bc / .ll 파일 로드
│   ├── CacheTypes.hpp            # (예정) BBAccessMap, AddressRange, TaskMeta
│   ├── CAIResult.hpp             # (예정) δ_i^j, r_i^j, CA_i 결과 타입
│   ├── CAIIResult.hpp            # (예정) CAII_static + 중간 결과 전체
│   ├── CacheAnalysisPipeline.hpp # (예정) 파이프라인 오케스트레이터
│   ├── MemAccessCollectorPass.hpp# (예정) P1
│   ├── ECBExtractorPass.hpp      # (예정) P2
│   ├── UCBDataflowPass.hpp       # (예정) P3
│   ├── HBExtractorPass.hpp       # (예정) P4
│   ├── InterferenceComputePass.hpp# (예정) P5
│   └── CAIComputePass.hpp        # (예정) CA_i
├── src/
│   ├── main.cpp                  # 스탠드얼론 분석기 CLI
│   ├── IRLoader.cpp
│   └── passes/                   # (예정) P1~P5, CAI 패스 구현
├── scripts/
│   ├── env.sh                    # 환경변수 (source 필요)
│   ├── build.sh                  # cmake 빌드 래퍼
│   ├── compile-to-ir.sh          # C → SPARC IR 변환
│   └── analyze.sh                # 분석기 실행 래퍼
└── examples/
    └── hello_rtems.c             # RTEMS 태스크 예제
```

## CAII 지표 파이프라인

CAAS 논문의 CA_i에 크로스-코어 캐시 간섭(I^c)과 선점 지연(CRPD)을 추가한 복합 지표.

```
CAII_i^static = CA_i / (1 + Ī_i^{c,static} + CRPD̄_i^static)
```

| 패스 | 이름 | 출력 | 설명 |
|---|---|---|---|
| P1 | `MemAccessCollectorPass` | BBAccessMap | BB별 접근 캐시 블록 집합 (Reuse Distance 원천 데이터) |
| P2 | `ECBExtractorPass` | ECB_i | 태스크 Evicting Cache Block 전체 합집합 |
| P3 | `UCBDataflowPass` | UCB_i(p), max UCB | Forward Must + Backward GEN/KILL 데이터플로우 |
| P4 | `HBExtractorPass` | HB_i | 다음 주기 시작 시 히트 기대 블록 |
| P5 | `InterferenceComputePass` | interference_matrix[n×n] | 크로스-코어 간섭 행렬 (코어 배치 독립) |
| CAI | `CAIComputePass` | CA_i | Reuse Distance 기반 캐시 친화도 ∈ [0, 1] |

GR740 캐시 파라미터: **N_LLC=4**, S=16,384 sets, b=32B, L2=2MiB (→ [`include/CacheConfig.hpp`](include/CacheConfig.hpp))

자세한 설계: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

## 사용법

```bash
build/caii-analyzer 
# → 모듈 로딩 확인 (CacheAnalysisPipeline 구현 예정)
```

## IR 생성 방법 (크로스 컴파일 상세)

RTEMS 헤더와 Clang 23의 `_Atomic` 구현 충돌 문제로, GCC의 `include` 디렉토리를 탐색 경로에서 제외해야 한다. `compile-to-ir.sh`에 이 로직이 내장되어 있다.

```bash
# compile-to-ir.sh 내부 핵심 플래그
clang --target=sparc-unknown-rtems6 \
  -isystem /include   # Clang 자체 stdatomic.h
  -isystem /include-fixed        # limits.h 보정
  -isystem /opt/rtems/6/sparc-rtems6/include \
  -isystem /opt/rtems/6/sparc-rtems6/gr740/lib/include \
  -D__leon__ -D__sparc_v8__ \
  -g -O1 -emit-llvm -c -o out.bc src.c
```

> GCC `include/` 디렉토리를 포함하면 GCC의 `stdatomic.h`가 선택되어
> `_Atomic` 타입 불일치 오류가 발생한다.

`/workspace/llvm-ir-caii/.clangd`가 clangd에 RTEMS 크로스 컴파일 플래그를 제공한다.

`*.c` 파일에 `rtems.h not found` 오류가 표시되면 `Ctrl+Shift+P` → **clangd: Restart language server**.
