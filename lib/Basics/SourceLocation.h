////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iosfwd>

namespace arangodb::basics {

// Poor-man's replacement for std::source_location, until we get C++20.
struct SourceLocation {
 private:
  char const* _fileName;
  int _line;

 public:
  SourceLocation() = delete;
  // cppcheck-suppress unknownMacro
  constexpr SourceLocation(decltype(_fileName) file,
                           decltype(_line) line) noexcept
      : _fileName(file), _line(line) {}

  [[nodiscard]] constexpr auto file_name() const noexcept
      -> decltype(_fileName) {
    return _fileName;
  }
  [[nodiscard]] constexpr auto line() const noexcept -> decltype(_line) {
    return _line;
  }
};

auto operator<<(std::ostream&, SourceLocation const&) -> std::ostream&;

}  // namespace arangodb::basics

#define ADB_HERE (::arangodb::basics::SourceLocation(__FILE__, __LINE__))
