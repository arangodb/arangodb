////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright $YEAR-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <function2.hpp>

namespace arangodb {

struct DeferredAction {
  DeferredAction() = default;
  DeferredAction(DeferredAction&&) noexcept = default;
  DeferredAction& operator=(DeferredAction&&) noexcept = default;
  DeferredAction(DeferredAction const&) noexcept = delete;
  DeferredAction& operator=(DeferredAction const&) noexcept = delete;

  ~DeferredAction() {
    fire();
  }

  void fire() noexcept {
    if (!_function.empty()) {
      _function.operator()();
    }
  }

  using function_object = fu2::unique_function<void() noexcept>;

  template<typename F>
  explicit DeferredAction(F&& fo) noexcept(std::is_nothrow_constructible_v<function_object, F>)
      : _function(std::forward<F>(fo)) {}

 private:
  function_object _function;
};


}

