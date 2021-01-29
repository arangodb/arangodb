////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef LIB_BASICS_ERRORCODE_H
#define LIB_BASICS_ERRORCODE_H

#include <iosfwd>

// TODO We probably want to put this into a namespace, but this is easy to
//      refactor automatically later.

class ErrorCode {
 public:
  ErrorCode() = delete;
  constexpr explicit ErrorCode(int value) : _value(value) {}
  constexpr ErrorCode(ErrorCode const&) noexcept = default;
  constexpr ErrorCode(ErrorCode&&) noexcept = default;
  constexpr auto operator=(ErrorCode const&) noexcept -> ErrorCode& = default;
  constexpr auto operator=(ErrorCode&&) noexcept -> ErrorCode& = default;

  // TODO Remove this later, or at least mark it explicit, and fix remaining
  //      compile errors.
  constexpr operator int() const noexcept { return _value; }

  [[nodiscard]] constexpr auto operator==(ErrorCode other) const noexcept -> bool {
    return _value == other._value;
  }

  [[nodiscard]] constexpr auto operator!=(ErrorCode other) const noexcept -> bool {
    return _value != other._value;
  }

  [[nodiscard]] constexpr auto asInt() const noexcept -> int { return _value; }

 private:
  int _value;
};

auto operator<<(std::ostream& out, ::ErrorCode const& res) -> std::ostream&;

#endif  // LIB_BASICS_ERRORCODE_H
