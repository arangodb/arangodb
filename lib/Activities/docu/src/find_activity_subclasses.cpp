#include "find_activity_subclasses.h"

#include "matcher.h"
#include "conversion.h"

#include <clang/Tooling/Tooling.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <vector>

auto find_all_activities(clang::tooling::CompilationDatabase const& database,
                         std::vector<std::string> const& sources)
    -> std::vector<ActivityDeclaration> {
  auto out = std::vector<ActivityDeclaration>{};
  auto callback = conversion::ActivityCallback(
      out, Bindings{.record = "activity_class", .declaration = "decl"});

  auto finder = clang::ast_matchers::MatchFinder{};
  finder.addMatcher(matcher::activity_as_field("activity_class").bind("decl"),
                    &callback);
  finder.addMatcher(
      matcher::activity_as_variable("activity_class").bind("decl"), &callback);

  auto tool = clang::tooling::ClangTool(database, sources);
  tool.run(clang::tooling::newFrontendActionFactory(&finder).get());

  return out;
}
