# HANDOFF: llvm-ir-caii — RTEMS 6.0 / GR740 LLVM IR 정적 분석 도구

## Goal

RTEMS 6.0 + GR740(SPARC V8, LEON4) 환경을 대상으로 하는 **LLVM IR API 기반 정적 분석 도구** 개발.
CAAS 논문의 Cache Affinity Indicator(CA) 분석 등 IR 레벨 분석 Pass를 확장해나가는 것이 최종 목적이다.

## 환경 요약

| 항목 | 경로 / 버전 |
|---|---|
| LLVM 빌드 | `/workspace/llvm-project/build` (23.0.0git, Sparc + X86 타겟) |
| RTEMS 6 툴체인 | `/opt/rtems/6/bin/sparc-rtems6-gcc` (GCC 13.3.0) |
| GR740 sysroot | `/opt/rtems/6/sparc-rtems6/gr740/lib/include` |
| pkg-config | `/opt/rtems/6/lib/pkgconfig/sparc-rtems6-gr740.pc` |
| clangd (VSCode) | `/usr/bin/clangd-14` |

## 프로젝트 구조

```
/workspace/llvm-ir-caii/
├── .clangd                         # VSCode clangd 설정 (RTEMS 헤더 경로)
├── CMakeLists.txt                  # LLVM 링크 설정 (find_package 방식)
├── build/                          # cmake 빌드 출력
│   ├── caii-analyzer               # 스탠드얼론 분석기 실행 파일
│   ├── caii_plugin.so              # opt --load-pass-plugin 플러그인
│   └── compile_commands.json       # C++ 분석기 소스용 (RTEMS C 소스는 미포함)
├── include/caii/
│   ├── IRLoader.hpp                # .bc/.ll 로드 인터페이스
│   └── AnalysisPass.hpp            # Pass 기반 인터페이스 + Diagnostic 타입
├── src/
│   ├── IRLoader.cpp
│   ├── main.cpp                    # CLI 분석기 (--pass 옵션으로 필터 가능)
│   ├── PluginEntry.cpp             # opt 플러그인 진입점 (caii-all pass)
│   └── passes/
│       ├── NullDerefChecker.cpp    # malloc 반환값 null 체크 누락, null 직접 역참조
│       ├── StackUsageAnalyzer.cpp  # alloca 합산 → 스택 사용량 추정 (경고 2KB, 오류 4KB)
│       └── RTEMSAPIChecker.cpp     # ISR에서 블로킹 API 호출, delete(SELF) 후 코드, 반환값 무시
├── scripts/
│   ├── env.sh                      # PATH/LLVM_DIR 환경변수 (source 해서 사용)
│   ├── build.sh                    # cmake 빌드 래퍼
│   ├── compile-to-ir.sh            # C → SPARC RTEMS6 LLVM IR (.bc + .ll)
│   └── analyze.sh                  # 분석기 실행 래퍼
└── examples/
    ├── hello_rtems.c               # 결함 3종 포함 예제 소스
    ├── hello_rtems.bc              # 생성된 LLVM bitcode
    └── hello_rtems.ll              # 생성된 human-readable IR
```

## Current Progress

### 기본 개발환경 구축 완료

- [x] CMake 프로젝트 설정 (LLVM 23 링크)
- [x] 빌드 성공: `caii-analyzer`, `caii_plugin.so`
- [x] SPARC RTEMS6 타겟 IR 생성 성공 (`hello_rtems.bc`)
- [x] 분석기 실행 및 결과 확인 (5개 진단 탐지)
- [x] VSCode clangd에서 RTEMS 헤더 인식 (`/workspace/llvm-ir-caii/.clangd`)
- [x] README.md 작성 (`/workspace/llvm-ir-caii/README.md`)

### CAII 파이프라인 설계 완료

- [x] GR740 캐시 하드웨어 상수 정의 (`include/CacheConfig.hpp`, N_LLC=4 확정)
- [x] 헤더 확장자 `.h` → `.hpp` 통일
- [x] 아키텍처 설계 문서 작성 (`docs/ARCHITECTURE.md`)

## 사용법

```bash
# 1. 환경변수 설정
source /workspace/llvm-ir-caii/scripts/env.sh

# 2. 빌드
cd /workspace/llvm-ir-caii/build
cmake .. -DLLVM_DIR=/workspace/llvm-project/build/lib/cmake/llvm -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j$(nproc)

# 3. RTEMS 소스 → IR 변환
source scripts/env.sh
scripts/compile-to-ir.sh examples/hello_rtems.c

# 4. 분석 실행
build/caii-analyzer examples/hello_rtems.bc
build/caii-analyzer examples/hello_rtems.bc --pass RTEMSAPIChecker   # 특정 pass만

# 5. opt 플러그인으로 실행
opt --load-pass-plugin=build/caii_plugin.so --passes="caii-all" examples/hello_rtems.bc -o /dev/null
```

## What Worked

### IR 생성 (Clang + RTEMS 헤더 충돌 해결)

GCC의 `stdatomic.h`가 Clang의 `_Atomic` 구현과 충돌하는 문제를 해결:

```bash
# 핵심: GCC include 디렉토리(stdatomic.h 포함)를 탐색 경로에서 제외
# → Clang의 stdatomic.h가 has_include_next 실패 → 자체 구현 사용
clang \
    --target=sparc-unknown-rtems6 \
    $(pkg-config --cflags /opt/rtems/6/lib/pkgconfig/sparc-rtems6-gr740.pc) \
    -isystem /workspace/llvm-project/build/lib/clang/23/include \
    -isystem /opt/rtems/6/lib/gcc/sparc-rtems6/13.3.0/include-fixed \
    -isystem /opt/rtems/6/sparc-rtems6/include \
    -D__leon__ -D__sparc_v8__ \
    -g -O1 -emit-llvm -c -o out.bc src.c
```

compile-to-ir.sh에 이 로직이 구현되어 있다.

### CMakeLists — `sparc` 컴포넌트 제외

`llvm_map_components_to_libnames`에 `sparc` 컴포넌트를 넣으면 `-lLLVMsparc` (소문자)를 찾아 링크 실패.
IR 레벨 분석은 Sparc 백엔드 코드젠 라이브러리가 불필요하므로 제거 → 링크 성공.

### VSCode clangd — `.clangd` 파일 방식

`compile_commands.json`이 `build/`에 있어 clangd가 RTEMS `.c` 파일에도 호스트 C++ 플래그를 적용하는 문제:
- `settings.json`의 `fallbackFlags`는 `compile_commands.json`이 없는 디렉토리(`experiments/`)에만 효과
- `.clangd` 파일로 `PathMatch: .*\.c$` 조건에 RTEMS 플래그 주입

## What Didn't Work

| 시도 | 실패 이유 |
|---|---|
| `--sysroot` 단독 사용 | `stddef.h` not found — Clang이 sysroot 안에서 컴파일러 런타임 헤더를 찾지 못함 |
| `--gcc-toolchain=/opt/rtems/6` | Clang 23에서 무시됨 (`argument unused` 경고) |
| GCC include + Clang 함께 | GCC `stdatomic.h`가 `include_next`로 선택되어 `_Atomic` 충돌 |
| `__STDC_HOSTED__=0` | Clang `limits.h`가 `include_next` 스킵 → `SSIZE_MAX` 미정의 오류 |
| CMakeLists에 `sparc` 컴포넌트 | 링크 시 `-lLLVMsparc` (소문자) 탐색 → 파일 없음 |
| `llvm/Passes/PassPlugin.h` | LLVM 23에서 경로가 `llvm/Plugins/PassPlugin.h`로 변경됨 |

## Next Steps

### Phase 1 — 헤더 정의 (인터페이스 확정)

1. `include/caii/CacheTypes.hpp` — BBAccessMap, AddressRange, TaskMeta
2. `include/caii/CAIResult.hpp` — δ_i^j, r_i^j, CA_i 결과 타입
3. `include/caii/CAIIResult.hpp` — CAII_static + 중간 결과 전체
4. `include/caii/passes/MemAccessCollectorPass.hpp` ~ `CAIComputePass.hpp` (6개)
5. `include/caii/CacheAnalysisPipeline.hpp` — 파이프라인 오케스트레이터

### Phase 2 — P1, P2 구현 (기반 데이터 수집)

6. `MemAccessCollectorPass.cpp` — Load/Store 순회, 주소 범위 3단계 해석
7. `ECBExtractorPass.cpp` — BBAccessMap 합산 (경로 독립 over-approximation)

### Phase 3 — P3, P4 구현 (UCB/HB 분석)

8. `UCBDataflowPass.cpp` — Forward Must 분석 + Backward GEN/KILL worklist
9. `HBExtractorPass.cpp` — Conservative(기본) / TailAccess 전략

### Phase 4 — P5 + CAI 구현 (지표 계산)

10. `InterferenceComputePass.cpp` — 집합별 LRU 충돌 분석, I_i^{c,static}
11. `CAIComputePass.cpp` — CFG 경로 열거, 재사용 거리, CA_i 수식

### Phase 5 — 파이프라인 통합

12. `CacheAnalysisPipeline.cpp` — 오케스트레이터, JSON 직렬화
13. `caii-analyzer` CLI 옵션 연동 (`--task-meta`, `--linker-map`, `--brt`, `--output`)
14. 실험 소스 분석: `/workspace/experiments/cache-interference/` exp1~exp5

### 기타 개선

- **NullDerefChecker**: 인터프로시저 분석 추가
- **StackUsageAnalyzer**: 콜 그래프 기반 누적 스택 계산
- **clangd**: `.clangd`의 `Remove: [-m*, --target=*]` 패턴 clangd-14 동작 확인

## 참조

- **아키텍처 설계서**: `/workspace/llvm-ir-caii/docs/ARCHITECTURE.md`
- 실험 설계 문서: `/workspace/cache-interference-experiments.md`
- 캐시 간섭 실험 handoff: `/workspace/HANDOFF.md`
- 공통 빌드 시스템: `/workspace/experiments/common.mk`
- CAAS 논문: `/workspace/CAAS Cache Affinity Aware Scheduling Framework for RTEMS with Edge Computing Support.pdf`
- 기존 CAAS 논문의 CAIC 지표: `/workspace/CAI 지표 정리.pdf`
- CAII 지표 정리 (GR740 확정값): `/workspace/CAII 지표 정리 (수정본 — GR740 확정값 반영)_apa.pdf`
- LLVM IR 패스 설계서: `/workspace/LLVM IR 기반 캐시 간섭 지표 추출 패스 설계서 (GR740)_apa.pdf`