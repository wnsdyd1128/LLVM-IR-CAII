#pragma once

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <string>

namespace caii {

/// .bc / .ll 파일을 로드해 Module을 반환한다.
/// 실패 시 nullptr 반환 (오류 메시지는 stderr 출력).
std::unique_ptr<llvm::Module> loadIR(llvm::LLVMContext &Ctx,
                                     const std::string &Path);

} // namespace caii
