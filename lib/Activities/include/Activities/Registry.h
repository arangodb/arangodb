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
#include "Activities/IRegistryMetrics.h"
#include "Containers/Concurrent/metrics.h"

#include "Basics/ErrorT.h"
#include "Basics/Guarded.h"

#include "Inspection/Status.h"

#include <velocypack/SharedSlice.h>

#include <deque>
#include <memory>

namespace arangodb::activities {

struct Registry {
  explicit Registry() = default;
  Registry(Registry const &) = delete;
  Registry(Registry &&) = delete;
  auto operator=(Registry const &) = delete;
  auto operator=(Registry &&) = delete;

  struct [[nodiscard]] ScopedCurrentlyExecutingActivity;

  auto setMetrics(std::shared_ptr<IRegistryMetrics> metrics) -> void;
  auto garbageCollect() -> void;

  static auto currentlyExecutingActivity() noexcept -> ActivityHandle {
    return _currentlyExecutingActivity;
  }
  static auto setCurrentlyExecutingActivity(ActivityHandle activity) noexcept
      -> void {
    _currentlyExecutingActivity = std::move(activity);
  }

  template <typename T, typename... Args>
  auto makeActivityWithParent(ActivityHandle parent, Args &&...args)
      -> T::HandleType {
    auto id = _activityIdCounter.fetch_add(1);
    auto activity =
        std::make_shared<T>(id, std::move(parent), std::forward<Args>(args)...);

    _registry.doUnderLock([this, &activity](auto &&reg) {
      reg.emplace_front(activity);
      _metrics->increment_total_nodes();
      _metrics->increment_registered_nodes();
    });

    return activity;
  }
  template <typename T, typename... Args>
  auto makeActivity(Args &&...args) -> T::HandleType {
    return makeActivityWithParent<T>(_currentlyExecutingActivity,
                                     std::forward<Args>(args)...);
  }

  auto snapshot()
      -> errors::ErrorT<inspection::Status, velocypack::SharedSlice>;

  auto findActivityById(ActivityId id) const -> std::optional<ActivityHandle>;

private:
  auto increment_total_nodes() -> void;
  auto increment_registered_nodes() -> void;
  auto store_registered_nodes(std::uint64_t count) -> void;

  static thread_local ActivityHandle _currentlyExecutingActivity;
  Guarded<std::deque<ActivityHandle>> _registry;
  std::atomic<ActivityId> _activityIdCounter;
  std::shared_ptr<IRegistryMetrics> _metrics;
};

struct [[nodiscard]] Registry::ScopedCurrentlyExecutingActivity {
  explicit ScopedCurrentlyExecutingActivity(ActivityHandle activity) noexcept;
  ~ScopedCurrentlyExecutingActivity();

  ScopedCurrentlyExecutingActivity(ScopedCurrentlyExecutingActivity const &) =
      delete;
  ScopedCurrentlyExecutingActivity(ScopedCurrentlyExecutingActivity &&) =
      delete;
  auto operator=(ScopedCurrentlyExecutingActivity const &) = delete;
  auto operator=(ScopedCurrentlyExecutingActivity &&) = delete;

private:
  ActivityHandle _oldExecutingActivity;
};

template <typename Func>
auto withSetCurrentlyExecutingActivity(ActivityHandle activity, Func &&func) {
  return [
    func = std::forward<Func>(func), activity
  ]<typename... Args,
    typename = std::enable_if_t<std::is_invocable_v<Func, Args...>>>(
      Args && ...args) mutable {
    Registry::ScopedCurrentlyExecutingActivity guard(activity);
    return std::forward<Func>(func)(std::forward<Args>(args)...);
  };
}

template <typename Func> auto withCurrentlyExecutingActivity(Func &&func) {
  return withSetCurrentlyExecutingActivity(
      Registry::currentlyExecutingActivity(), std::forward<Func>(func));
}

} // namespace arangodb::activities
