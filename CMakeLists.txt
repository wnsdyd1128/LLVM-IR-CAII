cmake_minimum_required(VERSION 3.20)
project(llvm-ir-caii LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# LLVM_DIR 는 env.sh 에서 export 한 값을 cmake -DLLVM_DIR=$LLVM_DIR 로 전달
find_package(LLVM REQUIRED CONFIG)

message(STATUS "LLVM ${LLVM_PACKAGE_VERSION}  targets: ${LLVM_TARGETS_TO_BUILD}")

include_directories(${LLVM_INCLUDE_DIRS} include)
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
    core support irreader bitreader
    analysis target
    scalaropts instcombine transformutils
)

# ── 공통 라이브러리 ────────────────────────────────────────────────────
add_library(caii_common STATIC src/IRLoader.cpp)
target_link_libraries(caii_common PUBLIC ${LLVM_LIBS})

# ── 스탠드얼론 분석기 ──────────────────────────────────────────────────
add_executable(caii-analyzer src/main.cpp)
target_link_libraries(caii-analyzer PRIVATE caii_common)

# ── Google Test (LLVM 번들 소스) ───────────────────────────────────────
set(GTEST_DIR /workspace/llvm-project/third-party/unittest/googletest)

add_library(gtest STATIC ${GTEST_DIR}/src/gtest-all.cc)
target_include_directories(gtest PUBLIC ${GTEST_DIR}/include PRIVATE ${GTEST_DIR})
target_compile_options(gtest PRIVATE -w)
target_link_libraries(gtest PUBLIC pthread)

add_library(gtest_main STATIC ${GTEST_DIR}/src/gtest_main.cc)
target_link_libraries(gtest_main PUBLIC gtest)

enable_testing()
add_subdirectory(tests)
