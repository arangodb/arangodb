#pragma once

#include <optional>
#include <string>

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
