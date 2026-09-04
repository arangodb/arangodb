#include "sources.h"

#include "paths.h"
#include "logging.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <optional>
#include <ranges>
#include <string_view>
#include <unordered_set>

namespace {

namespace fs = std::filesystem;

/**
 * The translation-unit source for `path`.
 *
 * A non-header is used unchanged.
 */
auto convert_header_to_source(fs::path const& path) -> std::optional<fs::path> {
  auto const extension = path.extension().string();
  if (extension != ".h" && extension != ".hpp" && extension != ".hxx") {
    return path;
  }
  for (auto const* candidate_extension : {".cpp", ".cc", ".cxx"}) {
    auto candidate = path;
    candidate.replace_extension(candidate_extension);
    auto error = std::error_code{};
    if (fs::is_regular_file(candidate, error) && not error) {
      return candidate;
    }
  }
  return std::nullopt;
}

/**
 * The `<root>/Documentation` and `<root>/3rdParty` directories to ignore.
 */
auto ignored_paths(std::string root) -> std::vector<fs::path> {
  return {fs::path{root} / "Documentation", fs::path{root} / "3rdParty"};
}

/**
 * Whether `path` lies in one of the ignored directories.
 */
auto is_ignored(fs::path const& path,
                std::vector<fs::path> const& ignored_paths) -> bool {
  return std::ranges::any_of(ignored_paths, [&](fs::path const& ignored_path) {
    return paths::is_inside(path.native(), ignored_path);
  });
}

}  // namespace

auto sources::get_database(std::string const& build_path) -> DatabaseOrError {
  auto error = std::error_code{};
  if (not fs::is_directory(fs::path{build_path}, error) || error) {
    return Error{.message = "Cannot find build directory " + build_path};
  }

  auto load_error = std::string{};
  auto database = clang::tooling::CompilationDatabase::loadFromDirectory(
      build_path, load_error);
  if (database == nullptr) {
    return Error{.message =
                     "Cannot find a compilation database (compile_commands.json"
                     ") in build directory " +
                     build_path};
  }
  return database;
}

auto sources::get_sources(clang::tooling::CompilationDatabase const& database,
                          std::vector<std::string> const& paths)
    -> std::vector<std::string> {
  auto const database_files = database.getAllFiles();
  auto const root = paths::repository_root(database_files);
  if (not root.has_value()) {
    log::warn("compilation database is empty; nothing to scan");
    return {};
  }
  auto const ignored = ignored_paths(root.value());

  auto files = std::vector<std::string>{};
  auto seen = std::unordered_set<std::string>{};
  for (auto const& path : paths) {
    auto const absolute = paths::absolute_path(path);
    if (not absolute.has_value()) {
      log::warn("skipping {}: file does not exist", path);
      continue;
    }
    // headers aren't in compile_commands.json, so convert to source file
    auto const source = convert_header_to_source(*absolute);
    if (not source.has_value()) {
      log::warn("skipping {}: has no sibling source-code file", path);
      continue;
    }
    if (is_ignored(*source, ignored)) {
      log::warn("skipping {}: lies in an ignored directory", path);
      continue;
    }

    // add all database_files inside path
    auto matched = false;
    for (auto const& file : database_files) {
      if (paths::is_inside(file, *source)) {
        matched = true;
        if (seen.insert(file).second) {
          files.push_back(file);
        }
      }
    }
    if (not matched) {
      log::warn("skipping {}: not part of the compilation database", path);
    }
  }
  return files;
}
