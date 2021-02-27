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
#include <string>

namespace arangodb::velocypack {
class Value;
}

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

  [[nodiscard]] constexpr explicit operator int() const noexcept { return _value; }

  // This could also be constexpr, but we'd have to include <velocypack/Value.h>,
  // and I'm unsure whether that's worth it, and rather rely on IPO here.
  [[nodiscard]] explicit operator arangodb::velocypack::Value() const noexcept;

  [[nodiscard]] constexpr auto operator==(ErrorCode other) const noexcept -> bool {
    return _value == other._value;
  }

  [[nodiscard]] constexpr auto operator!=(ErrorCode other) const noexcept -> bool {
    return _value != other._value;
  }

  friend auto to_string(::ErrorCode value) -> std::string;

 private:
  int _value;
};

namespace std {
template <>
struct hash<ErrorCode> {
  auto operator()(ErrorCode const& errorCode) const noexcept -> std::size_t {
    return std::hash<int>{}(static_cast<int>(errorCode));
  }
};
}  // namespace std

auto operator<<(std::ostream& out, ::ErrorCode const& res) -> std::ostream&;

#endif  // LIB_BASICS_ERRORCODE_H
