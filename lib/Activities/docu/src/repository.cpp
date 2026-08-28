#include "repository.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <string_view>

namespace {

/**
 * Quote `text` so the shell sees it as a single word.
 */
auto shell_quoted(std::string_view text) -> std::string {
  auto quoted = std::string{"'"};
  for (auto const character : text) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  return quoted + "'";
}

/**
 * Directory in which git should be asked about `path`.
 *
 * git needs a directory to work in, so a regular file is answered by its
 * parent.
 */
auto repository_directory(std::filesystem::path const& path)
    -> std::optional<std::filesystem::path> {
  auto error = std::error_code{};
  if (std::filesystem::is_directory(path, error) && not error) {
    return path;
  }
  error.clear();
  if (std::filesystem::is_regular_file(path, error) && not error) {
    auto const parent = path.parent_path();
    return parent.empty() ? std::filesystem::path{"."} : parent;
  }
  return {};
}

/**
 * Standard output of `command`, or nullopt when it could not run or failed.
 */
auto command_output(std::string const& command) -> std::optional<std::string> {
  auto* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  auto output = std::string{};
  auto chunk = std::array<char, 128>{};
  while (std::fgets(chunk.data(), chunk.size(), pipe) != nullptr) {
    output += chunk.data();
  }
  if (pclose(pipe) != 0) {
    return {};
  }
  return output;
}

}  // namespace

auto current_commit_id(std::string const& path) -> std::optional<std::string> {
  auto const directory = repository_directory(path);
  if (not directory.has_value()) {
    return {};
  }

  auto const output =
      command_output("git -C " + shell_quoted(directory->string()) +
                     " rev-parse --short HEAD 2>/dev/null");
  if (not output.has_value()) {
    return {};
  }

  // right-trims the output
  auto const last = output->find_last_not_of(" \t\r\n");
  if (last == std::string::npos) {
    return {};  // output was empty
  }
  return output->substr(0, last + 1);
}
