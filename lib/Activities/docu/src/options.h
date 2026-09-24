#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace options {

/**
 * Verified command-line options
 */
struct Options {
  std::string build_path;
  std::vector<std::string> source_paths;

  auto operator==(Options const&) const -> bool = default;
};

/**
 * The user asked for the usage text (-h / --help).
 */
struct HelpRequested {
  auto operator==(HelpRequested const&) const -> bool = default;
};

/**
 * The command line cannot be used; `message` says why.
 */
struct Error {
  std::string message;

  auto operator==(Error const&) const -> bool = default;
};

using ParseResult = std::variant<Options, HelpRequested, Error>;

/**
 * Interpret the command-line arguments (without program name).
 *
 * See usage() for arguments
 *
 * Example:
 *   auto const result = options::parse({"-p", "build", "arangod"});
 *   if (auto const* opts = std::get_if<options::Options>(&result)) {
 *     ...
 *   }
 * Example:
 *   options::parse(std::vector<std::string_view>(argv + 1, argv + argc));
 */
auto parse(std::vector<std::string_view> const& arguments) -> ParseResult;

/**
 * Usage text of the program.
 */
auto usage() -> std::string;

}  // namespace options
