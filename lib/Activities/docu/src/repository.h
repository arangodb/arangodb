#pragma once

#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <vector>

namespace repository {

/**
 * A repository and the commit it was scanned at.
 *
 * Example:
 *   Commit{.repository = "arangodb", .id = "abc1234"};
 */
struct Commit {
  std::string repository;
  std::string id;

  auto operator==(Commit const&) const -> bool = default;
};

/**
 * Commit ids to record for a scan over `source_paths`.
 *
 * The first entry is always arangodb's commit, taken from `build_path`. When
 * any of `source_paths` lies inside `<root>/enterprise`, the enterprise
 * submodule has its own commit, which is appended as a second entry.
 *
 * Example:
 *   for (auto const& commit : commit_ids(database, {"enterprise/Foo.cpp"})) {
 *     std::cout << commit.repository << ": " << commit.id << "\n";
 *   }
 */
auto commit_ids(clang::tooling::CompilationDatabase const& database,
                std::vector<std::string> const& source_paths)
    -> std::vector<Commit>;

}  // namespace repository
