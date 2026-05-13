#include "sources.h"

#include <filesystem>

namespace {
/**
 * Route a header path to its sibling translation-unit source.
 *
 * Headers aren't in compile_commands.json, so ClangTool needs a `.cpp`/
 * `.cc`/`.cxx` that includes them. If no sibling exists or the input is
 * already a TU, `p` is returned unchanged.
 */
auto resolve_source_file(std::filesystem::path const& p)
    -> std::filesystem::path {
  auto ext = p.extension().string();
  if (ext != ".h" && ext != ".hpp" && ext != ".hxx") return p;
  for (auto const* candExt : {".cpp", ".cc", ".cxx"}) {
    auto candidate = p;
    candidate.replace_extension(candExt);
    if (std::filesystem::is_regular_file(candidate)) return candidate;
  }
  return p;
}
}  // namespace

auto sources::get_sources(std::string const& path_name)
    -> std::optional<Sources> {
  namespace fs = std::filesystem;

  auto ec = std::error_code{};
  auto path = fs::path(path_name);
  if (!fs::exists(path, ec) || ec) return {};

  if (fs::is_directory(path)) {
    auto err = std::string{};
    auto db = clang::tooling::CompilationDatabase::autoDetectFromDirectory(
        path_name, err);
    if (!db) return Sources{};

    auto prefix = fs::canonical(path).string();
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';
    auto files = std::vector<std::string>{};
    for (auto const& file : db->getAllFiles()) {
      if (file.starts_with(prefix) && fs::is_regular_file(file)) {
        files.push_back(file);
      }
    }
    return Sources{.db = std::move(db), .files = files};

  } else if (fs::is_regular_file(path)) {
    auto source = resolve_source_file(fs::canonical(path));
    auto err = std::string{};
    auto db = clang::tooling::CompilationDatabase::autoDetectFromSource(
        source.string(), err);
    if (!db) return Sources{};
    return Sources{.db = std::move(db), .files = {source.string()}};

  } else {
    return Sources{};
  }
}
