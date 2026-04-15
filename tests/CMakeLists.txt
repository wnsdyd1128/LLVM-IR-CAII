set(CAII_SOURCE_DIR ${CMAKE_SOURCE_DIR})

function(caii_add_test target)
    target_link_libraries(${target} PRIVATE gtest_main caii_common)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(${target} PRIVATE
        CAII_SOURCE_DIR="${CAII_SOURCE_DIR}")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(CacheConfigTest unit/CacheConfigTest.cpp)
caii_add_test(CacheConfigTest)

add_executable(IRLoaderTest unit/IRLoaderTest.cpp)
caii_add_test(IRLoaderTest)
