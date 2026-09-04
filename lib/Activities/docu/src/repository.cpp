#include "repository.h"

#include "git.h"
#include "paths.h"
#include <string>
#include <filesystem>
#include <vector>

namespace {

auto enterprise_source(std::string const root,
                       std::vector<std::string> const& source_paths)
    -> std::optional<std::string> {
  auto const enterprise = std::filesystem::path{root} / "enterprise";

  for (auto const& source_path : source_paths) {
    auto const absolute = paths::absolute_path(source_path);
    if (not absolute.has_value()) {
      continue;
    }
    if (not paths::is_inside(absolute.value().string(), enterprise)) {
      continue;
    }
    return source_path;
  }
  return std::nullopt;
}

}  // namespace

auto repository::commit_ids(clang::tooling::CompilationDatabase const& database,
                            std::vector<std::string> const& source_paths)
    -> std::vector<Commit> {
  auto const root = paths::repository_root(database.getAllFiles());
  if (not root.has_value()) {
    return {};
  }
  auto commits = std::vector<Commit>{
      Commit{.repository = "arangodb",
             .id = git::current_commit_id(root.value()).value_or("unknown")}};

  auto enterprise_source_path = enterprise_source(root.value(), source_paths);
  if (not enterprise_source_path.has_value()) {
    return commits;
  }

  commits.push_back(
      Commit{.repository = "enterprise",
             .id = git::current_commit_id(enterprise_source_path.value())
                       .value_or("unknown")});
  return commits;
}
