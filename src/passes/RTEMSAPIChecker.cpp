#include "caii/AnalysisPass.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <unordered_map>
#include <unordered_set>

using namespace llvm;

namespace caii {

/// RTEMS API 호출 규칙 위반을 탐지한다.
///
/// 현재 검사 항목:
///   1. ISR 컨텍스트에서 블로킹 API 호출 가능성
///      — 함수명이 _ISR_/_ISR 로 시작/포함 되는 경우 blocking API 호출 경고
///   2. rtems_task_delete(RTEMS_SELF) 이후 코드 도달 가능성
///   3. 반환값 미사용 rtems_status_code API
class RTEMSAPIChecker : public AnalysisPass {
public:
    std::string name() const override { return "RTEMSAPIChecker"; }
    std::string description() const override {
        return "Checks RTEMS API usage rules (ISR context, return codes, etc.)";
    }

    std::vector<Diagnostic> run(Module &M) override {
        std::vector<Diagnostic> diags;
        for (auto &F : M) {
            if (F.isDeclaration()) continue;
            checkISRBlocking(F, diags);
            checkDeleteSelf(F, diags);
            checkUnusedStatus(F, diags);
        }
        return diags;
    }

private:
    // 블로킹 가능성이 있는 RTEMS API 목록
    static const std::unordered_set<std::string> &blockingAPIs() {
        static const std::unordered_set<std::string> s = {
            "rtems_semaphore_obtain",
            "rtems_message_queue_receive",
            "rtems_event_receive",
            "rtems_task_wake_after",
            "rtems_task_wake_when",
            "rtems_barrier_wait",
            "rtems_region_get_segment",
        };
        return s;
    }

    // 반환값(rtems_status_code)을 반드시 확인해야 하는 API
    static const std::unordered_set<std::string> &statusAPIs() {
        static const std::unordered_set<std::string> s = {
            "rtems_task_create",   "rtems_task_start",
            "rtems_semaphore_create", "rtems_semaphore_obtain",
            "rtems_message_queue_create", "rtems_message_queue_send",
        };
        return s;
    }

    static bool looksLikeISR(const Function &F) {
        StringRef n = F.getName();
        return n.contains("_ISR_") || n.contains("_isr_") ||
               n.starts_with("ISR_") || n.ends_with("_isr");
    }

    void checkISRBlocking(Function &F, std::vector<Diagnostic> &diags) {
        if (!looksLikeISR(F)) return;
        for (auto &BB : F)
            for (auto &I : BB)
                if (auto *CI = dyn_cast<CallInst>(&I))
                    if (auto *Callee = CI->getCalledFunction())
                        if (blockingAPIs().count(Callee->getName().str()))
                            diags.push_back({Diagnostic::Error, name(),
                                F.getName().str(),
                                "Blocking API '" +
                                    Callee->getName().str() +
                                    "' called from ISR context",
                                getLine(I)});
    }

    void checkDeleteSelf(Function &F, std::vector<Diagnostic> &diags) {
        for (auto &BB : F) {
            bool deletedSelf = false;
            for (auto &I : BB) {
                if (deletedSelf) {
                    // rtems_task_delete(RTEMS_SELF) 이후 명령어 존재
                    if (!isa<UnreachableInst>(&I)) {
                        diags.push_back({Diagnostic::Warning, name(),
                            F.getName().str(),
                            "Code reachable after rtems_task_delete(SELF)",
                            getLine(I)});
                        break;
                    }
                }
                if (auto *CI = dyn_cast<CallInst>(&I)) {
                    auto *Callee = CI->getCalledFunction();
                    if (Callee &&
                        Callee->getName() == "rtems_task_delete" &&
                        CI->arg_size() >= 1) {
                        // RTEMS_SELF == 0
                        if (auto *C =
                                dyn_cast<ConstantInt>(CI->getArgOperand(0)))
                            if (C->isZero())
                                deletedSelf = true;
                    }
                }
            }
        }
    }

    void checkUnusedStatus(Function &F, std::vector<Diagnostic> &diags) {
        for (auto &BB : F)
            for (auto &I : BB)
                if (auto *CI = dyn_cast<CallInst>(&I)) {
                    auto *Callee = CI->getCalledFunction();
                    if (!Callee) continue;
                    if (!statusAPIs().count(Callee->getName().str())) continue;
                    // 반환값이 전혀 사용되지 않으면 경고
                    if (CI->use_empty())
                        diags.push_back({Diagnostic::Warning, name(),
                            F.getName().str(),
                            "Return value of '" +
                                Callee->getName().str() + "' ignored",
                            getLine(I)});
                }
    }

    static unsigned getLine(const Instruction &I) {
        if (const auto &DL = I.getDebugLoc()) return DL.getLine();
        return 0;
    }
};

std::unique_ptr<AnalysisPass> createRTEMSAPIChecker() {
    return std::make_unique<RTEMSAPIChecker>();
}

} // namespace caii