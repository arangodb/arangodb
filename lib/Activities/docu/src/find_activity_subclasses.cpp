#include "find_activity_subclasses.h"

#include "sources.h"
#include "matcher.h"
#include "conversion.h"

#include "clang/Tooling/Tooling.h"
#include <vector>
#include <iostream>

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
  auto callback = conversion::ActivityCallback(out);
  auto finder = matcher::match(callback);

  auto tool = clang::tooling::ClangTool(*sources.db, sources.files);
  tool.run(clang::tooling::newFrontendActionFactory(&finder).get());

  return out;
}
