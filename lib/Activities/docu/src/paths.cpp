#include "paths.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

/**
 * The path split into its components, e.g. "/a/b" into {"/", "a", "b"}.
 */
auto path_components(std::string const& path) -> std::vector<std::string> {
  auto const parts =
      fs::path{path} |
      std::views::transform([](fs::path const& part) { return part.string(); });
  return {parts.begin(), parts.end()};
}

}  // namespace

auto paths::is_inside(std::string_view path,
                      std::filesystem::path const& directory) -> bool {
  std::string_view const directory_string = directory.native();
  if (path == directory_string) {
    return true;
  }
  return path.size() > directory_string.size() &&
         path.starts_with(directory_string) &&
         path[directory_string.size()] == '/';
}

auto paths::absolute_path(std::string const& path)
    -> std::optional<std::filesystem::path> {
  auto error = std::error_code{};
  auto const canonical =
      std::filesystem::canonical(std::filesystem::path{path}, error);
  if (not error) {
    return canonical;
  }
  return std::nullopt;
}

auto paths::repository_root(std::vector<std::string> const& files)
    -> std::optional<std::string> {
  if (files.empty()) {
    return std::nullopt;
  }

  auto common = path_components(files.front());
  for (auto const& file : files) {
    auto const components = path_components(file);
    auto const [common_end, unused] = std::ranges::mismatch(common, components);
    common.erase(common_end, common.end());
  }

  auto root = fs::path{};
  for (auto const& component : common) {
    root /= component;
  }
  return root.string();
}
