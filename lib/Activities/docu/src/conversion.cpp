#include "conversion.h"

#include "clang/AST/QualTypeNames.h"
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

constexpr char const* GUARDED_TEMPLATE = "GuardedActivity";  // TODO

using namespace clang;

namespace {

/**
 *Materialize a range into a vector
 */
template<std::ranges::input_range R>
auto to_vector(R&& r) -> std::vector<std::ranges::range_value_t<R>> {
  return {r.begin(), r.end()};
}

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

struct TypeDefinition {
  std::vector<Struct> types;
  std::unordered_set<std::string> seen_types;

  ASTContext const& ctx;
  SourceManager const& sm;

  auto add_field_recursively(CXXRecordDecl const* data_record) -> void {
    if (not seen_types
                .insert(
                    data_record->getCanonicalDecl()->getQualifiedNameAsString())
                .second)
      return;

    auto public_members = to_vector(
        data_record->fields() | std::views::filter([this](FieldDecl const* f) {
          return is_public(f) && is_in_project(f->getLocation(), sm);
        }));

    types.push_back(
        Struct{.name = data_record->getNameAsString(),
               .fields = to_vector(std::ranges::transform_view(
                   std::views::all(public_members), [this](FieldDecl const* f) {
                     return Member{
                         .name = f->getNameAsString(),
                         .type = type_to_string(f->getType(), ctx,
                                                /*is_fully_qualified=*/false)};
                   }))});

    for (auto const& field : public_members) {
      auto const* nested =
          peel_one_template_arg(field->getType().getNonReferenceType())
              ->getAsCXXRecordDecl();
      if (nested != nullptr && !is_std_record(nested, sm)) {
        add_field_recursively(nested);
      }
    }
  }
};

}  // namespace

auto conversion::ActivityCallback::run(
    clang::ast_matchers::MatchFinder::MatchResult const& result) -> void {
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
  if (!_seen_activities.insert(std::move(seen_key)).second) return;

  // Primary class templates aren't usable as field/var types in well-formed
  // code, so the matcher almost never delivers one — guard anyway.
  if (rd->getDescribedClassTemplate() != nullptr) return;
  auto data_type = find_guarded_data_type(rd, GUARDED_TEMPLATE);
  if (data_type.isNull()) {
    _out_activities.push_back(
        ActivityDeclaration{.owner_file = std::move(path), .owner_line = line});
    return;
  }

  auto const* data_record = data_type->getAsCXXRecordDecl();
  if (data_record == nullptr || is_std_record(data_record, sm)) {
    _out_activities.push_back(ActivityDeclaration{
        .owner_file = std::move(path),
        .owner_line = line,
        .data_type = type_to_string(data_type, *result.Context,
                                    /*is_fully_qualified=*/true)});
    return;
  }

  auto type_definition = TypeDefinition{.ctx = *result.Context, .sm = sm};
  type_definition.add_field_recursively(data_record);
  _out_activities.push_back(ActivityDeclaration{
      .owner_file = std::move(path),
      .owner_line = line,
      .data_type = type_to_string(data_type, *result.Context,
                                  /*is_fully_qualified=*/true),
      .type_definition = std::move(type_definition.types)});
}
