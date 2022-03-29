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
struct Result {
  // Type that can be returned by a function instead of a Results
  // to indicate that this function cannot fail.
  struct Success {
    constexpr bool ok() const noexcept { return true; }
    constexpr bool canFail() const noexcept { return false; }
  };

  Result() = default;

  Result(Success) : Result() {}

  Result(std::string error)
      : _error(std::make_unique<Error>(std::move(error))) {}

  bool ok() const noexcept { return _error == nullptr; }
  constexpr bool canFail() const noexcept { return true; }

  std::string const& error() const noexcept {
    TRI_ASSERT(!ok());
    return _error->message;
  }

  std::string const& path() const noexcept {
    TRI_ASSERT(!ok());
    return _error->path;
  }

 private:
  template<class T>
  friend struct InspectorBase;
  friend struct VPackLoadInspector;
  friend struct VPackSaveInspector;

  struct AttributeTag {};
  struct ArrayTag {};

  Result(Result&& res, std::string const& index, ArrayTag)
      : Result(std::move(res)) {
    prependPath("[" + index + "]");
  }

  Result(Result&& res, std::string_view attribute, AttributeTag)
      : Result(std::move(res)) {
    if (attribute.find('.') != std::string::npos) {
      prependPath("['" + std::string(attribute) + "']");
    } else {
      prependPath(std::string(attribute));
    }
  }

  void prependPath(std::string const& s) {
    assert(!ok());
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

template<class Func>
Result operator|(Result&& res, Func&& func) {
  if (!res.ok()) {
    return std::move(res);
  }
  return func();
}

template<class Func>
auto operator|(Result::Success&&, Func&& func) {
  return func();
}

}  // namespace arangodb::inspection
