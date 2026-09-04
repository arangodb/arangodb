#pragma once

#include <format>
#include <iostream>
#include <string_view>
#include <utility>

namespace log {

/**
 * Print a log-message to stderr, prefixed with `name`.
 */
template<class... Args>
auto log(std::string_view name, std::string_view fmt, Args&&... args) -> void {
  std::cerr << name << ": " << std::vformat(fmt, std::make_format_args(args...))
            << '\n';
}
template<class... Args>
auto warn(std::string_view fmt, Args&&... args) -> void {
  return log("warning", fmt, std::forward<Args>(args)...);
}
template<class... Args>
auto err(std::string_view fmt, Args&&... args) -> void {
  return log("error", fmt, std::forward<Args>(args)...);
}

}  // namespace log
