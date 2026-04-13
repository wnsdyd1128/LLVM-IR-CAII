#include "caii/AnalysisPass.hpp"

#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#include <llvm/Support/raw_ostream.h>

namespace caii {
std::unique_ptr<AnalysisPass> createNullDerefChecker();
std::unique_ptr<AnalysisPass> createStackUsageAnalyzer();
std::unique_ptr<AnalysisPass> createRTEMSAPIChecker();
} // namespace caii

using namespace llvm;

/// opt --load-pass-plugin=./caii_plugin.so --passes="caii-all" 로 실행
struct CAIIModulePass : public PassInfoMixin<CAIIModulePass> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        std::vector<std::unique_ptr<caii::AnalysisPass>> passes;
        passes.push_back(caii::createNullDerefChecker());
        passes.push_back(caii::createStackUsageAnalyzer());
        passes.push_back(caii::createRTEMSAPIChecker());

        int total = 0;
        for (auto &p : passes)
            for (auto &d : p->run(M)) {
                ++total;
                llvm::errs() << "[" << d.pass << "] " << d.function;
                if (d.line) llvm::errs() << ":" << d.line;
                llvm::errs() << ": " << d.message << "\n";
            }

        llvm::errs() << "=== caii: " << total << " diagnostic(s) ===\n";
        return PreservedAnalyses::all();
    }
};

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "caii", LLVM_VERSION_STRING,
            [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, ModulePassManager &MPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "caii-all") {
                            MPM.addPass(CAIIModulePass{});
                            return true;
                        }
                        return false;
                    });
            }};
}
