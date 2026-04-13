# llvm-ir-caii

RTEMS 6.0 + GR740(SPARC V8, LEON4) 환경을 대상으로 하는 **LLVM IR API 기반 정적 분석 도구**.

C 소스를 SPARC RTEMS6 타겟 LLVM IR로 변환하고, IR 레벨 Analysis Pass로 두 가지 목적을 달성한다:

1. **결함 탐지** — RTEMS 특화 버그 패턴 (null 역참조, 스택 과다 사용, ISR 블로킹 API)
2. **CAII 지표 추출** — 태스크의 캐시 친화도(CA_i) + 크로스-코어 간섭 + 선점 지연을 정적으로 계산하여 CAAS 스케줄링 아키텍처 선택에 활용

---

## 환경 요구사항

| 항목 | 경로 |
|---|---|
| LLVM 빌드 | `/workspace/llvm-project/build` (23.0.0git, Sparc 타겟 포함) |
| RTEMS 6 툴체인 | `/opt/rtems/6` (`sparc-rtems6-gcc` 13.3.0) |
| GR740 BSP | `/opt/rtems/6/sparc-rtems6/gr740` |

---

## 빠른 시작

```bash
# 1. 환경변수 설정
source scripts/env.sh

# 2. 빌드
scripts/build.sh

# 3. RTEMS C 소스 → LLVM IR 변환
scripts/compile-to-ir.sh examples/hello_rtems.c
# → examples/hello_rtems.bc  (bitcode)
# → examples/hello_rtems.ll  (human-readable IR)

# 4. 정적 분석 실행
build/caii-analyzer examples/hello_rtems.bc
```

---

## 프로젝트 구조

```
llvm-ir-caii/
├── CMakeLists.txt
├── docs/
│   └── ARCHITECTURE.md     # CAII 파이프라인 아키텍처 설계서
├── include/
│   ├── CacheConfig.hpp     # GR740 L2 캐시 HW 상수 (N_LLC=4, S=16384, b=32B)
│   └── caii/
│       ├── IRLoader.hpp            # .bc / .ll 파일 로드
│       ├── AnalysisPass.hpp        # 결함 탐지 Pass 기반 인터페이스 + Diagnostic
│       ├── CacheTypes.hpp          # (예정) BBAccessMap, AddressRange, TaskMeta
│       ├── CAIResult.hpp           # (예정) δ_i^j, r_i^j, CA_i 결과 타입
│       ├── CAIIResult.hpp          # (예정) CAII_static + 중간 결과 전체
│       ├── CacheAnalysisPipeline.hpp # (예정) 파이프라인 오케스트레이터
│       └── passes/
│           ├── MemAccessCollectorPass.hpp  # (예정) P1
│           ├── ECBExtractorPass.hpp        # (예정) P2
│           ├── UCBDataflowPass.hpp         # (예정) P3
│           ├── HBExtractorPass.hpp         # (예정) P4
│           ├── InterferenceComputePass.hpp # (예정) P5
│           └── CAIComputePass.hpp          # (예정) CA_i
├── src/
│   ├── main.cpp            # 스탠드얼론 분석기 CLI
│   ├── IRLoader.cpp
│   ├── PluginEntry.cpp     # opt 플러그인 진입점
│   └── passes/
│       ├── NullDerefChecker.cpp
│       ├── StackUsageAnalyzer.cpp
│       └── RTEMSAPIChecker.cpp
├── scripts/
│   ├── env.sh              # 환경변수 (source 필요)
│   ├── build.sh            # cmake 빌드 래퍼
│   ├── compile-to-ir.sh    # C → SPARC IR 변환
│   └── analyze.sh          # 분석기 실행 래퍼
└── examples/
    └── hello_rtems.c       # 결함 예제 (3종)
```

---

## CAII 지표 파이프라인 (설계 중)

CAAS 논문의 CA_i에 크로스-코어 캐시 간섭과 선점 지연(CRPD)을 추가한 복합 지표.

```
CAII_i^static = CA_i / (1 + Ī_i^{c,static} + CRPD̄_i^static)
```

| 패스 | 출력 | 설명 |
|---|---|---|
| P1 `MemAccessCollectorPass` | BBAccessMap | BB별 접근 캐시 블록 집합 |
| P2 `ECBExtractorPass` | ECB_i | 태스크 Evicting Cache Block 전체 합집합 |
| P3 `UCBDataflowPass` | UCB_i(p), max\|UCB\| | Forward Must + Backward GEN/KILL 데이터플로우 |
| P4 `HBExtractorPass` | HB_i | 다음 주기 시작 시 히트 기대 블록 |
| P5 `InterferenceComputePass` | I_i^{c,static} | 크로스-코어 간섭 사이클 합산 |
| CAI `CAIComputePass` | CA_i | 재사용 거리 기반 캐시 친화도 [0, 1] |

GR740 캐시 파라미터: N_LLC=4, S=16,384 sets, b=32B, L2=2MiB (→ [`include/CacheConfig.hpp`](include/CacheConfig.hpp))

자세한 설계: [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

---

## 결함 탐지 Pass

### NullDerefChecker

`malloc` / `rtems_malloc` 계열 반환값을 null 체크 없이 바로 역참조하는 패턴과, 상수 null 포인터를 직접 load/store 하는 패턴을 탐지한다.

```c
char *buf = malloc(256);
memset(buf, 0, 256);   // warning: Allocation result used without null check
```

### StackUsageAnalyzer

함수별 정적 `alloca` 합산으로 스택 사용량을 추정한다. GR740 BSP 기본 태스크 스택 기준:

| 임계값 | 진단 등급 |
|---|---|
| ≥ 2 KB | warning |
| ≥ 4 KB | error |

```c
void process_telemetry(void) {
    char local_buf[8192];   // error: Estimated static stack: 8192 bytes
}
```

### RTEMSAPIChecker

세 가지 RTEMS API 규칙 위반을 탐지한다:

| 검사 항목 | 진단 등급 |
|---|---|
| ISR 컨텍스트에서 블로킹 API 호출 (`rtems_semaphore_obtain` 등) | error |
| `rtems_task_delete(RTEMS_SELF)` 이후 도달 가능한 코드 | warning |
| `rtems_status_code` 반환 API 결과 무시 | warning |

---

## CLI 옵션

```
caii-analyzer [options] <input.bc|input.ll>

옵션:
  --pass <name>   특정 Pass만 실행 (반복 가능). 생략 시 전체 실행.
  --no-color      컬러 출력 비활성화

예시:
  build/caii-analyzer app.bc
  build/caii-analyzer app.bc --pass NullDerefChecker --pass RTEMSAPIChecker
```

## opt 플러그인으로 실행

```bash
opt --load-pass-plugin=build/caii_plugin.so \
    --passes="caii-all" \
    examples/hello_rtems.bc -o /dev/null
```

---

## IR 생성 방법 (크로스 컴파일 상세)

RTEMS 헤더와 Clang 23의 `_Atomic` 구현 충돌 문제로, GCC의 `include` 디렉토리를 탐색 경로에서 제외해야 한다. `compile-to-ir.sh`에 이 로직이 내장되어 있다.

```bash
# compile-to-ir.sh 내부 핵심 플래그
clang --target=sparc-unknown-rtems6 \
    -mcpu=leon3 \
    -isystem <clang_resource>/include   # Clang 자체 stdatomic.h
    -isystem <gcc>/include-fixed        # limits.h 보정
    -isystem /opt/rtems/6/sparc-rtems6/include \
    -isystem /opt/rtems/6/sparc-rtems6/gr740/lib/include \
    -D__leon__ -D__sparc_v8__ \
    -g -O1 -emit-llvm -c -o out.bc src.c
```

> GCC `include/` 디렉토리를 포함하면 GCC의 `stdatomic.h`가 선택되어
> `_Atomic` 타입 불일치 오류가 발생한다.

---

## 새 Pass 추가하기

1. `include/caii/AnalysisPass.hpp`의 `AnalysisPass`를 상속받아 구현
2. `src/passes/MyPass.cpp` 작성 — `run(Module &M)` 에서 `Diagnostic` 목록 반환
3. `src/main.cpp`와 `src/PluginEntry.cpp`에 팩토리 함수 등록
4. `CMakeLists.txt`의 `caii-analyzer` / `caii_plugin` 소스 목록에 추가

```cpp
// src/passes/MyPass.cpp
namespace caii {
class MyPass : public AnalysisPass {
public:
    std::string name() const override { return "MyPass"; }
    std::string description() const override { return "..."; }
    std::vector<Diagnostic> run(llvm::Module &M) override { ... }
};
std::unique_ptr<AnalysisPass> createMyPass() {
    return std::make_unique<MyPass>();
}
} // namespace caii
```

---

## VSCode 설정

`/workspace/llvm-ir-caii/.clangd`가 clangd에 RTEMS 크로스 컴파일 플래그를 제공한다.
`*.c` 파일에 `rtems.h not found` 오류가 표시되면 `Ctrl+Shift+P` → **clangd: Restart language server**.