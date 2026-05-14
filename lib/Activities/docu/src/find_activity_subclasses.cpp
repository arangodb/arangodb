#include "find_activity_subclasses.h"

#include "sources.h"
#include "matcher.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/QualTypeNames.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>
#include <system_error>
#include <unordered_set>
#include <iostream>

constexpr char const* GUARDED_TEMPLATE = "GuardedActivity";  // TODO

using namespace clang;
using namespace clang::tooling;

namespace {

/**
 * Convert a clang QualType to a human-readable string.
 *
 * When `is_fully_qualified` is true, nested identifiers are rewritten with
 * full namespace prefixes everywhere; otherwise the source-as-written
 * spelling is preserved.
 */
auto type_to_string(QualType qt, ASTContext const& ctx, bool is_fully_qualified)
    -> std::string {
  auto pp = PrintingPolicy(ctx.getLangOpts());
  pp.SuppressTagKeyword = true;
  pp.SuppressScope = false;
  pp.FullyQualifiedName = false;
  pp.SuppressUnwrittenScope = true;
  pp.PrintCanonicalTypes = false;
  if (is_fully_qualified) {
    // PrintingPolicy::FullyQualifiedName alone doesn't fully qualify nested
    // identifiers; the dedicated helper walks the type and rewrites it with
    // full namespace prefixes everywhere.
    auto requalified = TypeName::getFullyQualifiedType(qt, ctx);
    return requalified.getAsString(pp);
  }
  return qt.getAsString(pp);
}

/**
 * Whether a source location is inside the project's own code.
 *
 * Excludes invalid locations, system headers, `3rdParty/`, and build output.
 */
auto is_in_project(SourceLocation loc, SourceManager const& sm) -> bool {
  if (loc.isInvalid()) return false;
  loc = sm.getFileLoc(loc);
  if (loc.isInvalid()) return false;
  if (sm.isInSystemHeader(loc)) return false;
  auto fn = sm.getFilename(loc);
  if (fn.empty()) return false;
  if (fn.contains("/3rdParty/")) return false;
  if (fn.contains("/build/") || fn.contains("/build-presets/")) return false;
  return true;
}

/**
 * Whether a record is in namespace std or in a system header.
 */
auto is_std_record(CXXRecordDecl const* rd, SourceManager const& sm) -> bool {
  if (rd == nullptr) return false;
  if (rd->isInStdNamespace()) return true;
  auto loc = sm.getFileLoc(rd->getLocation());
  return loc.isInvalid() || sm.isInSystemHeader(loc);
}

/**
 * Whether a field is publicly accessible.
 */
auto is_public(FieldDecl const* f) -> bool {
  return f->getAccess() == AS_public || f->getAccess() == AS_none;
}

/**
 * Resolve the Data type from `derived`'s GuardedActivity<Self, Data> base.
 *
 * Preserves typedef sugar (so e.g. GuardedActivity<G, GenericActivityData>
 * yields the alias, not its std::unordered_map<...> canonical form).
 * Returns a null QualType if no matching base is found.
 */
auto find_guarded_data_type(CXXRecordDecl const* derived,
                            std::string const& template_name) -> QualType {
  if (derived == nullptr || !derived->hasDefinition()) return {};
  for (auto const& base : derived->bases()) {
    // QualType::getAs<T>() peels through sugar (typedefs, ElaboratedType),
    // so we don't need to strip ElaboratedType manually first.
    auto const* tst = base.getType()->getAs<TemplateSpecializationType>();
    if (tst == nullptr) continue;
    auto const* td = tst->getTemplateName().getAsTemplateDecl();
    if (td == nullptr) continue;
    if (td->getName() != template_name) continue;
    auto args = tst->template_arguments();
    if (args.size() < 2) continue;
    if (args[1].getKind() != TemplateArgument::Type) continue;
    return args[1].getAsType();
  }
  return {};
}

/**
 * Return the first template argument's type, or `qt` unchanged.
 *
 * Used to drill one level into common containers (vector<T>, optional<T>,
 * ...) so the underlying record can be inspected.
 */
auto peel_one_template_arg(QualType qt) -> QualType {
  if (auto const* tst = qt->getAs<TemplateSpecializationType>()) {
    if (!tst->template_arguments().empty()) {
      auto const& arg = tst->template_arguments().front();
      if (arg.getKind() == TemplateArgument::Type) return arg.getAsType();
    }
  }
  return qt;
}

/**
 * Collect the public fields of a record into IR `Member`s.
 */
auto collect_public_members(CXXRecordDecl const* record, ASTContext const& ctx)
    -> std::vector<Member> {
  auto out = std::vector<Member>{};
  if (record == nullptr) return out;
  for (auto const* field : record->fields()) {
    if (!is_public(field)) continue;
    out.push_back(Member{.name = field->getNameAsString(),
                         .type = type_to_string(field->getType(), ctx,
                                                /*is_fully_qualified=*/false)});
  }
  return out;
}

/**
 * Build an IR `ActivityDeclaration` from a matched Activity-subclass record.
 *
 * Returns nullopt for records that aren't real concrete activities (primary
 * templates, partial specializations, undefined classes). The returned
 * declaration's `field_types` is empty when the Data type is a dynamic
 * container or otherwise unresolvable.
 */
auto build_activity_declaration(CXXRecordDecl const* rd, std::string owner_file,
                                unsigned owner_line, ASTContext const& ctx,
                                SourceManager const& sm)
    -> std::optional<ActivityDeclaration> {
  if (rd == nullptr) return std::nullopt;
  // Primary class templates aren't usable as field/var types in well-formed
  // code, so the matcher almost never delivers one — guard anyway.
  if (rd->getDescribedClassTemplate() != nullptr) return std::nullopt;

  auto ad = ActivityDeclaration{};
  ad.owner_file = std::move(owner_file);
  ad.owner_line = owner_line;

  auto data_type = find_guarded_data_type(rd, GUARDED_TEMPLATE);
  if (data_type.isNull()) return ad;

  ad.data_type = type_to_string(data_type, ctx, /*is_fully_qualified=*/true);

  auto const* data_record = data_type->getAsCXXRecordDecl();
  if (data_record == nullptr || is_std_record(data_record, sm)) return ad;

  ad.field_types.push_back(
      Struct{.name = data_record->getNameAsString(),
             .fields = collect_public_members(data_record, ctx)});

  auto seen = std::unordered_set<std::string>{};
  seen.insert(data_record->getCanonicalDecl()->getQualifiedNameAsString());
  for (auto const* field : data_record->fields()) {
    if (!is_public(field)) continue;
    auto inner = peel_one_template_arg(field->getType().getNonReferenceType());
    auto const* nested = inner->getAsCXXRecordDecl();
    if (nested == nullptr || is_std_record(nested, sm)) continue;
    auto nloc = sm.getFileLoc(nested->getLocation());
    if (!is_in_project(nloc, sm)) continue;
    auto nq = nested->getCanonicalDecl()->getQualifiedNameAsString();
    if (!seen.insert(nq).second) continue;
    auto members = collect_public_members(nested, ctx);
    if (members.empty()) continue;
    ad.field_types.push_back(Struct{.name = nested->getNameAsString(),
                                    .fields = std::move(members)});
  }

  return ad;
}

/**
 * MatchFinder callback: collects one ActivityDeclaration per match into the
 * caller-supplied output vector.
 *
 * Dedupes by `owner_file:owner_line`, so the same physical declaration isn't
 * reported across the many TUs that include the same header — but distinct
 * declarations of the same Activity class (e.g. a member field plus a
 * `make<T>` call site elsewhere) both survive. Location-based filtering
 * (system headers, 3rdParty/, build outputs, Activity library internals) is
 * applied in the matcher itself.
 */
class ActivityCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  explicit ActivityCallback(std::vector<ActivityDeclaration>& out)
      : _out(out) {}

  auto run(clang::ast_matchers::MatchFinder::MatchResult const& result)
      -> void override {
    auto const* decl = result.Nodes.getNodeAs<DeclaratorDecl>("decl");
    auto const* rd = result.Nodes.getNodeAs<CXXRecordDecl>("activity_class");
    if (decl == nullptr || rd == nullptr) return;

    auto const& sm = *result.SourceManager;
    auto loc = sm.getFileLoc(decl->getLocation());
    auto path = sm.getFilename(loc).str();
    auto line = sm.getSpellingLineNumber(loc);

    // Dedupe by source location: the same declaration is visited across many
    // TUs, but distinct declarations of the same Activity class (e.g. a
    // member field plus a `make<T>` call site elsewhere) should both survive.
    auto seen_key = path + ":" + std::to_string(line);
    if (!_seen.insert(std::move(seen_key)).second) return;

    auto ad = build_activity_declaration(rd, std::move(path), line,
                                         *result.Context, sm);
    if (!ad) return;
    _out.push_back(std::move(*ad));
  }

 private:
  std::vector<ActivityDeclaration>& _out;
  std::unordered_set<std::string> _seen;
};

}  // namespace

auto find_all_activities(std::string const& path)
    -> std::vector<ActivityDeclaration> {
  auto maybe_sources = sources::get_sources(path);
  if (not maybe_sources.has_value()) {
    std::cerr << "Cannot find directory or regular file at path " << path
              << std::endl;
    return {};
  }
  auto sources = std::move(maybe_sources).value();
  if (sources.db == nullptr) {
    std::cerr
        << "Cannot find a code-database (file 'compile_commands.json') for "
           "the given path "
        << path << std::endl;
    return {};
  }
  if (sources.files.empty()) {
    std::cerr << "Cannot find any files in path " << path << std::endl;
    return {};
  }

  auto out = std::vector<ActivityDeclaration>{};
  auto callback = ActivityCallback(out);
  auto finder = matcher::match(callback);

  auto tool = ClangTool(*sources.db, sources.files);
  tool.run(newFrontendActionFactory(&finder).get());

  return out;
}
