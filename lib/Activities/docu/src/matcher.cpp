#include "matcher.h"

constexpr char const* BASE_CLASS = "::arangodb::activities::Activity";
constexpr char const* GUARDED_TEMPLATE = "GuardedActivity";
// File paths that can never own a real activity declaration: build outputs,
// 3rdParty/, and the Activity library itself (where Registry, GuardedActivity,
// ... declare activity-typed fields as internal plumbing).
constexpr char const* EXCLUDED_FILE_REGEX =
    "/3rdParty/|/build(-presets)?/|/lib/Activities/";

using namespace clang::ast_matchers;

auto matcher::match(MatchFinder::MatchCallback& callback) -> MatchFinder {
  auto activity_subclass =
      cxxRecordDecl(isDerivedFrom(hasName(BASE_CLASS)),
                    unless(hasName(GUARDED_TEMPLATE)), hasDefinition(),
                    unless(classTemplatePartialSpecializationDecl()))
          .bind("activity_class");

  auto type_filter =
      hasType(hasUnqualifiedDesugaredType(recordType(hasDeclaration(anyOf(
          activity_subclass,
          classTemplateSpecializationDecl(
              hasAnyName("::std::shared_ptr", "::std::unique_ptr"),
              hasTemplateArgument(
                  0, refersToType(hasDeclaration(activity_subclass)))))))));

  auto in_project =
      allOf(unless(isExpansionInSystemHeader()),
            unless(isExpansionInFileMatching(EXCLUDED_FILE_REGEX)));

  auto finder = MatchFinder{};
  finder.addMatcher(fieldDecl(type_filter, in_project).bind("decl"), &callback);
  finder.addMatcher(
      varDecl(type_filter, in_project, unless(parmVarDecl())).bind("decl"),
      &callback);
  return finder;
}
