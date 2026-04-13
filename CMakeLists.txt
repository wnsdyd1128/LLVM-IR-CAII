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
add_executable(caii-analyzer
    src/main.cpp
    src/passes/NullDerefChecker.cpp
    src/passes/StackUsageAnalyzer.cpp
    src/passes/RTEMSAPIChecker.cpp
)
target_link_libraries(caii-analyzer PRIVATE caii_common)

# ── opt 플러그인 (opt --load-pass-plugin=./caii_plugin.so) ─────────────
add_library(caii_plugin MODULE
    src/passes/NullDerefChecker.cpp
    src/passes/StackUsageAnalyzer.cpp
    src/passes/RTEMSAPIChecker.cpp
    src/PluginEntry.cpp
)
target_link_libraries(caii_plugin PRIVATE ${LLVM_LIBS})
set_target_properties(caii_plugin PROPERTIES PREFIX "" SUFFIX ".so")
