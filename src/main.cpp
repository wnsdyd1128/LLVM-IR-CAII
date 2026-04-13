#include "caii/AnalysisPass.hpp"
#include "caii/IRLoader.hpp"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/raw_ostream.h>

#include <memory>
#include <vector>

// 각 Pass의 팩토리 함수 선언
namespace caii {
std::unique_ptr<AnalysisPass> createNullDerefChecker();
std::unique_ptr<AnalysisPass> createStackUsageAnalyzer();
std::unique_ptr<AnalysisPass> createRTEMSAPIChecker();
} // namespace caii

using namespace llvm;

// ── CLI 옵션 ──────────────────────────────────────────────────────────
static cl::opt<std::string>
    InputFile(cl::Positional, cl::desc("<input .bc/.ll>"), cl::Required);

static cl::opt<bool>
    NoColor("no-color", cl::desc("Disable colored output"), cl::init(false));

static cl::list<std::string>
    EnabledPasses("pass",
        cl::desc("Enable specific pass (default: all). Repeatable."),
        cl::value_desc("pass-name"));

// ── 출력 헬퍼 ─────────────────────────────────────────────────────────
static void printDiagnostic(const caii::Diagnostic &D) {
    const char *tag  = "";
    const char *col  = "";
    const char *reset = NoColor ? "" : "\033[0m";

    switch (D.severity) {
    case caii::Diagnostic::Error:
        tag = "error";
        col = NoColor ? "" : "\033[1;31m";
        break;
    case caii::Diagnostic::Warning:
        tag = "warning";
        col = NoColor ? "" : "\033[1;33m";
        break;
    case caii::Diagnostic::Note:
        tag = "note";
        col = NoColor ? "" : "\033[1;36m";
        break;
    }

    llvm::errs() << col << "[" << D.pass << "] " << reset
                 << D.function;
    if (D.line) llvm::errs() << ":" << D.line;
    llvm::errs() << ": " << col << tag << reset << ": " << D.message << "\n";
}

// ── main ──────────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    cl::ParseCommandLineOptions(argc, argv,
        "caii-analyzer — RTEMS/GR740 LLVM IR Static Analyzer\n");

    // IR 로드
    LLVMContext Ctx;
    auto M = caii::loadIR(Ctx, InputFile);
    if (!M) return 1;

    // Pass 등록
    std::vector<std::unique_ptr<caii::AnalysisPass>> passes;
    passes.push_back(caii::createNullDerefChecker());
    passes.push_back(caii::createStackUsageAnalyzer());
    passes.push_back(caii::createRTEMSAPIChecker());

    // 필터
    auto isEnabled = [&](const std::string &pname) -> bool {
        if (EnabledPasses.empty()) return true;
        for (auto &e : EnabledPasses)
            if (e == pname) return true;
        return false;
    };

    // 실행
    int errors = 0, warnings = 0;
    for (auto &pass : passes) {
        if (!isEnabled(pass->name())) continue;
        auto diags = pass->run(*M);
        for (auto &d : diags) {
            printDiagnostic(d);
            if (d.severity == caii::Diagnostic::Error) ++errors;
            else if (d.severity == caii::Diagnostic::Warning) ++warnings;
        }
    }

    llvm::errs() << "\n=== " << errors << " error(s), "
                 << warnings << " warning(s) ===\n";
    return errors > 0 ? 1 : 0;
}
