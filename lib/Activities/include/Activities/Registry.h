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
#include "Registry.h"

#include <velocypack/SharedSlice.h>

#include <deque>
#include <memory>

namespace arangodb::activities {

// We need a wrapper because the concurrent-registry needs a compile-time
// constant item type but our activities can have different types (all
// inheriting from Activity)
struct ActivityPtr {
  // either this (ownership in ActivityOwner and children)
  std::weak_ptr<Activity> item;
  // or this (after marked_for_deletion)
  std::unique_ptr<Activity> owned;

  using Snapshot = Activity::Snapshot;

  auto snapshot() -> Snapshot {
    return item.lock()->snapshot();
  }
};

using ThreadRegistry = containers::ThreadRegistry<ActivityPtr>;

template<typename T>
struct ActivityOwner;

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
    auto id = _activityIdCounter.fetch_add(1);
    auto deleter =
        std::make_shared<std::function<void(T*)>>([](T* ptr) { delete ptr; });
    // The activity owner creates and owns the shared_ptr to the activity.
    // We add a custom (at this point standard) deleter.
    auto h = std::shared_ptr<T>(
        new T{id, std::move(parent), std::forward<Args>(args)...},
        [deleter](T* ptr) { (*deleter)(ptr); });
    // We add an ActivityPtr (with a weak_ptr to the activity) to the registry.
    auto node = this->get_thread_registry().add(
        [&]() { return ActivityPtr{.item = h}; });
    // Now we can properly set the deleter: when the shared_ptr of activity goes
    // out of scope, the node continues to own the activity and the node is
    // marked for deletion. This way, the activity is deleted when the node is
    // deleted.
    *deleter = [node](T* ptr) {
      node->data.owned = std::unique_ptr<Activity>(ptr);
      node->list->mark_for_deletion(node);
    };
    return h;
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
