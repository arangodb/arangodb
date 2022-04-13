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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>

#include "Basics/debugging.h"

namespace arangodb::inspection {
struct Status {
  // Type that can be returned by a function instead of a `Status`
  // to indicate that this function cannot fail.
  struct Success {
    constexpr bool ok() const noexcept { return true; }
  };

  Status() = default;

  Status(Success) : Status() {}

  Status(std::string error)
      : _error(std::make_unique<Error>(std::move(error))) {}

  bool ok() const noexcept { return _error == nullptr; }

  std::string const& error() const noexcept {
    TRI_ASSERT(!ok());
    return _error->message;
  }

  std::string const& path() const noexcept {
    TRI_ASSERT(!ok());
    return _error->path;
  }

  struct AttributeTag {};
  struct ArrayTag {};

  Status(Status&& res, std::string const& index, ArrayTag)
      : Status(std::move(res)) {
    prependPath("[" + index + "]");
  }

  Status(Status&& res, std::string_view attribute, AttributeTag)
      : Status(std::move(res)) {
    if (attribute.find('.') != std::string::npos) {
      prependPath("['" + std::string(attribute) + "']");
    } else {
      prependPath(std::string(attribute));
    }
  }

 private:
  void prependPath(std::string const& s) {
    TRI_ASSERT(!ok());
    if (_error->path.empty()) {
      _error->path = s;
    } else {
      if (_error->path[0] != '[') {
        _error->path = "." + _error->path;
      }
      _error->path = s + _error->path;
    }
  }

  struct Error {
    explicit Error(std::string&& msg) : message(std::move(msg)) {}
    std::string message;
    std::string path;
  };
  std::unique_ptr<Error> _error;
};

constexpr inline bool isSuccess(Status const&) { return false; }
constexpr inline bool isSuccess(Status::Success const&) { return true; }

template<class Func>
Status operator|(Status&& res, Func&& func) {
  if (!res.ok()) {
    return std::move(res);
  }
  return func();
}

template<class Func>
auto operator|(Status::Success&&, Func&& func) {
  return func();
}

}  // namespace arangodb::inspection
