#include "caii/IRLoader.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

static cl::opt<std::string>
    InputFile(cl::Positional, cl::desc("<input .bc/.ll>"), cl::Required);

int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    cl::ParseCommandLineOptions(argc, argv,
        "caii-analyzer — RTEMS/GR740 LLVM IR Cache Affinity Analyzer\n");

    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, InputFile);
    if (!M) return 1;

    llvm::outs() << "Loaded module: " << M->getName() << "\n";
    llvm::outs() << "Functions: " << M->getFunctionList().size() << "\n";

    // TODO: CacheAnalysisPipeline::run(*M) 호출 예정
    return 0;
}