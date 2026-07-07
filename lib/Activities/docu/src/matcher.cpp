#include "matcher.h"

constexpr char const* BASE_CLASS = "::arangodb::activities::Activity";
// File paths that can never own a real activity declaration: build outputs,
// 3rdParty/, and the Activity library itself (where Registry, GuardedActivity,
// ... declare activity-typed fields as internal plumbing).
constexpr char const* EXCLUDED_FILE_REGEX =
    "/3rdParty/|/build(-presets)?/|/lib/Activities/";

using namespace clang::ast_matchers;

namespace {

auto activity_filter(std::string_view activity_binding) {
  auto activity_subclass =
      cxxRecordDecl(
          isDerivedFrom(hasName(BASE_CLASS)), hasDefinition(),
          unless(classTemplatePartialSpecializationDecl())  // concrete subclass
          )
          .bind(activity_binding);

  return hasType(hasUnqualifiedDesugaredType(recordType(hasDeclaration(
      anyOf(activity_subclass,
            classTemplateSpecializationDecl(
                hasAnyName("::std::shared_ptr", "::std::unique_ptr"),
                hasTemplateArgument(
                    0, refersToType(hasDeclaration(activity_subclass)))))))));
  // refersToType drills from the template argument back to the CXXRecordDecl,
  // so the same activity_subclass matcher applies here too
}

auto project_filter() {
  return allOf(unless(isExpansionInSystemHeader()),
               unless(isExpansionInFileMatching(EXCLUDED_FILE_REGEX)));
}

}  // namespace

auto matcher::activity_as_field(std::string_view activity_binding)
    -> internal::BindableMatcher<clang::Decl> {
  return fieldDecl(activity_filter(activity_binding), project_filter());
}

auto matcher::activity_as_variable(std::string_view activity_binding)
    -> internal::BindableMatcher<clang::Decl> {
  return varDecl(activity_filter(activity_binding), project_filter(),
                 unless(parmVarDecl())  // no function parameters
  );
}
