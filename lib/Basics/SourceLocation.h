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

namespace arangodb::basics {

// Poor-man's replacement for std::source_location, until we get C++20.
struct SourceLocation {
 private:
  const char* _file_name;
  int _line;

 public:
  SourceLocation() = delete;
  constexpr SourceLocation(decltype(_file_name) file,
                           decltype(_line) line) noexcept
      : _file_name(file), _line(line) {}

  [[nodiscard]] constexpr auto file_name() const noexcept
      -> decltype(_file_name) {
    return _file_name;
  }
  [[nodiscard]] constexpr auto line() const noexcept -> decltype(_line) {
    return _line;
  }
};

}  // namespace arangodb::basics

#define ADB_HERE (::arangodb::basics::SourceLocation(__FILE__, __LINE__))
