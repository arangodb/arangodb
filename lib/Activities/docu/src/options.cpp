#include "options.h"

#include <optional>

namespace {

constexpr std::string_view BUILD_PATH_LONG = "--build-path";
constexpr std::string_view BUILD_PATH_SHORT = "-p";

/**
 * The value glued to `argument` after `flag=`, if `argument` has that form.
 *
 * e.g. returns "build" from "--build-path=build"
 */
auto attached_value(std::string_view argument, std::string_view flag)
    -> std::optional<std::string_view> {
  auto const prefix = std::string{flag} + "=";
  if (argument.starts_with(prefix)) {
    return argument.substr(prefix.size());
  }
  return std::nullopt;
}

}  // namespace

auto options::parse(std::vector<std::string_view> const& arguments)
    -> ParseResult {
  auto build_path = std::optional<std::string>{};
  auto source_paths = std::vector<std::string>{};

  for (auto index = std::size_t{0}; index < arguments.size(); ++index) {
    auto const argument = arguments[index];

    // help
    if (argument == "-h" || argument == "--help") {
      return HelpRequested{};
    }

    // build-path
    if (argument == BUILD_PATH_LONG || argument == BUILD_PATH_SHORT) {
      if (index + 1 >= arguments.size()) {
        return Error{.message = "missing value for " + std::string{argument}};
      }
      if (build_path.has_value()) {
        return Error{.message = "--build-path given more than once"};
      }
      build_path = std::string{arguments[++index]};
      continue;
    }
    if (auto const value = attached_value(argument, BUILD_PATH_LONG)) {
      if (build_path.has_value()) {
        return Error{.message = "--build-path given more than once"};
      }
      build_path = std::string{*value};
      continue;
    }

    // other option
    if (argument.starts_with('-')) {
      return Error{.message = "unknown option " + std::string{argument}};
    }

    // source paths
    source_paths.emplace_back(argument);
  }

  if (not build_path.has_value()) {
    return Error{.message = "missing required --build-path"};
  }
  if (source_paths.empty()) {
    return Error{.message = "missing source path: at least one is required"};
  }
  return Options{.build_path = std::move(*build_path),
                 .source_paths = std::move(source_paths)};
}

auto options::usage() -> std::string {
  return "usage: find-activity-subclasses --build-path <build-path> "
         "<source-path> [<source-path> ...]\n"
         "\n"
         "  -p, --build-path <build-path>  arangodb build directory holding "
         "compile_commands.json (required)\n"
         "  <source-path>                  file or directory to search for "
         "activities (at least one required;\n"
         "                                 directories are searched "
         "recursively). Paths must be part of the\n"
         "                                 compilation database; paths inside "
         "<root>/Documentation and\n"
         "                                 <root>/3rdParty are ignored. "
         "Skipped "
         "paths give a warning.\n"
         "  -h, --help                     show this help and exit\n";
}
