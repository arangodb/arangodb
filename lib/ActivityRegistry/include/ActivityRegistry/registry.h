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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ActivityRegistry/activity.h"
#include "Containers/Concurrent/Registry.h"
namespace arangodb::activity_registry {
using ThreadRegistry = containers::ThreadRegistry<ActivityInRegistry>;

struct Registry : containers::Registry<ActivityInRegistry> {
  explicit Registry() = default;
  Registry(Registry const&) = delete;
  Registry(Registry&&) = delete;
  auto operator=(Registry const&) = delete;
  auto operator=(Registry&&) = delete;

  static Parent& defaultParent() noexcept {
    return Registry::_currentDefaultParent;
  }

  struct ScopedDefaultParent;

  static auto currentParent() noexcept -> Parent {
    return _currentDefaultParent;
  }

 private:
  static thread_local Parent _currentDefaultParent;
};

struct Registry::ScopedDefaultParent {
  explicit ScopedDefaultParent(Parent parent) noexcept;
  ~ScopedDefaultParent();

 private:
  Parent _oldParent;
};

template<typename Func>
auto withParent(Func&& func) {
  return [func = std::forward<Func>(func), ctx = Registry::defaultParent()]<
             typename... Args,
             typename = std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
             Args&&... args) mutable {
    Registry::ScopedDefaultParent guard(ctx);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

}  // namespace arangodb::activity_registry
