#pragma once

#include "clang/Tooling/CompilationDatabase.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace sources {

using Database = std::unique_ptr<clang::tooling::CompilationDatabase>;
struct Error {
  std::string message;

  auto operator==(Error const&) const -> bool = default;
};
using DatabaseOrError = std::variant<Database, Error>;

/**
 * Compilation database of the arangodb build in `build_path`.
 */
auto get_database(std::string const& build_path) -> DatabaseOrError;

/**
 * The translation units of `database` to scan for the user's `source_paths`.
 *
 * Each source path is a file or a directory (searched recursively). Headers are
 * routed to their sibling .cpp. A source path is skipped when it has no source
 * file, lies in an ignored directory or is not part of `database`. The result
 * are the matching database files, each once.
 *
 * Example:
 *   sources::get_sources(database, {"arangod/Cluster"});
 */
auto get_sources(clang::tooling::CompilationDatabase const& database,
                 std::vector<std::string> const& source_paths)
    -> std::vector<std::string>;

}  // namespace sources
