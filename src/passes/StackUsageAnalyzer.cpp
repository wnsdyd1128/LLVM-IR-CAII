#include "caii/AnalysisPass.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace caii {

/// 각 함수의 정적 스택 사용량을 계산한다.
/// RTEMS 태스크는 스택이 제한적이므로 alloca 합계로 경계값을 추정한다.
class StackUsageAnalyzer : public AnalysisPass {
public:
    // GR740 BSP 기본 태스크 스택 크기 (bytes) — bspopts.h 값 기준
    static constexpr uint64_t WARN_THRESHOLD  = 2048;
    static constexpr uint64_t ERROR_THRESHOLD = 4096;

    std::string name() const override { return "StackUsageAnalyzer"; }
    std::string description() const override {
        return "Estimates static stack usage per function";
    }

    std::vector<Diagnostic> run(Module &M) override {
        std::vector<Diagnostic> diags;
        const DataLayout &DL = M.getDataLayout();

        for (auto &F : M) {
            if (F.isDeclaration()) continue;
            uint64_t total = estimateStack(F, DL);
            if (total >= ERROR_THRESHOLD)
                emit(diags, Diagnostic::Error, F.getName().str(), total);
            else if (total >= WARN_THRESHOLD)
                emit(diags, Diagnostic::Warning, F.getName().str(), total);
        }
        return diags;
    }

private:
    static uint64_t estimateStack(Function &F, const DataLayout &DL) {
        uint64_t total = 0;
        for (auto &BB : F) {
            for (auto &I : BB) {
                if (auto *AI = dyn_cast<AllocaInst>(&I)) {
                    if (AI->isStaticAlloca()) {
                        uint64_t sz =
                            DL.getTypeAllocSize(AI->getAllocatedType());
                        if (auto *C =
                                dyn_cast<ConstantInt>(AI->getArraySize()))
                            sz *= C->getZExtValue();
                        total += sz;
                    }
                }
            }
        }
        return total;
    }

    static void emit(std::vector<Diagnostic> &diags, Diagnostic::Severity sev,
                     const std::string &func, uint64_t bytes) {
        std::string msg = "Estimated static stack: " +
                          std::to_string(bytes) + " bytes";
        diags.push_back({sev, "StackUsageAnalyzer", func, msg, 0});
    }
};

std::unique_ptr<AnalysisPass> createStackUsageAnalyzer() {
    return std::make_unique<StackUsageAnalyzer>();
}

} // namespace caii