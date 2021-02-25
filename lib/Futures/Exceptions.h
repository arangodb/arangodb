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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_FUTURES_EXCEPTIONS_H
#define ARANGODB_FUTURES_EXCEPTIONS_H 1

#include <cstdint>
#include <exception>

namespace arangodb {
namespace futures {

enum class ErrorCode : uint8_t {
  BrokenPromise = 1,
  FutureAlreadyRetrieved = 2,
  FutureNotReady = 3,
  PromiseAlreadySatisfied = 4,
  NoState = 5
};

struct FutureException : public std::exception {
  explicit FutureException(ErrorCode code) : _code(code) {}

  ErrorCode code() const noexcept { return _code; }

  char const* what() const noexcept override {
    switch (_code) {
      case ErrorCode::BrokenPromise:
        return "Promise abandoned the shared state";
      case ErrorCode::FutureAlreadyRetrieved:
        return "Future was already retrieved";
      case ErrorCode::FutureNotReady:
        return "Future is not ready";
      case ErrorCode::PromiseAlreadySatisfied:
        return "Promise was already satisfied";
      case ErrorCode::NoState:
        return "No shared state";
      default:
        return "invalid future exception";
    }
  }

 private:
  ErrorCode _code;
};

}  // namespace futures
}  // namespace arangodb

#endif
