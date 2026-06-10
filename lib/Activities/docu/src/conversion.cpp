#include "conversion.h"
#include <clang/AST/DeclCXX.h>

#include "clang/AST/QualTypeNames.h"
#include <ranges>
#include <string>
#include <unordered_set>
#include <vector>

constexpr char const* GUARDED_TEMPLATE = "GuardedActivity";

using namespace clang;

/**
 * - QualType: type reference with qualifiers, e.g. pointers, references, class
 *             types, typedefs, ...
 * - CXXRecordDecl: definition of a C++ class, struct or union
 */

namespace {

/**
 *Materialize a range into a vector
 */
template<std::ranges::input_range R>
auto to_vector(R&& r) -> std::vector<std::ranges::range_value_t<R>> {
  return {r.begin(), r.end()};
}

/**
 * Convert a clang QualType including all namespace prefixes to a human-readable
 * string.
 */
auto fully_qualified_type_to_string(QualType qt, ASTContext const& ctx)
    -> std::string {
  auto pp = PrintingPolicy(ctx.getLangOpts());
  // PrintingPolicy::FullyQualifiedName alone doesn't fully qualify nested
  // identifiers
  return TypeName::getFullyQualifiedType(qt, ctx).getAsString(pp);
}
/**
 * Convert a clang QualType to a human-readable string.
 */
auto type_to_string(QualType qt, ASTContext const& ctx) -> std::string {
  auto pp = PrintingPolicy(ctx.getLangOpts());
  return qt.getAsString(pp);
}

/**
 * Whether a source location is inside the project's own code.
 *
 * Excludes invalid locations, system headers, `3rdParty/`, and build output.
 */
auto is_in_project(SourceLocation loc, SourceManager const& sm) -> bool {
  if (loc.isInvalid()) {
    return false;
  }
  if (sm.isInSystemHeader(loc)) {
    return false;
  }
  auto fn = sm.getFilename(loc);
  if (fn.empty()) {
    return false;
  }
  if (fn.contains("/3rdParty/")) {
    return false;
  }
  if (fn.contains("/build/") || fn.contains("/build-presets/")) {
    return false;
  }
  return true;
}

/**
 * Whether a record is in namespace std or in a system header.
 */
auto is_std_record(CXXRecordDecl const* rd, SourceManager const& sm) -> bool {
  if (rd == nullptr) {
    return false;
  }
  if (rd->isInStdNamespace()) {
    return true;
  }
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
 * Preserves typedef sugar
 * (e.g. GenericActivityData instead of std::unordered_map<...>)
 */
auto get_data_type(CXXRecordDecl const* derived) -> QualType {
  if (derived == nullptr || !derived->hasDefinition()) {
    return {};
  }
  for (auto const& base : derived->bases()) {
    // QualType::getAs<T>() peels through sugar (typedefs, ElaboratedType)
    auto const* tst = base.getType()->getAs<TemplateSpecializationType>();
    if (tst == nullptr) {
      continue;
    }
    auto const* td = tst->getTemplateName().getAsTemplateDecl();
    if (td == nullptr) {
      continue;
    }
    if (td->getName() != GUARDED_TEMPLATE) {
      continue;
    }
    auto args = tst->template_arguments();
    if (args.size() < 2) {
      continue;
    }
    if (args[1].getKind() != TemplateArgument::Type) {
      continue;
    }
    return args[1].getAsType();
  }
  return {};
}

/**
 * Return the first template argument's type or nullopt
 *
 * Used to drill one level into common containers (vector<T>, optional<T>,
 * ...) so the underlying record can be inspected.
 */
auto peel_one_template_arg(QualType qt) -> std::optional<QualType> {
  if (auto const* tst = qt->getAs<TemplateSpecializationType>()) {
    if (!tst->template_arguments().empty()) {
      auto const& arg = tst->template_arguments().front();
      if (arg.getKind() == TemplateArgument::Type) {
        return arg.getAsType();
      }
    }
  }
  return std::nullopt;
}

/**
 * Type definition of activity's data
 *
 * First Struct is the activities data type, following Struct's are data's
 * fields with non-std types and non-std template parameter types, recursively
 * added.
 */
struct TypeDefinition {
  std::vector<Struct> types;

 private:
  std::unordered_set<std::string> seen_types;

  ASTContext const& ctx;
  SourceManager const& sm;

 public:
  TypeDefinition(const clang::ASTContext& ctx, const clang::SourceManager& sm)
      : ctx(ctx), sm(sm) {}
  auto add_field_recursively(CXXRecordDecl const* data_record) -> void {
    if (data_record == nullptr) {
      return;
    }
    if (is_std_record(data_record, sm)) {
      return;
    }

    if (not seen_types
                .insert(
                    data_record->getCanonicalDecl()->getQualifiedNameAsString())
                .second)
      return;

    auto public_members = to_vector(
        data_record->fields() | std::views::filter([this](FieldDecl const* f) {
          return is_public(f) &&
                 is_in_project(sm.getFileLoc(f->getLocation()), sm);
        }));

    types.push_back(Struct{
        .name = data_record->getQualifiedNameAsString(),
        .fields = to_vector(std::ranges::transform_view(
            std::views::all(public_members), [this](FieldDecl const* f) {
              return Member{
                  .name = f->getNameAsString(),
                  .type = fully_qualified_type_to_string(f->getType(), ctx)};
            }))});

    for (auto const& field : public_members) {
      // add record type
      add_field_recursively(
          field->getType().getNonReferenceType()->getAsCXXRecordDecl());

      // add first template parameter type
      if (auto const first_parameter =
              peel_one_template_arg(field->getType().getNonReferenceType());
          first_parameter.has_value()) {
        add_field_recursively(first_parameter.value()->getAsCXXRecordDecl());
      }
    }
  }
};

}  // namespace

auto conversion::ActivityCallback::run(
    clang::ast_matchers::MatchFinder::MatchResult const& result) -> void {
  auto const* decl =
      result.Nodes.getNodeAs<DeclaratorDecl>(_bindings.declaration);
  auto const* rd = result.Nodes.getNodeAs<CXXRecordDecl>(_bindings.record);
  if (decl == nullptr || rd == nullptr) {
    return;
  }

  auto const& sm = *result.SourceManager;
  auto loc = sm.getFileLoc(decl->getLocation());
  auto path = sm.getFilename(loc).str();
  auto line = sm.getSpellingLineNumber(loc);

  // every activity declaration at a distinct source location should be present
  // once in the results
  auto seen_key = path + ":" + std::to_string(line);
  if (!_seen_activities.insert(std::move(seen_key)).second) {
    return;
  }

  auto const data_type = get_data_type(rd);
  if (data_type.isNull()) {
    _out_activities.push_back(
        ActivityDeclaration{.owner_file = std::move(path),
                            .owner_line = line,
                            .type = rd->getQualifiedNameAsString()});
    return;
  }

  auto ASTContext = result.Context;
  auto type_definition = TypeDefinition{*ASTContext, sm};
  type_definition.add_field_recursively(data_type->getAsCXXRecordDecl());

  _out_activities.push_back(ActivityDeclaration{
      .owner_file = std::move(path),
      .owner_line = line,
      .type = rd->getQualifiedNameAsString(),
      .data_type = fully_qualified_type_to_string(data_type, *ASTContext),
      .type_definition = std::move(type_definition.types)});
}
