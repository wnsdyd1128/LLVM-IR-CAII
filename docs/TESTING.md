# 테스트 가이드

## 개요

이 프로젝트의 단위 테스트는 **GoogleTest**를 사용한다.  
외부 패키지 설치 없이 LLVM 번들 소스(`/workspace/llvm-project/third-party/unittest/googletest`)를 직접 빌드한다.

테스트 실행기는 **CTest**로 통합되어 있다.

---

## 빠른 시작

```bash
# 빌드 (최초 또는 파일 추가 후)
cmake --build /workspace/llvm-ir-caii/build -- -j$(nproc)

# 전체 테스트 실행
ctest --test-dir /workspace/llvm-ir-caii/build --output-on-failure

# 특정 테스트만 실행
ctest --test-dir /workspace/llvm-ir-caii/build -R CacheConfigTest --output-on-failure

# gtest 상세 출력 (직접 실행)
/workspace/llvm-ir-caii/build/tests/CacheConfigTest --gtest_color=yes
/workspace/llvm-ir-caii/build/tests/IRLoaderTest    --gtest_color=yes

# 특정 케이스만 실행
/workspace/llvm-ir-caii/build/tests/CacheConfigTest --gtest_filter="SetIndex.*"
```

---

## 디렉토리 구조

```
tests/
├── CMakeLists.txt              # 테스트 타깃 등록 + caii_add_test() 헬퍼
├── fixtures/
│   ├── minimal.ll              # 유효한 최소 LLVM IR (함수 3개, RTEMS 의존 없음)
│   └── invalid.ll              # 파서 오류를 유발하는 깨진 IR (오류 처리 테스트용)
└── unit/
    ├── CacheConfigTest.cpp     # GR740 캐시 상수, block_id(), set_index() 검증
    └── IRLoaderTest.cpp        # IRLoader 성공/실패 케이스
```

---

## CMake 구조

### gtest 빌드 (`CMakeLists.txt` 루트)

```cmake
set(GTEST_DIR /workspace/llvm-project/third-party/unittest/googletest)

add_library(gtest STATIC ${GTEST_DIR}/src/gtest-all.cc)
target_include_directories(gtest PUBLIC ${GTEST_DIR}/include PRIVATE ${GTEST_DIR})
target_compile_options(gtest PRIVATE -w)
target_link_libraries(gtest PUBLIC pthread)

add_library(gtest_main STATIC ${GTEST_DIR}/src/gtest_main.cc)
target_link_libraries(gtest_main PUBLIC gtest)
```

- `gtest-all.cc`는 gtest 모든 소스를 단일 번역 단위로 포함한다.
- `-w`로 third-party 경고를 억제한다.

### 테스트 타깃 헬퍼 (`tests/CMakeLists.txt`)

```cmake
function(caii_add_test target)
    target_link_libraries(${target} PRIVATE gtest_main caii_common)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${target} PRIVATE
        CAII_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME ${target} COMMAND ${target})
endfunction()
```

`CAII_SOURCE_DIR` 매크로로 fixture 파일 절대 경로를 테스트 코드 안에서 사용할 수 있다.

```cpp
// 사용 예
auto M = caii::loadIR(Ctx, CAII_SOURCE_DIR "/tests/fixtures/minimal.ll");
```

---

## 새 테스트 추가하는 방법

### 1. 파일 생성

```
tests/unit/MyPassTest.cpp
```

### 2. `tests/CMakeLists.txt`에 등록

```cmake
add_executable(MyPassTest unit/MyPassTest.cpp)
caii_add_test(MyPassTest)
```

### 3. 기본 테스트 파일 구조

```cpp
#include "caii/passes/MyPass.hpp"
#include <gtest/gtest.h>

TEST(MyPass, BasicCase) {
    // ...
    EXPECT_EQ(actual, expected);
}
```

---

## 패스 테스트 패턴 (P1~P5용)

패스가 구현되면 `llvm::parseAssemblyString()`으로 IR을 코드 안에서 생성해 테스트한다.  
RTEMS 툴체인 없이 호스트 빌드만으로 동작한다.

```cpp
#include "caii/passes/MemAccessCollectorPass.hpp"
#include <gtest/gtest.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>

// IR 문자열 → Module 헬퍼
static std::unique_ptr<llvm::Module> parseIR(llvm::LLVMContext &Ctx,
                                              llvm::StringRef Src) {
    llvm::SMDiagnostic Err;
    auto M = llvm::parseAssemblyString(Src, Err, Ctx);
    if (!M) Err.print("test", llvm::errs());
    return M;
}

TEST(MemAccessCollector, SingleLoad) {
    llvm::LLVMContext Ctx;
    auto M = parseIR(Ctx, R"(
        define i32 @f(i32* %p) {
          %v = load i32, i32* %p
          ret i32 %v
        }
    )");
    ASSERT_NE(M, nullptr);

    // 패스 실행 후 BBAccessMap 검증
    // (패스 구현 후 채울 것)
}
```

### asmparser 컴포넌트 추가

`parseAssemblyString`을 사용하는 테스트는 `CMakeLists.txt`의 `LLVM_LIBS`에 `asmparser`를 추가해야 한다.

```cmake
llvm_map_components_to_libnames(LLVM_LIBS
    core support irreader bitreader
    analysis target
    scalaropts instcombine transformutils
    asmparser   # ← parseAssemblyString 사용 시 추가
)
```

---

## Fixture 파일 규칙

| 파일 | 용도 |
|---|---|
| `minimal.ll` | 유효한 최소 IR. 함수 구조, 이름 등 정상 경로 테스트에 사용. |
| `invalid.ll` | 파서가 거부하는 깨진 텍스트. 오류 처리 경로 확인에 사용. |
| `(예정) loop_simple.ll` | 단순 루프 1개. UCBDataflowPass 루프 처리 검증용. |
| `(예정) multi_bb.ll` | BB 3개 이상의 CFG. ECBExtractorPass 경로 합산 검증용. |

fixture는 RTEMS 의존 없이 호스트 LLVM만으로 파싱 가능해야 한다.

---

## 현재 테스트 목록

### CacheConfigTest (8개)

| 테스트 | 검증 내용 |
|---|---|
| `GR740Constants.Values` | `NUM_WAYS=4`, `NUM_SETS=16384`, `LINE_SIZE=32`, `TOTAL_LINES=65536` |
| `BlockId.AlignedAddresses` | `block_id(0)=0`, `block_id(32)=1`, `block_id(16384)=512` |
| `BlockId.UnalignedAddresses` | 같은 캐시 라인 내 주소는 동일 block_id |
| `SetIndex.BasicMapping` | `set_index(0)=0`, `set_index(32)=1`, `set_index(16384)=512` |
| `SetIndex.WrapAround` | `set_index(0) == set_index(NUM_SETS * LINE_SIZE)` |
| `SetIndex.TwoCacheWraps` | 2배 wrap-around도 동일 set_index |
| `CacheConfig.LastByteOfLine` | 주소 0과 31은 같은 라인 |
| `CacheConfig.FirstByteNextLine` | 주소 32는 다른 라인 |

### IRLoaderTest (5개)

| 테스트 | 검증 내용 |
|---|---|
| `IRLoader.LoadsMinimalLL` | `minimal.ll` 로드 성공, 함수 3개 |
| `IRLoader.FunctionNamesPresent` | `add`, `store_test`, `load_test` 함수명 확인 |
| `IRLoader.LoadsHelloRtemsBc` | `examples/hello_rtems.bc` 로드 성공 |
| `IRLoader.ReturnsNullForMissingFile` | 존재하지 않는 경로 → `nullptr` |
| `IRLoader.ReturnsNullForInvalidIR` | `invalid.ll` → `nullptr` |

> `IRLoader.LoadsHelloRtemsBc`는 `compile-to-ir.sh` 실행 후 `.bc`가 있어야 통과한다.  
> CI 환경에서는 이 테스트를 `DISABLED_` 접두사로 조건부 비활성화하거나 fixture로 대체하는 것을 권장한다.

---

## 알려진 제약

- **`IRLoader.LoadsHelloRtemsBc`**: RTEMS 툴체인이 없는 환경에서는 `.bc` 파일이 없어 실패한다. 해결 방법은 fixture 전용 `.bc`를 커밋하거나, 테스트를 `DISABLED_`로 마크하는 것이다.
- **asmparser 미연결**: 현재 `LLVM_LIBS`에 `asmparser`가 없다. `parseAssemblyString`을 사용하는 패스 테스트를 추가할 때 `CMakeLists.txt`에 추가해야 한다.