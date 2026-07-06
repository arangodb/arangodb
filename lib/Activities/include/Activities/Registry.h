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

#include "Activities/ActivityHandle.h"
#include "Activities/ActivityId.h"
#include "Containers/Concurrent/metrics.h"
#include "Activities/Activity.h"

#include "Basics/ErrorT.h"
#include "Basics/Guarded.h"
#include "Containers/Concurrent/Registry.h"

#include "Inspection/Status.h"
#include "Containers/Concurrent/ThreadOwnedList.h"

#include <velocypack/SharedSlice.h>

#include <deque>
#include <memory>

namespace arangodb::activities {

using ThreadRegistry = containers::ThreadRegistry<ActivityPtr>;

struct Registry : containers::Registry<ActivityPtr> {
  auto get_thread_registry() noexcept -> ThreadRegistry& {
    struct Guard {
      explicit Guard(Registry& registry)
          : _self{registry},
            _registry{ThreadRegistry::make(registry.get_metrics())} {
        registry.add(_registry);
      }

      Registry& _self;
      std::shared_ptr<ThreadRegistry> _registry;
    };
    static thread_local auto guard = Guard{*this};
    return *guard._registry;
  }

  template<typename T, typename... Args>
  auto makeActivityWithParent(ActivityHandle parent, Args&&... args) ->
      typename T::HandleType {
    auto const id = _activityIdCounter.fetch_add(1);
    // create the node - which is a specific activity of type T at the same time
    auto node =
        std::make_unique<T>(id, std::move(parent), std::forward<Args>(args)...);
    auto* const rawNode = node.get();
    this->get_thread_registry().add(std::move(node));
    // The deleter must not free the activity itself, since the registry owns
    // it, but marks it for deletion. Then it can be deleted in a gc-run.
    return std::shared_ptr<T>(rawNode,
                              [](T* item) { item->mark_for_deletion(); });
  }
  template<typename T, typename... Args>
  auto makeActivity(Args&&... args) -> typename T::HandleType {
    return makeActivityWithParent<T>(_currentlyExecutingActivity,
                                     std::forward<Args>(args)...);
  }

  struct [[nodiscard]] ScopedCurrentlyExecutingActivity;
  static auto currentlyExecutingActivity() noexcept -> ActivityHandle {
    return _currentlyExecutingActivity;
  }
  static auto setCurrentlyExecutingActivity(ActivityHandle activity) noexcept
      -> void {
    _currentlyExecutingActivity = std::move(activity);
  }

  auto snapshot()
      -> errors::ErrorT<inspection::Status, velocypack::SharedSlice>;

 private:
  static thread_local ActivityHandle _currentlyExecutingActivity;
  std::atomic<ActivityId> _activityIdCounter{0};
};

struct [[nodiscard]] Registry::ScopedCurrentlyExecutingActivity {
  explicit ScopedCurrentlyExecutingActivity(ActivityHandle activity) noexcept;
  ~ScopedCurrentlyExecutingActivity();

  ScopedCurrentlyExecutingActivity(ScopedCurrentlyExecutingActivity const&) =
      delete;
  ScopedCurrentlyExecutingActivity(ScopedCurrentlyExecutingActivity&&) = delete;
  auto operator=(ScopedCurrentlyExecutingActivity const&) = delete;
  auto operator=(ScopedCurrentlyExecutingActivity&&) = delete;

 private:
  ActivityHandle _oldExecutingActivity;
};

template<typename Func>
auto withCurrentlyExecutingActivity(Func&& func) {
  return [
    func = std::forward<Func>(func),
    activity = Registry::currentlyExecutingActivity()
  ]<typename... Args,
    typename = std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
      Args && ... args) mutable {
    Registry::ScopedCurrentlyExecutingActivity guard(activity);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

}  // namespace arangodb::activities
