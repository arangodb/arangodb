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

  struct ScopedCurrentActivity;

  static auto currentActivity() noexcept -> ActivityId {
    return _currentActivity;
  }
  static auto setCurrentActivity(ActivityId activity) noexcept -> void {
    _currentActivity = std::move(activity);
  }

 private:
  static thread_local ActivityId _currentActivity;
};

struct Registry::ScopedCurrentActivity {
  explicit ScopedCurrentActivity(ActivityId activity) noexcept;
  ~ScopedCurrentActivity();

 private:
  ActivityId _oldActivity;
};

template<typename Func>
auto withCurrentActivity(ActivityId activity, Func&& func) {
  return [func = std::forward<Func>(func),
          activity]<typename... Args,
                    typename =
                        std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
             Args&&... args) mutable {
    Registry::ScopedCurrentActivity guard(activity);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

template<typename Func>
auto withCurrentActivity(Func&& func) {
  return [func = std::forward<Func>(func),
          currentActivity = Registry::currentActivity()]<
             typename... Args,
             typename = std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
             Args&&... args) mutable {
    Registry::ScopedCurrentActivity guard(currentActivity);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

}  // namespace arangodb::activity_registry
