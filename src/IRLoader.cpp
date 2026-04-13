#include "caii/IRLoader.hpp"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

namespace caii {

std::unique_ptr<llvm::Module> loadIR(llvm::LLVMContext &Ctx,
                                     const std::string &Path) {
    llvm::SMDiagnostic Err;
    auto M = llvm::parseIRFile(Path, Err, Ctx);
    if (!M) {
        Err.print("caii-analyzer", llvm::errs());
        return nullptr;
    }
    return M;
}

} // namespace caii
