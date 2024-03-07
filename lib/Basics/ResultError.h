////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <string>
#include <string_view>

#include "Basics/ErrorCode.h"
#include "Basics/voc-errors.h"

namespace arangodb::result {

// arangodb::Error is used in arangodb::Result

class Error final {
 public:
  explicit Error(ErrorCode errorNumber) noexcept(
      noexcept(std::string::allocator_type()));

  Error() : _errorNumber{TRI_ERROR_NO_ERROR}, _errorMessage{""} {};

  Error(ErrorCode errorNumber, std::string_view errorMessage);

  // Include ResultError.tpp when you want to call this function.
  template<typename... Args>
  static auto fmt(ErrorCode, Args&&...) -> Error;

  [[nodiscard]] auto errorNumber() const noexcept -> ErrorCode;
  [[nodiscard]] auto errorMessage() const& noexcept -> std::string_view;
  [[nodiscard]] auto errorMessage() && noexcept -> std::string;

  template<typename S>
  void resetErrorMessage(S&& msg) {
    _errorMessage = std::forward<S>(msg);
  }

  template<typename S>
  void appendErrorMessage(S&& msg) {
    if (_errorMessage.empty()) {
      _errorMessage += errorMessage();
    }
    _errorMessage += std::forward<S>(msg);
  }

  template<typename Inspector>
  friend auto inspect(Inspector& f, Error& x);

  friend auto operator==(Error const& left, Error const& right) noexcept
      -> bool = default;

 private:
  ErrorCode _errorNumber;
  std::string _errorMessage;
};

template<typename Inspector>
auto inspect(Inspector& f, Error& x) {
  return f.object(x).fields(f.field("number", x._errorNumber),
                            f.field("message", x._errorMessage));
}
}  // namespace arangodb::result
