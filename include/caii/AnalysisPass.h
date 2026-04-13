#pragma once

#include <llvm/IR/Module.h>
#include <string>
#include <vector>

namespace caii {

struct Diagnostic {
    enum Severity { Note, Warning, Error };

    Severity    severity;
    std::string pass;       // 발생한 pass 이름
    std::string function;   // 대상 함수
    std::string message;
    unsigned    line = 0;   // IR debug info 있을 때만 유효
};

/// 모든 분석 Pass의 기반 인터페이스
class AnalysisPass {
public:
    virtual ~AnalysisPass() = default;

    virtual std::string name() const = 0;
    virtual std::string description() const = 0;

    /// Module 전체를 분석해 진단 결과를 반환한다.
    virtual std::vector<Diagnostic> run(llvm::Module &M) = 0;
};

} // namespace caii