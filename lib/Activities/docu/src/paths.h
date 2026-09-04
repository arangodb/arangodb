#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace paths {

/**
 * Whether `path` equals `directory` or lies somewhere below it.
 *
 * Plain string comparison, so path must be absolute and free of a trailing
 * separator.
 */
auto is_inside(std::string_view path, std::filesystem::path const& directory)
    -> bool;

/**
 * Absolute, symlink-resolved form of `path`.
 */
auto absolute_path(std::string const& path)
    -> std::optional<std::filesystem::path>;

/**
 * Absolute path of the arangodb repository root the compilation database was
 * built from.
 *
 * Derived as the longest directory containing every one of `files` (a
 * database's translation units): all compiled files live under the root and
 * span several of its top-level directories, so their common ancestor is the
 * root itself. nullopt when `files` is empty.
 */
auto repository_root(std::vector<std::string> const& files)
    -> std::optional<std::string>;

}  // namespace paths
