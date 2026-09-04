#pragma once

#include <optional>
#include <string>

namespace git {

/**
 * Short commit id of the git repository that contains `path`.
 *
 * Describes the checkout at the moment of the call. Accepts a directory or a
 * file path. Returns nullopt when git is unavailable or `path` is not inside a
 * repository.
 *
 * Example:
 *   std::cout << current_commit_id("lib/Activities").value_or("unknown");
 */
auto current_commit_id(std::string const& path) -> std::optional<std::string>;

/**
 * Absolute path of the git repository that contains `path`.
 *
 * Accepts a directory or a file path. Returns nullopt when git is unavailable
 * or `path` is not inside a repository. A path inside a git submodule (such as
 * `3rdParty` or `enterprise`) resolves to that submodule's root, not the
 * superproject's.
 *
 * Example:
 *   std::cout << repository_root("lib/Activities").value_or("");
 */
auto repository_root(std::string const& path) -> std::optional<std::string>;

}  // namespace git
