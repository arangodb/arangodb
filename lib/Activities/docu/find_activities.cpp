#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ToolCategory("find-vars-of-type");
static cl::opt<std::string> TypeName(
    "type", cl::desc("Name of the C++ type to search for"),
    cl::value_desc("typename"), cl::Required, cl::cat(ToolCategory));

class VarPrinter : public MatchFinder::MatchCallback {
 public:
  void run(const MatchFinder::MatchResult& Result) override {
    const auto* VD = Result.Nodes.getNodeAs<VarDecl>("var");
    if (!VD) return;

    const SourceManager& SM = *Result.SourceManager;
    SourceLocation Loc = VD->getLocation();

    if (Loc.isInvalid()) return;  // implicit / builtin decls
    Loc = SM.getFileLoc(Loc);     // step out of macros
    if (Loc.isInvalid()) return;
    if (SM.isInSystemHeader(Loc)) return;  // skip <vector>, etc.

    auto Filename = SM.getFilename(Loc);
    if (Filename.empty()) return;

    llvm::outs() << SM.getFilename(Loc) << ":" << SM.getSpellingLineNumber(Loc)
                 << ":" << SM.getSpellingColumnNumber(Loc) << ": "
                 << VD->getQualifiedNameAsString() << "  ["
                 << VD->getType().getAsString() << "]\n";
  }
};

int main(int argc, const char** argv) {
  // llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
  // llvm::PrettyStackTraceProgram X(argc, argv);

  auto Expected = CommonOptionsParser::create(
      argc, argv, ToolCategory, llvm::cl::ZeroOrMore,
      "Find all variables of a given type");
  if (!Expected) {
    llvm::errs() << toString(Expected.takeError());
    return 1;
  }
  CommonOptionsParser& Options = *Expected;
  ClangTool Tool(Options.getCompilations(), Options.getSourcePathList());

  VarPrinter Callback;
  MatchFinder Finder;
  Finder.addMatcher(
      // varDecl(hasType(cxxRecordDecl(hasName(TypeName)))).bind("var"),
      varDecl(hasType(hasUnqualifiedDesugaredType(recordType(
                  hasDeclaration(cxxRecordDecl(hasName(TypeName)))))))
          .bind("var"),
      &Callback);

  return Tool.run(newFrontendActionFactory(&Finder).get());
}
