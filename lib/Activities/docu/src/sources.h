#pragma once

#include "clang/Tooling/CompilationDatabase.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sources {

/**
 * The compile database plus the list of source files to feed to ClangTool.
 */
struct Sources {
  std::unique_ptr<clang::tooling::CompilationDatabase> db;
  std::vector<std::string> files;
};

/**
 * Resolve `path_name` (a file or a directory) into a Sources payload.
 *
 * Returns:
 *   - nullopt when `path_name` is neither a regular file nor a directory.
 *   - Sources with `db == nullptr` when no compile_commands.json was found.
 *   - Sources with `files` empty when no project sources matched.
 */
auto get_sources(std::string const& path_name) -> std::optional<Sources>;

}  // namespace sources
