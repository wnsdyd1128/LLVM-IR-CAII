#include "caii/AnalysisPass.hpp"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/PatternMatch.h>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace caii {

/// null 포인터 역참조 가능성을 IR 레벨에서 탐지한다.
/// - malloc/rtems_malloc 반환값 null 체크 누락
/// - 상수 null 포인터를 직접 load/store 하는 패턴
class NullDerefChecker : public AnalysisPass {
public:
    std::string name() const override { return "NullDerefChecker"; }
    std::string description() const override {
        return "Detects potential null pointer dereferences";
    }

    std::vector<Diagnostic> run(Module &M) override {
        std::vector<Diagnostic> diags;

        for (auto &F : M) {
            if (F.isDeclaration()) continue;
            checkFunction(F, diags);
        }
        return diags;
    }

private:
    void checkFunction(Function &F, std::vector<Diagnostic> &diags) {
        for (auto &BB : F) {
            for (auto &I : BB) {
                // load/store 에서 null 상수 포인터 직접 사용
                if (auto *LI = dyn_cast<LoadInst>(&I)) {
                    if (isNullConstant(LI->getPointerOperand()))
                        emit(diags, Diagnostic::Error, F.getName().str(),
                             "Load from null pointer", getLine(I));
                } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
                    if (isNullConstant(SI->getPointerOperand()))
                        emit(diags, Diagnostic::Error, F.getName().str(),
                             "Store to null pointer", getLine(I));
                }

                // malloc 계열 호출 후 반환값을 null 체크 없이 바로 역참조
                if (auto *CI = dyn_cast<CallInst>(&I)) {
                    if (isAllocCall(CI))
                        checkAllocResult(CI, diags, F);
                }
            }
        }
    }

    static bool isNullConstant(Value *V) {
        return isa<ConstantPointerNull>(V);
    }

    static bool isAllocCall(CallInst *CI) {
        auto *F = CI->getCalledFunction();
        if (!F) return false;
        StringRef name = F->getName();
        return name == "malloc" || name == "calloc" ||
               name == "rtems_malloc" || name == "__rtems_calloc";
    }

    /// alloc 반환값이 null 체크 없이 load/store 에 사용되는지 확인
    static void checkAllocResult(CallInst *AllocCI,
                                 std::vector<Diagnostic> &diags,
                                 Function &F) {
        // 사용자 중 null 비교(icmp) 없이 메모리 접근이 있으면 경고
        bool hasNullCheck = false;
        for (auto *U : AllocCI->users()) {
            if (auto *CMP = dyn_cast<ICmpInst>(U)) {
                if (CMP->isEquality() && isNullConstant(CMP->getOperand(1)))
                    hasNullCheck = true;
            }
        }
        if (hasNullCheck) return;

        for (auto *U : AllocCI->users()) {
            if (isa<LoadInst>(U) || isa<StoreInst>(U) ||
                isa<GetElementPtrInst>(U)) {
                emit(diags, Diagnostic::Warning, F.getName().str(),
                     "Allocation result used without null check",
                     getLine(*AllocCI));
                return;
            }
        }
    }

    static unsigned getLine(const Instruction &I) {
        if (const auto &DL = I.getDebugLoc())
            return DL.getLine();
        return 0;
    }

    static void emit(std::vector<Diagnostic> &diags, Diagnostic::Severity sev,
                     const std::string &func, const std::string &msg,
                     unsigned line) {
        diags.push_back({sev, "NullDerefChecker", func, msg, line});
    }
};

// 팩토리 함수 (main.cpp / PluginEntry.cpp 에서 사용)
std::unique_ptr<AnalysisPass> createNullDerefChecker() {
    return std::make_unique<NullDerefChecker>();
}

} // namespace caii