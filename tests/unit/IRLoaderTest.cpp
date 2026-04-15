#include "caii/IRLoader.hpp"
#include <gtest/gtest.h>
#include <llvm/IR/LLVMContext.h>

using namespace llvm;

// ── 유효한 .ll 파일 로드 ─────────────────────────────────────────────────

TEST(IRLoader, LoadsMinimalLL) {
    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, CAII_SOURCE_DIR "/tests/fixtures/minimal.ll");
    ASSERT_NE(M, nullptr);
    EXPECT_EQ(M->getFunctionList().size(), 3u);
}

TEST(IRLoader, FunctionNamesPresent) {
    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, CAII_SOURCE_DIR "/tests/fixtures/minimal.ll");
    ASSERT_NE(M, nullptr);
    EXPECT_NE(M->getFunction("add"),        nullptr);
    EXPECT_NE(M->getFunction("store_test"), nullptr);
    EXPECT_NE(M->getFunction("load_test"),  nullptr);
}

// ── 예제 RTEMS bitcode 로드 ───────────────────────────────────────────────

TEST(IRLoader, LoadsHelloRtemsBc) {
    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, CAII_SOURCE_DIR "/examples/hello_rtems.bc");
    ASSERT_NE(M, nullptr) << "hello_rtems.bc 로드 실패 — compile-to-ir.sh 를 먼저 실행하세요";
    EXPECT_GT(M->getFunctionList().size(), 0u);
}

// ── 오류 처리 ────────────────────────────────────────────────────────────

TEST(IRLoader, ReturnsNullForMissingFile) {
    LLVMContext Ctx;
    // stderr 출력은 정상 — loadIR 실패 시 nullptr 반환 확인
    auto M = caii::loadIR(Ctx, "/nonexistent/path/file.bc");
    EXPECT_EQ(M, nullptr);
}

TEST(IRLoader, ReturnsNullForInvalidIR) {
    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, CAII_SOURCE_DIR "/tests/fixtures/invalid.ll");
    EXPECT_EQ(M, nullptr);
}