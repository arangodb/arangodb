#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_set>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ToolCategory("find-activity-subclasses");
static cl::opt<std::string> OutputDir(
    "output-dir",
    cl::desc("Directory to write per-class Markdown fragments into"),
    cl::value_desc("dir"), cl::Required, cl::cat(ToolCategory));
static cl::opt<std::string> BaseClass(
    "base-class", cl::desc("Fully qualified base class to enumerate"),
    cl::value_desc("name"),
    cl::init("::arangodb::activities::Activity"), cl::cat(ToolCategory));
static cl::opt<std::string> GuardedTemplate(
    "guarded-template",
    cl::desc("Simple name of the CRTP template carrying the Data type"),
    cl::value_desc("name"), cl::init("GuardedActivity"), cl::cat(ToolCategory));

namespace {

std::string sanitizeFilename(std::string name) {
  for (auto& c : name) {
    if (c == ':' || c == '<' || c == '>' || c == ',' || c == ' ' || c == '/' ||
        c == '\\' || c == '*' || c == '&') {
      c = '_';
    }
  }
  return name;
}

std::string typeToString(QualType qt, ASTContext const& ctx) {
  PrintingPolicy pp(ctx.getLangOpts());
  pp.SuppressTagKeyword = true;
  pp.SuppressScope = false;
  pp.FullyQualifiedName = false;
  pp.SuppressUnwrittenScope = true;
  pp.PrintCanonicalTypes = false;
  return qt.getAsString(pp);
}

ClassTemplateSpecializationDecl const* asTemplateSpecialization(
    CXXRecordDecl const* rd) {
  if (rd == nullptr) return nullptr;
  return dyn_cast<ClassTemplateSpecializationDecl>(rd);
}

CXXRecordDecl const* findGuardedDataType(CXXRecordDecl const* derived,
                                         std::string const& templateName) {
  if (derived == nullptr || !derived->hasDefinition()) return nullptr;
  for (auto const& base : derived->bases()) {
    QualType bt = base.getType().getCanonicalType();
    CXXRecordDecl const* baseRecord = bt->getAsCXXRecordDecl();
    if (baseRecord == nullptr) continue;
    if (baseRecord->getName() != templateName) continue;
    auto const* spec = asTemplateSpecialization(baseRecord);
    if (spec == nullptr) continue;
    auto const& args = spec->getTemplateArgs();
    if (args.size() < 2) continue;
    TemplateArgument const& dataArg = args.get(1);
    if (dataArg.getKind() != TemplateArgument::Type) continue;
    QualType dataType = dataArg.getAsType();
    if (CXXRecordDecl const* dataRecord = dataType->getAsCXXRecordDecl()) {
      return dataRecord;
    }
  }
  return nullptr;
}

bool isInProject(SourceLocation loc, SourceManager const& sm) {
  if (loc.isInvalid()) return false;
  loc = sm.getFileLoc(loc);
  if (loc.isInvalid()) return false;
  if (sm.isInSystemHeader(loc)) return false;
  StringRef fn = sm.getFilename(loc);
  if (fn.empty()) return false;
  if (fn.contains("/3rdParty/")) return false;
  if (fn.contains("/build/") || fn.contains("/build-presets/")) return false;
  return true;
}

// Returns true if at least one row was emitted.
bool emitFieldsTable(std::ostringstream& out, CXXRecordDecl const* record,
                     ASTContext const& ctx) {
  std::ostringstream rows;
  bool any = false;
  for (FieldDecl const* f : record->fields()) {
    if (f->getAccess() == AS_private || f->getAccess() == AS_protected) {
      continue;
    }
    rows << "| `" << f->getNameAsString() << "` | `"
         << typeToString(f->getType(), ctx) << "` |\n";
    any = true;
  }
  if (!any) return false;
  out << "| Field | Type |\n| --- | --- |\n" << rows.str();
  return true;
}

bool isStdRecord(CXXRecordDecl const* rd, SourceManager const& sm) {
  if (rd == nullptr) return false;
  SourceLocation loc = sm.getFileLoc(rd->getLocation());
  if (loc.isInvalid()) return true;
  if (sm.isInSystemHeader(loc)) return true;
  // Belt-and-suspenders: anything declared inside namespace std.
  for (DeclContext const* dc = rd->getDeclContext(); dc != nullptr;
       dc = dc->getParent()) {
    if (auto const* ns = dyn_cast<NamespaceDecl>(dc)) {
      if (ns->getName() == "std") return true;
    }
  }
  return false;
}

class ActivityCallback : public MatchFinder::MatchCallback {
 public:
  ActivityCallback() = default;

  void run(MatchFinder::MatchResult const& result) override {
    auto const* rd = result.Nodes.getNodeAs<CXXRecordDecl>("activity");
    if (rd == nullptr) return;
    if (!rd->hasDefinition()) return;
    if (rd->getDescribedClassTemplate() != nullptr) return;
    if (isa<ClassTemplatePartialSpecializationDecl>(rd)) return;

    SourceManager const& sm = *result.SourceManager;
    SourceLocation loc = sm.getFileLoc(rd->getLocation());
    if (!isInProject(loc, sm)) return;

    std::string qualified = rd->getCanonicalDecl()->getQualifiedNameAsString();
    if (!_seen.insert(qualified).second) return;

    CXXRecordDecl const* dataRecord =
        findGuardedDataType(rd, GuardedTemplate.getValue());

    std::string filename = OutputDir + "/" + sanitizeFilename(qualified) + ".md";
    int fd = ::open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
      if (errno == EEXIST) return;
      llvm::errs() << "open(" << filename << ") failed: " << strerror(errno)
                   << "\n";
      return;
    }

    std::ostringstream out;
    StringRef relPath = sm.getFilename(loc);
    unsigned line = sm.getSpellingLineNumber(loc);

    out << "## " << qualified << "\n\n";
    out << "Source: `" << relPath.str() << ":" << line << "`\n\n";

    out << "### Snapshot envelope\n\n";
    out << "| Field | Type |\n| --- | --- |\n";
    out << "| `id` | `ActivityId` |\n";
    out << "| `parent` | `std::optional<ActivityId>` |\n";
    out << "| `type` | `ActivityType` |\n";
    out << "| `created` | `ActivityCreated` |\n";
    out << "| `data` | _see Data fields below_ |\n\n";

    if (dataRecord == nullptr) {
      out << "_Data type could not be resolved (no `" << GuardedTemplate
          << "` base found)._\n\n";
    } else {
      // Look up the actual base specialization to get the spelled Data type
      // (with template args) for display.
      std::string dataDisplay;
      for (auto const& base : rd->bases()) {
        QualType bt = base.getType().getCanonicalType();
        CXXRecordDecl const* baseRecord = bt->getAsCXXRecordDecl();
        if (baseRecord == nullptr) continue;
        if (baseRecord->getName() != GuardedTemplate.getValue()) continue;
        if (auto const* spec =
                dyn_cast<ClassTemplateSpecializationDecl>(baseRecord)) {
          if (spec->getTemplateArgs().size() >= 2) {
            QualType dt = spec->getTemplateArgs().get(1).getAsType();
            dataDisplay = typeToString(dt, *result.Context);
          }
        }
        break;
      }
      if (dataDisplay.empty()) {
        dataDisplay =
            dataRecord->getCanonicalDecl()->getQualifiedNameAsString();
      }

      if (isStdRecord(dataRecord, sm)) {
        out << "### Data\n\n";
        out << "Data type: `" << dataDisplay << "`\n\n";
        out << "Serialized as a dynamic container; field names are not "
               "statically known.\n\n";
      } else {
        out << "### Data fields (`" << dataDisplay << "`)\n\n";
        bool emitted = emitFieldsTable(out, dataRecord, *result.Context);
        if (!emitted) {
          out << "_(no public fields)_\n";
        }
        out << "\n";

        // One level of recursion: for each Data field whose type is a
        // project-local record (with at least one public field), emit it.
        std::unordered_set<std::string> nestedSeen;
        std::string dataQualified =
            dataRecord->getCanonicalDecl()->getQualifiedNameAsString();
        for (FieldDecl const* f : dataRecord->fields()) {
          QualType ft = f->getType().getNonReferenceType();
          // Drill through one level of the most common containers so
          // vector<T> / optional<T> etc. expose T.
          QualType inner = ft;
          if (auto const* tst = inner->getAs<TemplateSpecializationType>()) {
            if (tst->template_arguments().size() >= 1) {
              TemplateArgument const& a = tst->template_arguments().front();
              if (a.getKind() == TemplateArgument::Type) {
                inner = a.getAsType();
              }
            }
          }
          CXXRecordDecl const* nested = inner->getAsCXXRecordDecl();
          if (nested == nullptr) continue;
          if (isStdRecord(nested, sm)) continue;
          SourceLocation nloc = sm.getFileLoc(nested->getLocation());
          if (!isInProject(nloc, sm)) continue;
          std::string nq =
              nested->getCanonicalDecl()->getQualifiedNameAsString();
          if (nq == dataQualified) continue;
          if (!nestedSeen.insert(nq).second) continue;

          // Only emit if the nested record actually has public fields —
          // skip opaque ID wrappers like TransactionId.
          std::ostringstream nestedOut;
          if (!emitFieldsTable(nestedOut, nested, *result.Context)) continue;

          out << "#### " << nq << "\n\n" << nestedOut.str() << "\n";
        }
      }
    }

    std::string body = out.str();
    ssize_t written = ::write(fd, body.data(), body.size());
    ::close(fd);
    if (written != static_cast<ssize_t>(body.size())) {
      llvm::errs() << "short write to " << filename << "\n";
    }
  }

 private:
  std::unordered_set<std::string> _seen;
};

}  // namespace

int main(int argc, char const** argv) {
  auto expected = CommonOptionsParser::create(
      argc, argv, ToolCategory, llvm::cl::ZeroOrMore,
      "Find every concrete subclass of an Activity-style base class and emit "
      "a Markdown fragment per class.");
  if (!expected) {
    llvm::errs() << toString(expected.takeError());
    return 1;
  }
  CommonOptionsParser& options = *expected;

  if (::mkdir(OutputDir.c_str(), 0755) != 0 && errno != EEXIST) {
    llvm::errs() << "mkdir(" << OutputDir << ") failed: " << strerror(errno)
                 << "\n";
    return 1;
  }

  ClangTool tool(options.getCompilations(), options.getSourcePathList());

  ActivityCallback callback;
  MatchFinder finder;
  finder.addMatcher(
      cxxRecordDecl(isDefinition(),
                    isDerivedFrom(hasName(BaseClass.getValue())),
                    unless(hasName(GuardedTemplate.getValue())))
          .bind("activity"),
      &callback);

  return tool.run(newFrontendActionFactory(&finder).get());
}
