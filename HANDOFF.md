# HANDOFF: llvm-ir-caii — RTEMS 6.0 / GR740 LLVM IR 캐시 친화도 분석 도구

## 프로젝트 한 줄 요약

RTEMS 6.0 + GR740(SPARC V8, LEON4FT) 환경에서 LLVM IR 기반으로 CAII_i^static 지표를
정적 추출하여, CAAS 논문의 CA_i를 대체하는 더 정밀한 스케줄링 배치 결정 지표를 제공한다.

CAAS 논문은 CA_i 단독으로 스케줄링 아키텍처(global/partitioned)를 선택했으나,
태스크 간 캐시 간섭을 무시한다는 한계가 있다.
본 프로젝트는 크로스-코어 간섭과 선점 지연을 추가 반영한 CAII_i^static으로 CA_i를 대체한다.

```
CAII_i^static = CA_i / (1 + Ī_i^{c,static} + CRPD̄_i^static)
```

설계 핵심 — 순환 의존 해소:

- CAII 계산에 코어 배치가 필요하고, 코어 배치 결정에 CAII가 필요한 순환이 존재한다.
- n×n 간섭 행렬을 IR 분석 단계에서 코어 배치 없이 미리 계산해두면,
  배치 후보가 바뀔 때마다 행렬 합산(O(n²))만 재수행하면 되므로 순환이 끊긴다.

2단계 파이프라인:

1. Stage 1 (1회): IR 분석 → CA_i + interference_matrix[n×n] (코어 배치 불필요)
2. Stage 2 (반복): CA_i 휴리스틱으로 초기 배치 → CAII로 평가 및 개선 → 수렴

> 초기 환경 검증용이던 결함 탐지(NullDeref 등)는 범위에서 제거되었다.

## 환경

| 항목 | 경로 |
|---|---|
| LLVM 빌드 | `/workspace/llvm-project/build` (23.0.0git, Sparc + X86 타겟) |
| RTEMS 6 툴체인 | `/opt/rtems/6/bin/sparc-rtems6-gcc` (GCC 13.3.0) |
| GR740 sysroot | `/opt/rtems/6/sparc-rtems6/gr740/lib/include` |
| pkg-config | `/opt/rtems/6/lib/pkgconfig/sparc-rtems6-gr740.pc` |
| clangd (VSCode) | `/usr/bin/clangd-14` |

## 프로젝트 구조

```
/workspace/llvm-ir-caii/
├── .clangd                          # VSCode clangd 설정 (RTEMS 헤더 경로)
├── CMakeLists.txt                   # LLVM 링크 + GoogleTest/CTest 설정
├── docs/
│   └── ARCHITECTURE.md             # CAII 파이프라인 아키텍처 설계서
├── build/                           # cmake 빌드 출력
│   ├── caii-analyzer                # 스탠드얼론 분석기 실행 파일
│   ├── CacheConfigTest              # 단위 테스트 실행 파일
│   ├── IRLoaderTest                 # 단위 테스트 실행 파일
│   └── compile_commands.json
├── include/
│   ├── CacheConfig.hpp              # GR740 캐시 상수 + 주소→set 매핑 유틸
│   ├── IRLoader.hpp                 # .bc/.ll 로드 인터페이스
│   ├── CacheTypes.hpp               # 예정 — BBAccessMap, AddressRange, TaskMeta
│   ├── CAIResult.hpp                # 예정 — CA_i 결과 타입
│   ├── CAIIResult.hpp               # 예정 — CAII_static + 중간 결과
│   ├── CacheAnalysisPipeline.hpp    # 예정 — 파이프라인 오케스트레이터
│   └── passes/                      # 예정 — P1~P5, CAI 패스 헤더
├── src/
│   ├── IRLoader.cpp                 # 구현 완료
│   ├── main.cpp                     # CLI 스켈레톤 (TODO: CacheAnalysisPipeline 호출)
│   └── passes/                      # 예정 — P1~P5, CAI 패스 구현
├── scripts/
│   ├── env.sh                       # PATH/LLVM_DIR 환경변수
│   ├── build.sh                     # cmake 빌드 래퍼
│   ├── compile-to-ir.sh             # C → SPARC RTEMS6 LLVM IR (.bc + .ll)
│   └── analyze.sh                   # 분석기 실행 래퍼
├── examples/
│   ├── hello_rtems.c                # 예제 RTEMS 태스크 소스
│   ├── hello_rtems.bc               # 생성된 LLVM bitcode
│   └── hello_rtems.ll               # 생성된 human-readable IR
└── tests/
    ├── CMakeLists.txt               # GoogleTest 타깃 등록
    ├── fixtures/
    │   ├── invalid.ll               # 오류 처리 검증용 fixture
    │   └── minimal.ll               # 최소 유효 IR fixture
    └── unit/
        ├── CacheConfigTest.cpp      # GR740 캐시 상수/매핑 테스트
        └── IRLoaderTest.cpp         # IRLoader 성공/실패 케이스 테스트
```

## Current Progress

### 기본 개발환경 구축 완료

- [x] CMake 프로젝트 설정
- [x] LLVM 23 링크 성공
- [x] `caii-analyzer` 빌드 성공
- [x] SPARC RTEMS6 타깃 IR 생성 성공 (`examples/hello_rtems.bc`)
- [x] VSCode clangd에서 RTEMS 헤더 인식 (`.clangd`)

### CAII 파이프라인 설계 완료

- [x] GR740 캐시 하드웨어 상수 정의 (`include/CacheConfig.hpp`)
- [x] 헤더 확장자 `.h` → `.hpp` 통일
- [x] 아키텍처 설계 문서 작성 (`docs/ARCHITECTURE.md`)

### 테스트 기반 추가 완료

- [x] `CMakeLists.txt`에 GoogleTest/CTest 연결
- [x] `tests/unit/CacheConfigTest.cpp` 추가
- [x] `tests/unit/IRLoaderTest.cpp` 추가
- [x] `tests/fixtures/minimal.ll`, `tests/fixtures/invalid.ll` 추가
- [x] `ctest --test-dir build --output-on-failure` 통과 확인

### 미완료

- [ ] 모든 패스 헤더/구현 파일 (P1~P5, CAI, 파이프라인)
- [ ] `main.cpp`의 `CacheAnalysisPipeline::run()` 연동
- [ ] 결과 직렬화(JSON 등) 및 실제 CLI 인자 흐름 완성

## Git 상태 (2026-04-14 UTC 기준)

```
M CMakeLists.txt
M include/CacheConfig.hpp
```

- 테스트 하네스와 캐시 설정 관련 변경이 아직 커밋되지 않았다.
- 다음 에이전트는 이 상태를 현재 작업 기준선으로 간주하면 된다.
- 이 변경들은 사용자 또는 이전 에이전트의 진행 중 작업일 수 있으므로, 명시 요청 없이 되돌리지 말 것.

## 마지막 확인된 빌드 및 테스트 상태

2026-04-14 UTC에 아래 명령으로 확인했다.

```bash
cmake --build /workspace/llvm-ir-caii/build -- -j2
ctest --test-dir /workspace/llvm-ir-caii/build --output-on-failure
```

- `CacheConfigTest` 통과
- `IRLoaderTest` 통과
- 총 2개 테스트, 실패 0개

## 빠른 재현 순서

```bash
source /workspace/llvm-ir-caii/scripts/env.sh
cd /workspace/llvm-ir-caii/build
cmake .. -DLLVM_DIR=/workspace/llvm-project/build/lib/cmake/llvm -DCMAKE_BUILD_TYPE=Debug
cmake --build . -- -j$(nproc)

# 3. RTEMS 소스 → IR 변환
/workspace/llvm-ir-caii/scripts/compile-to-ir.sh /workspace/llvm-ir-caii/examples/hello_rtems.c

# 4. 분석 실행 (현재: 모듈 정보만 출력)
/workspace/llvm-ir-caii/build/caii-analyzer /workspace/llvm-ir-caii/examples/hello_rtems.bc

# 6. 완성 후 예정 CLI
/workspace/llvm-ir-caii/build/caii-analyzer task_inlined.bc \
  --task-meta task_config.yaml \
  --linker-map gr740.map \
  --output task_cache_profile.json
```

## Known Issues & 해결책

### IR 생성: Clang + RTEMS 헤더 충돌 해결

GCC의 `stdatomic.h`가 Clang의 `_Atomic` 구현과 충돌하는 문제를 회피했다.

- GCC include 디렉토리(`stdatomic.h` 포함)를 일반 탐색 경로에서 배제
- Clang 내장 헤더 구현을 사용

`compile-to-ir.sh`에 이 로직이 구현되어 있다.

```bash
--target=sparc-unknown-rtems6 \
$(pkg-config --cflags /opt/rtems/6/lib/pkgconfig/sparc-rtems6-gr740.pc) \
-isystem /workspace/llvm-project/build/lib/clang/23/include \
-isystem /opt/rtems/6/lib/gcc/sparc-rtems6/13.3.0/include-fixed \
-isystem /opt/rtems/6/sparc-rtems6/include \
-D__leon__ -D__sparc_v8__ \
-g -O1 -emit-llvm -c -o out.bc src.c
```

### CMake: `sparc` 컴포넌트 제외

`llvm_map_components_to_libnames`에 `sparc`를 넣으면 `-lLLVMsparc`를 찾아 링크 실패한다.
IR 레벨 분석에는 Sparc 백엔드 코드젠 라이브러리가 필요 없으므로 제외하는 것이 맞다.

### VSCode clangd: `.clangd` 파일 방식 사용

`compile_commands.json`이 `build/`에 있어 RTEMS `.c` 파일에 호스트 C++ 플래그가 잘못 적용되는 문제가 있었다.

- `settings.json`의 `fallbackFlags`에 의존하지 않음
- `.clangd`에서 `PathMatch: .*\.c$` 규칙으로 RTEMS 관련 플래그를 직접 주입

### 테스트 하네스 직접 구성

- `/workspace/llvm-project/third-party/unittest/googletest`를 직접 사용해 `gtest`, `gtest_main` 정적 라이브러리 생성
- 외부 패키지 설치 없이 현 워크스페이스만으로 테스트를 빌드할 수 있다
- `tests/CMakeLists.txt`에서 `CAII_SOURCE_DIR`를 컴파일 정의로 주입해 fixture와 example 파일 경로를 안정적으로 참조한다

### What Didn't Work

| 시도 | 실패 원인 |
|---|---|
| `--sysroot` 단독 사용 | `stddef.h` not found — Clang이 sysroot 안에서 컴파일러 런타임 헤더를 찾지 못함 |
| `--gcc-toolchain=/opt/rtems/6` | Clang 23에서 무시됨 (`argument unused` 경고) |
| GCC include + Clang 함께 | GCC `stdatomic.h`가 `include_next`로 선택되어 `_Atomic` 충돌 |
| `__STDC_HOSTED__=0` | Clang `limits.h`가 `include_next` 스킵 → `SSIZE_MAX` 미정의 오류 |
| CMakeLists에 `sparc` 컴포넌트 추가 | 링크 시 `-lLLVMsparc` 탐색 → 파일 없음 |
| `llvm/Passes/PassPlugin.h` 사용 | LLVM 23에서 경로가 `llvm/Plugins/PassPlugin.h`로 변경됨 |

## 다음 작업 권장 순서

### Phase 1 — 헤더 정의 (인터페이스 확정) ← 가장 자연스러운 다음 작업

1. `include/caii/CacheTypes.hpp` 작성
2. `include/caii/CAIResult.hpp` 작성
3. `include/caii/CAIIResult.hpp` 작성
4. `include/caii/passes/MemAccessCollectorPass.hpp` ~ `CAIComputePass.hpp` 작성
5. `include/caii/CacheAnalysisPipeline.hpp` 작성

`docs/ARCHITECTURE.md` §5의 타입 정의를 그대로 코드화하는 것이 가장 빠르다.

1. `CacheTypes.hpp`에서 주소 범위, 블록 집합, 기본 블록별 접근 표현을 먼저 고정
2. `CAIResult.hpp`와 `CAIIResult.hpp`에서 패스 간 데이터 계약을 정리
3. 패스 헤더와 파이프라인 헤더를 추가
4. 최소한 헤더만 포함해도 빌드가 깨지지 않게 `CMakeLists.txt`와 include 경로를 점검

- 현재 테스트는 IR 로더와 캐시 상수만 보호하고 있다.
- 다음 구현 리스크는 알고리즘보다도 타입 경계가 흔들리는 것이다.
- 헤더 계약을 먼저 고정하면 이후 P1~P5 구현의 되돌림 비용이 줄어든다.

### Phase 2 — P1, P2 구현 (기반 데이터 수집)

6. `src/passes/MemAccessCollectorPass.cpp` — Load/Store 순회, 주소 범위 3단계 해석
   - 중요: P0 인라이닝 후 남는 `CallInst`를 3가지로 분류하여 처리할 것
     - 유형 A `callee->isDeclaration()` (RTEMS API, libc) → full-cache fallback + INFO 로그
     - 유형 B `CI->isIndirectCall()` (함수 포인터) → full-cache fallback + WARN 로그
     - 유형 C 직접 호출 + IR 있음 (`always-inline` 실패) → ERROR 진단 발행
   - full-cache fallback = 해당 지점에서 전체 캐시 블록(S × N_LLC = 65,536 lines)을 ECB에 추가 (sound, over-approximation)
7. `src/passes/ECBExtractorPass.cpp` — `BBAccessMap` 합산 (경로 독립 over-approximation)

### Phase 3 — P3, P4 구현 (UCB/HB 분석)

8. `src/passes/UCBDataflowPass.cpp` — Forward Must 분석 + Backward GEN/KILL worklist
9. `src/passes/HBExtractorPass.cpp` — Conservative(기본) 또는 TailAccess 전략

### Phase 4 — P5 + CAI 구현 (지표 계산)

10. `src/passes/InterferenceComputePass.cpp` — n×n 간섭 행렬 계산 (코어 배치 불필요)
    - `interference_matrix[i][k] = |HB_i ∩ CB_k| × ⌈C_i/T_k⌉ × BRT`
      (`|·|` = 집합 기수, 즉 충돌 블록 수)
    - Stage 2에서 크로스-코어 열 합산 → `I_i^{c,static}`
11. `src/passes/CAIComputePass.cpp` — CFG 경로 열거, 재사용 거리, `CA_i` 수식

### Phase 5 — 파이프라인 통합

12. `src/CacheAnalysisPipeline.cpp` — 오케스트레이터, JSON 직렬화
13. `src/main.cpp` — `--task-meta`, `--linker-map`, `--brt`, `--output` 연동
14. `CMakeLists.txt` — 새 소스 파일 추가
15. `/workspace/experiments/cache-interference/`의 실험 소스 분석으로 forward test

## 다음 에이전트에게 바로 유용한 메모

- 시작 순서는 [`docs/ARCHITECTURE.md`](/workspace/llvm-ir-caii/docs/ARCHITECTURE.md:1),
  [`include/CacheConfig.hpp`](/workspace/llvm-ir-caii/include/CacheConfig.hpp:1),
  [`tests/unit/IRLoaderTest.cpp`](/workspace/llvm-ir-caii/tests/unit/IRLoaderTest.cpp:1) 가 가장 효율적이다.
- 새 타입 헤더를 추가할 때는 작은 테스트를 바로 `tests/unit`에 붙여 넣는 편이 안전하다.
- `IRLoaderTest`는 현재 [`examples/hello_rtems.bc`](/workspace/llvm-ir-caii/examples/hello_rtems.bc:1)에도 의존한다.
  추후 환경 독립성을 높이려면 fixture 중심 테스트를 늘리는 것이 좋다.
- [`include/CacheConfig.hpp`](/workspace/llvm-ir-caii/include/CacheConfig.hpp:1) 는 현재 `block_id()`와
  `set_index()`만 제공한다. 향후 ECB/HB/UCB 분석에 필요한 주소 구간 표현은 별도 타입 헤더로 분리하는 편이 설계상 더 깔끔하다.
- 테스트가 이미 준비되어 있으므로, 다음 작업은 "문서 정리"보다 "헤더 계약 구현 + 최소 테스트 추가" 쪽이 생산성이 높다.

## 관련 문서 경로

- [`docs/ARCHITECTURE.md`](/workspace/llvm-ir-caii/docs/ARCHITECTURE.md:1) — 아키텍처 설계서
- [`docs/TESTING.md`](/workspace/llvm-ir-caii/docs/TESTING.md:1) — 테스트 기술 문서
- [`/workspace/cache-interference-experiments.md`](/workspace/cache-interference-experiments.md:1) — 실험 설계 문서
- [`/workspace/HANDOFF.md`](/workspace/HANDOFF.md:1) — 캐시 간섭 실험 handoff
- [`/workspace/experiments/common.mk`](/workspace/experiments/common.mk:1) — 공통 빌드 시스템

## 참고 자료

- `/workspace/CAAS Cache Affinity Aware Scheduling Framework for RTEMS with Edge Computing Support.pdf`
- `/workspace/CAII 지표 정리 (수정본 — GR740 확정값 반영)_apa.pdf`
- `/workspace/LLVM IR 기반 캐시 간섭 지표 추출 패스 설계서 (GR740)_apa.pdf`
