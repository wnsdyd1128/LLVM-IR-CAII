# CAII 정적 분석 파이프라인 아키텍처 설계

## 1. 목적 및 개요

CAAS 논문(SAC '26)이 제안한 CA_i (Cache Affinity Indicator)를 기반으로,
크로스-코어 캐시 간섭과 선점 지연(CRPD)을 추가로 고려하는 확장 지표
CAII_i^static 을 GR740(SPARC V8, LEON4FT) 환경에서 LLVM IR로부터 완전 정적으로 추출한다.

모든 입력 `(C_i, T_i, D_i, max_p UCB_i(p), ECB_j, N_LLC, BRT)` 은
컴파일 타임 또는 캘리브레이션 1회 측정으로 결정되며, 런타임 고정점 반복이 없다.

| 지표 | 역할 | 기존 CAAS |
|---|---|---|
| CA_i | 초기 배치 휴리스틱의 시작점 | 없음 — 순수 IR 분석 |
| n×n 간섭 행렬 | 태스크 쌍 간 캐시 간섭 잠재량 정량화 | 없음 — 순수 IR 분석 |
| CAII_i^static | 최종 배치 결정 기준 (CA_i 대체) | 있음 — 후보 배치별 재계산 |

CAAS 논문은 CA_i 단독으로 스케줄링 아키텍처를 선택했다.
본 프로젝트는 여기에 크로스-코어 간섭과 선점 지연을 추가 반영한
CAII_i^static으로 CA_i를 대체하는 것이 목적이다.

순환 의존 해소: CAII 계산에 코어 배치가 필요하고 코어 배치 결정에 CAII가 필요하지만,
n×n 간섭 행렬을 IR 분석 단계에서 코어 배치 없이 미리 계산해두면
후보 배치가 바뀔 때마다 행렬 합산만 다시 수행하면 되므로 순환이 끊긴다.

## 2. 핵심 지표 정의

### 2.1 원형 CA_i (CAAS 논문 §4.2)

```
CA_i =        Σ_{j=1}^{N(X_i)} r_i^j
         ─────────────────────────────────────────
         Σ_{j=1}^{N(X_i)} (δ_i^j + 1) × r_i^j
```

- `X_i`   : 태스크 τ_i의 정적 메모리 접근 시퀀스 (CFG 경로 열거)
- `δ_i^j` : 주소 x_j의 평균 재사용 거리
- `r_i^j` : 주소 x_j의 반복 접근 횟수
- 값 범위: CA_i ∈ [0, 1] (높을수록 시간 지역성 강함)

### 2.2 확장 CAII_i^static

```
                           CA_i
CAII_i^static = ────────────────────────────────────────────────
                1 + Ī_i^{c,static} + CRPD̄_i^{static}
```

크로스-코어 간섭 항:

```
I^c_{i,k}(C_i) = |HB_i ∩ CB_k| × ⌈C_i / T_k⌉ × BRT    ← |·| : 집합 기수(cardinality)

I_i^{c,static} = Σ_{k: core(k)≠core(i)} I^c_{i,k}(C_i)

Ī_i^{c,static} = I_i^{c,static} / (C_i × BRT × N_LLC)    ← [0,1] 정규화
```

선점 지연 항 (CRPD):

```
CRPD_{i,j}     = min(max_p UCB_i(p), ECB_j) × BRT         ← 1회 선점당 비용

CRPD_i^static  = Σ_{j ∈ hp(i)} ⌈D_i / T_j⌉ × CRPD_{i,j} ← 선점 횟수 상한: ⌈D_i/T_j⌉

CRPD̄_i^static = CRPD_i^static / C_i                       ← 무차원 정규화
```

> 참고 — 간섭 창 기준 비교:
> - I^c 항: 창 = C_i (1회 실행) → 선점 횟수 `⌈C_i/T_k⌉`
> - CRPD 항: 창 = D_i (데드라인) → 선점 횟수 `⌈D_i/T_j⌉`
> - D_i ≥ C_i 이므로 CRPD 선점 횟수가 더 크게 추정 → 더 보수적.
> - 런타임 응답 시간 R_i (R_i ≤ D_i) 를 사용하지 않기 위한 완전 정적 설계에서 의도된 차이이며,
>   D_i ≥ R_i ≥ C_i 이므로 safe upper bound 가 유지된다.

파티셔닝 ON 단순화:

```
HB_i ∩ CB_k = ∅  ⟹  Ī_i^{c,static} = 0
⟹  CAII = CA_i / (1 + CRPD̄_i^static)
```

## 3. GR740 캐시 하드웨어 상수

| 상수 | 값 | 근거 |
|---|---|---|
| `N_LLC` (NUM_WAYS) | 4 ways | GR740 UM v2.7 ("4-ways, BCH protection") |
| `b` (LINE_SIZE) | 32 bytes | L1/L2 공통 |
| `S` (NUM_SETS) | 16,384 sets | 2 MiB ÷ (4 × 32 B) |
| `L2_cap` (TOTAL_LINES) | 65,536 lines | N_LLC × S |
| `N_core` | 4 | LEON4FT 코어 수 |
| `BRT` | 측정값 (기본 85 cycles) | laysim-gr740 Phase 1 캘리브레이션 |

> 주의: 이전 문서의 N_LLC = 16 수치는 오류. GR740 UM v2.7 기준으로 N_LLC = 4 확정.

```
block_id(addr)  = addr >> 5         (= addr / 32)
set_index(addr) = block_id(addr) % S (= block_id % 16384)
```

구현: [`include/CacheConfig.hpp`](../include/CacheConfig.hpp)

## 4. 전체 분석 파이프라인

파이프라인은 2단계로 분리된다.
Stage 1은 코어 배치와 완전히 독립적으로 1회만 실행되며,
Stage 2는 배치 후보를 바꿔가며 반복 실행된다.

```
┌────────────────────────────────────────────────────────────────┐
│              Stage 1: IR 분석 (1회, 코어 배치 불필요)           │
│                                                                │
│   inlined IR ──► P1 ──► P2 ──► P3 ──► P4                     │
│                   │              │      │                      │
│                  CAI            P5 (n×n 행렬)                  │
│                   │              │                             │
│             CA_i ∈[0,1]  interference_matrix[i][k]            │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│      Stage 2: CAII 기반 배치 결정 (반복, 코어 배치 후보 입력)   │
│                                                                │
│  초기 배치: CA_i ≥ θ → 파티셔닝 / CA_i < θ → 글로벌 (휴리스틱) │
│  후보 배치 평가 (행렬 합산만 재수행, O(n^2)):                   │
│                                                                │
│    I_i^{c,static}  = Σ_{k:core(k)≠core(i)} matrix[i][k]      │
│                                                                │
│    CRPD_i^static   = Σ_{j∈hp(i)} ⌈D_i/T_j⌉                   │
│                        × min(UCB_max_i, ECB_j) × BRT          │
│                                                                │
│    CAII_i^static   = CA_i / (1 + Ī_i^{c,static} + CRPD̄_i)   │
│                                                                │
│  CAII 기준으로 배치 개선 → 수렴 시 최종 배치 확정              │
└────────────────────────────────────────────────────────────────┘
```

### 4.1 패스 파이프라인 표

| # | 패스 이름 | 종류 | 출력 | 의존 |
|---|---|---|---|---|
| P0 | `always-inline` (외부 `opt`) | IR 변환 | 평탄화 단일 CFG | — |
| P1 | `MemAccessCollectorPass` | FunctionPass | `BBAccessMap` (BB → CacheBlockSet) | P0 |
| P2 | `ECBExtractorPass` | FunctionPass | `ECB_i` = ∪BB접근집합 | P1 |
| P3 | `UCBDataflowPass` | FunctionPass | `UCB_i(p)` per-instruction, `max_p UCB_i(p)` | P1 |
| P4 | `HBExtractorPass` | FunctionPass | `HB_i` (Conservative 또는 TailAccess) | P2, P3 |
| P5 | `InterferenceComputePass` | ModulePass | `interference_matrix[n×n]` | P2, P4 |
| CAI | `CAIComputePass` | FunctionPass | `δ_i^j`, `r_i^j`, `CA_i` | P1 |

### 4.2 P0: 사전 인라이닝 (외부)

P1~P4는 `FunctionPass`로 단일 CFG만 처리한다.
태스크 내 callee 접근을 누락 없이 분석하기 위해
P0을 파이프라인 실행 전 외부에서 수행한다.

```bash
opt -passes="always-inline" task.bc -o task_inlined.bc
```

잔여 `CallInst` 유형 처리 (P1에서):

| 유형 | 조건 | 처리 |
|---|---|---|
| A | `callee->isDeclaration()` (RTEMS API, libc) | full-cache fallback + INFO 로그 |
| B | `CI->isIndirectCall()` (함수 포인터) | full-cache fallback + WARN 로그 |
| C | 직접 호출 + IR 있음 (`always-inline` 실패) | P1-ERROR 진단 발행 (개발자 수정 유도) |

> full-cache fallback: 해당 CallInst 지점에서 전체 캐시 블록(S × N_LLC = 65,536 lines)을
> ECB에 추가. 분석 결과가 실제보다 크게 나오는 over-approximation이며 sound(안전)하다.

### 4.3 P1: MemAccessCollectorPass

- `LoadInst` / `StoreInst` 순회 → 주소 범위 해석 → 캐시 블록 집합 매핑
- 주소 범위 해석 3단계 fallback:
  1. `GlobalVariable` → 링커 맵 물리 주소
  2. `AllocaInst` → 스택 프레임 오프셋 + 스택 베이스
  3. `GetElementPtrInst` + `ScalarEvolution` → SCEV 범위
  4. (실패) → full-cache fallback (over-approximation, sound)

### 4.4 P2: ECBExtractorPass

- 정의: `ECB_j = { b | ∃ load/store in τ_j accessing block b }`
- 모든 BB의 접근 블록 합산 (경로 독립적 over-approximation)
- CRPD 계산에서 `min(max_p UCB_i(p), ECB_j)` 의 `ECB_j`로 사용

### 4.5 P3: UCBDataflowPass

```
UCB_i(p) = InCache(p) ∩ NeedAfter(p)
```

| 분석 | 방향 | join 연산 | 의미 |
|---|---|---|---|
| `InCache` | Forward (entry → exit) | 교집합 (Must 분석) | p 시점에 반드시 캐시에 있는 블록 |
| `NeedAfter` | Backward (exit → entry) | 합집합 | p 이후 재사용될 블록 |

- 루프 처리: `LoopInfo` + `ScalarEvolution::getBackedgeTakenCount()` 으로 루프 경계 정적 결정
- 수렴 후 `max_p UCB_i(p)` → CRPD 계산 입력

### 4.6 P4: HBExtractorPass

| 전략 | 정의 | 방향 |
|---|---|---|
| `Conservative` (기본) | `HB_i = ECB_i` | 과대 (safe) |
| `TailAccess` | `HB_i = InCache(exit(F))` — P3 Forward 분석 결과 | 정밀 |

집합당 `N_LLC` way 초과 블록은 `filterByCapacity()`로 제거.

### 4.7 P5: InterferenceComputePass

코어 배치 없이 모든 태스크 쌍 (i, k)에 대해 n×n 간섭 행렬을 계산한다.
`CB_k = ECB_k`.

```
interference_matrix[i][k] = |HB_i ∩ CB_k| × ⌈C_i / T_k⌉ × BRT
```

충돌 블록 수 계산 — 단순 upper bound (기본, 보수적):

```
conflict_blocks = |HB_i ∩ CB_k|
```

충돌 블록 수 계산 — 집합별 LRU 조건 (정밀):

```
evictable_s     = min(|inter_s|, max(0, |HB_i_s| + |CB_k_s| − N_LLC))
conflict_blocks = Σ_s evictable_s
```

Stage 2에서의 사용: 코어 배치 후보가 주어지면 해당 행에서 크로스-코어 열만 합산한다.

```
I_i^{c,static} = Σ_{k: core(k) ≠ core(i)} interference_matrix[i][k]
```

Multi-way 간섭: b ∈ HB_i ∩ CB_j ∩ CB_k 일 때 b가 두 번 카운트되나,
j 퇴출 후 재적재 → k 퇴출 후 재적재 시나리오에 대응하는 over-approximation이다.
Safe upper bound가 유지된다.

### 4.8 CAI: CAIComputePass

CAAS 논문 식 (1), (2) 구현:

1. CFG 경로 열거 + 루프 언롤 → 정적 접근 시퀀스 `X_i`
2. 각 주소 `x_j`별 연속 참조 사이의 구별 접근 수 평균 → `δ_i^j`
3. 반복 횟수 → `r_i^j`

## 5. 타입 정의

### 5.1 하드웨어 및 공통 타입

```cpp
// CacheConfig.hpp — GR740 하드웨어 상수
namespace GR740Cache {
  constexpr uint32_t LINE_SIZE    = 32;
  constexpr uint32_t NUM_WAYS     = 4;
  constexpr uint32_t NUM_SETS     = 16384;
  constexpr uint32_t TOTAL_LINES  = 65536;

  using CacheBlockSet = std::unordered_set; // block_id 집합
}

// caii/CacheTypes.hpp
struct AddressRange { uint64_t low, high; bool valid; };
using BBAccessMap = std::map;

struct TaskMeta {
  std::string name;
  uint64_t Ci, Ti, Di;          // WCET, 주기, 데드라인 [cycles]
  uint32_t core;                 // 배치 코어 (0~3)
  GR740Cache::CacheBlockSet HB, ECB; // P4, P2 결과
};
```

### 5.2 분석 결과 타입

```cpp
// caii/CAIResult.hpp
struct MemAccessInfo { uint64_t addr; double delta; uint64_t repeat; };

struct CAIResult {
  std::string task_name;
  std::vector accesses; // Δ_i
  double   ca;     // CA_i ∈ [0,1]
  uint64_t n_xi;   // N(X_i)
};

// caii/CAIIResult.hpp
struct PairwiseInterference {
  std::string interferer_task;
  uint32_t    interferer_core;
  size_t      conflict_blocks;
  uint64_t    preempt_count;    // ⌈C_i / T_k⌉
  uint64_t    Ic_ik_cycles;
};

struct CRPDContribution {
  std::string preemptor_task;
  size_t      ecb_j, ucb_max_i, min_ucb_ecb;
  uint64_t    preempt_count;    // ⌈D_i / T_j⌉
  uint64_t    crpd_cycles;
};

struct CAIIResult {
  CAIResult cai;                // CA_i 원형
  size_t    ecb_count, hb_count, ucb_max;
  std::vector interference_pairs;
  uint64_t  Ic_static_total_cycles;
  double    Ic_static_normalized;   // Ī_i^{c,static} = total / (C_i × BRT × N_LLC)
  std::vector  crpd_contributions;
  uint64_t  crpd_static_total_cycles;
  double    crpd_static_normalized; // CRPD̄_i^static = total / C_i
  double    caii_static;            // CAII_i^static ∈ [0,1]
};
```

## 6. 파이프라인 오케스트레이터

```cpp
// caii/CacheAnalysisPipeline.hpp
struct PipelineConfig {
  std::string linker_map_path;   // gr740.map
  std::string task_meta_path;    // task_config.yaml
  uint64_t    brt_cycles;        // BRT (기본 85)
  HBStrategy  hb_strategy;       // Conservative | TailAccess
  std::string output_json_path;  // 결과 JSON
};

class CacheAnalysisPipeline {
public:
  explicit CacheAnalysisPipeline(PipelineConfig config);
  std::vector run(llvm::Module &M);
};
```

실행 예시:

```bash
# 1. P0 — 외부 인라이닝
opt -passes="always-inline" task.bc -o task_inlined.bc

# 2. P1~P5 + CAI — 파이프라인 실행
build/caii-analyzer task_inlined.bc \
  --pass CacheAnalysis \
  --task-meta task_config.yaml \
  --linker-map gr740.map \
  --output task_cache_profile.json
```

## 7. 출력 형식 (JSON)

```json
{
  "task": "ctrl_task",
  "Ci_cycles": 120000,
  "Ti_cycles": 1000000,
  "Di_cycles": 1000000,
  "ECB": { "count": 384 },
  "HB":  { "count": 320, "strategy": "tail-access" },
  "interference": {
    "Ic_static_total_cycles": 1152000,
    "Ic_static_normalized":   0.12,
    "pairs": [
      { "from": "sensor_task", "core": 1,
        "conflict_blocks": 48, "preempt_count": 5, "Ic_ik_cycles": 576000 }
    ]
  },
  "CRPD_static": {
    "total_cycles": 93600,
    "normalized":   0.078,
    "contributions": [
      { "preemptor": "ctrl_hi", "min_ucb_ecb": 156,
        "preempt_count": 3, "crpd_cycles": 46800 }
    ]
  },
  "CAII_static": 0.847
}
```

## 8. 정확도 한계 및 근사 방향

| 항목 | 추정 방향 | 원인 | 비고 |
|---|---|---|---|
| ECB | over-approximation | 미실행 BB도 합산 | CRPD 과대 추정 → 안전 |
| InCache (UCB) | under-approximation | Must 분석 join=교집합 | 공통 블록만 추적 |
| HB (Conservative) | over-approximation | ECB = HB 가정 (warm cache 과대) | — |
| 충돌 블록 수 | over-approximation | LRU 조건 완화 적용 | — |
| 선점 횟수 상한 (I^c) | over-approximation | `⌈C_i/T_k⌉` 사용 (창 = C_i) | 1회 실행 구간 기준 |
| 선점 횟수 상한 (CRPD) | over-approximation | `⌈D_i/T_j⌉` 사용 (창 = D_i) | D_i ≥ R_i ≥ C_i → safe; I^c 창(C_i)과 의도적으로 다름 |
| 외부 함수 호출 (유형 A) | over-approximation | `callee->isDeclaration()` → full-cache fallback | ECB 과대 추정 → 안전 |
| 간접 호출 (유형 B) | over-approximation | 함수 포인터 정적 해결 불가 → full-cache fallback | ECB 과대 추정 → 안전 |

모든 지표가 실제 값의 안전한 상한(safe upper bound) →
CAII 분모 증가 → CAII_static 이 보수적으로 낮게 나옴 → 하드 실시간 안전 마진 확보.

## 9. 구현 현황 (헤더 기준)

```
include/
├── CacheConfig.hpp           (구현 완료 — GR740 HW 상수)
├── IRLoader.hpp              (구현 완료 — .bc/.ll 로드)
├── CacheTypes.hpp            (예정 — BBAccessMap, AddressRange, TaskMeta)
├── CAIResult.hpp             (예정 — δ_i^j, r_i^j, CA_i)
├── CAIIResult.hpp            (예정 — CAII_static + 중간 결과 전체)
├── CacheAnalysisPipeline.hpp (예정 — 파이프라인 오케스트레이터)
├── MemAccessCollectorPass.hpp(예정 — P1)
├── ECBExtractorPass.hpp      (예정 — P2)
├── UCBDataflowPass.hpp       (예정 — P3)
├── HBExtractorPass.hpp       (예정 — P4)
├── InterferenceComputePass.hpp(예정 — P5)
└── CAIComputePass.hpp        (예정 — CA_i)

src/
├── IRLoader.cpp              (구현 완료)
├── main.cpp                  (구현 완료 — CLI 스켈레톤)
├── MemAccessCollectorPass.cpp(예정)
├── ECBExtractorPass.cpp      (예정)
├── UCBDataflowPass.cpp       (예정)
├── HBExtractorPass.cpp       (예정)
├── InterferenceComputePass.cpp(예정)
├── CAIComputePass.cpp        (예정)
└── CacheAnalysisPipeline.cpp (예정)
```

## 10. 구현 로드맵

### Phase 1 — 헤더 정의 (인터페이스 확정)
- [ ] `CacheTypes.hpp`
- [ ] `CAIResult.hpp`, `CAIIResult.hpp`
- [ ] `passes/MemAccessCollectorPass.hpp` ~ `CAIComputePass.hpp`
- [ ] `CacheAnalysisPipeline.hpp`

### Phase 2 — P1, P2 구현 (기반 데이터 수집)
- [ ] `MemAccessCollectorPass.cpp` — 주소 해석 3단계 fallback
- [ ] `ECBExtractorPass.cpp` — BBAccessMap 합산

### Phase 3 — P3, P4 구현 (UCB/HB 분석)
- [ ] `UCBDataflowPass.cpp` — forward/backward worklist
- [ ] `HBExtractorPass.cpp` — Conservative + TailAccess 전략

### Phase 4 — P5 + CAI 구현 (지표 계산)
- [ ] `InterferenceComputePass.cpp` — 집합별 LRU 충돌 분석
- [ ] `CAIComputePass.cpp` — CFG 경로 열거 + 재사용 거리

### Phase 5 — 파이프라인 통합
- [ ] `CacheAnalysisPipeline.cpp` — 오케스트레이터
- [ ] `caii-analyzer` CLI 옵션 연동

## 11. 참고 자료

- CAAS 논문: `CAAS Cache Affinity Aware Scheduling Framework for RTEMS with Edge Computing Support.pdf`
- 기존 CAAS 논문의 CAIC 지표: `/workspace/CAI 지표 정리.pdf`
- CAII 지표 정리: `/workspace/CAII 지표 정리 (수정본 — GR740 확정값 반영)_apa.pdf`
- LLVM IR 기반 패스 설계서: `/workspace/LLVM IR 기반 캐시 간섭 지표 추출 패스 설계서 (GR740)_apa.pdf`
- GR740 User Manual v2.7 (2024)
