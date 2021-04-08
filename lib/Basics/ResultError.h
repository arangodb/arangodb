////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

// Note that error.h uses ARANGODB_BASICS_ERROR_H!
#ifndef ARANGODB_BASICS_RESULT_ERROR_H
#define ARANGODB_BASICS_RESULT_ERROR_H

#include <string>
#include <string_view>

#include "Basics/ErrorCode.h"

namespace arangodb::result {

// arangodb::Error is used in arangodb::Result

class Error final {
 public:
  explicit Error(ErrorCode errorNumber) noexcept(noexcept(std::string::allocator_type()));

  Error(ErrorCode errorNumber, std::string_view errorMessage);
  [[nodiscard]] auto errorNumber() const noexcept -> ErrorCode;
  [[nodiscard]] auto errorMessage() const& noexcept -> std::string_view;
  [[nodiscard]] auto errorMessage() && noexcept -> std::string;

  template <typename S>
  void resetErrorMessage(S&& msg) {
    _errorMessage = std::forward<S>(msg);
  }

  template <typename S>
  void appendErrorMessage(S&& msg) {
    if (_errorMessage.empty()) {
      _errorMessage += errorMessage();
    }
    _errorMessage += std::forward<S>(msg);
  }

 private:
  ErrorCode _errorNumber;
  std::string _errorMessage;
};

}  // namespace arangodb::result

#endif  // ARANGODB_BASICS_RESULT_ERROR_H
