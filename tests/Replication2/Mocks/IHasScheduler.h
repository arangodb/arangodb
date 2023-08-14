////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "Basics/overload.h"

namespace arangodb::replication2::test {

template<class T>
concept PointerLike = requires {
  typename std::pointer_traits<T>::pointer;
};

template<class T>
concept NotPointerLike = not PointerLike<T>;

static_assert(PointerLike<int*>);
static_assert(PointerLike<int const*>);
static_assert(PointerLike<std::shared_ptr<int>>);
static_assert(PointerLike<std::shared_ptr<int const>>);
static_assert(NotPointerLike<int>);
static_assert(NotPointerLike<int const>);
static_assert(NotPointerLike<int&>);
static_assert(NotPointerLike<int const&>);

struct IHasScheduler;

template<class T>
concept HasScheduler = std::derived_from<T, IHasScheduler> or
    std::derived_from<typename std::pointer_traits<T>::element_type,
                      IHasScheduler>;

struct IHasScheduler {
  virtual ~IHasScheduler() = default;

  // // run exactly one task, crash of there is none.
  // virtual void runOnce() noexcept = 0;
  //
  // // run all tasks currently in the queue, but nothing that is added
  // meanwhile. virtual auto runAllCurrent() noexcept -> std::size_t = 0;

  virtual auto hasWork() const noexcept -> bool = 0;

  virtual auto runAll() noexcept -> std::size_t = 0;

  // convenience function: run all tasks in all passed schedulers, until there
  // are no more tasks in any of them.
  // Schedulers can be passed by reference, pointer, or shared_ptr. nullptrs
  // will be ignored.
  static auto runAll(HasScheduler auto&... schedulersArg) {
    auto schedulers = {schedulerTypeToPointer(schedulersArg)...};
    auto count = std::size_t{0};
    while (std::any_of(schedulers.begin(), schedulers.end(),
                       [](auto const& scheduler) {
                         return scheduler != nullptr && scheduler->hasWork();
                       })) {
      for (auto& scheduler : schedulers) {
        if (scheduler != nullptr) {
          count += scheduler->runAll();
        }
      }
    }
    return count;
  }

  static auto hasWork(HasScheduler auto&... schedulersArg) -> bool {
    auto schedulers = {schedulerTypeToPointer(schedulersArg)...};
    return std::any_of(schedulers.begin(), schedulers.end(),
                       [](auto const& scheduler) {
                         return scheduler != nullptr && scheduler->hasWork();
                       });
  }

  template<typename T>
  requires requires(T ts) {
    ts.begin();
    ts.end();
    requires std::derived_from<typename T::value_type, IHasScheduler>;
  }
  static auto runAll(T& schedulers) -> std::size_t {
    for (auto& scheduler : schedulers) {
      scheduler.runAll();
    }
  }
  template<typename T>
  requires requires(T ts) {
    ts.begin();
    ts.end();
    requires std::derived_from<
        typename std::pointer_traits<typename T::value_type>::element_type,
        IHasScheduler>;
  }
  static auto runAll(T& schedulers) -> std::size_t {
    auto count = std::size_t{0};
    for (auto& scheduler : schedulers) {
      count += schedulerTypeToPointer(scheduler)->runAll();
    }
    return count;
  }

  template<typename T>
  requires requires(T ts) {
    ts.begin();
    ts.end();
    requires std::derived_from<typename T::value_type, IHasScheduler>;
  }
  static auto hasWork(T& schedulers) -> std::size_t {
    return std::any_of(
        schedulers.begin(), schedulers.end(),
        [](auto const& scheduler) { return scheduler.hasWork(); });
  }
  template<typename T>
  requires requires(T ts) {
    ts.begin();
    ts.end();
    requires std::derived_from<
        typename std::pointer_traits<typename T::value_type>::element_type,
        IHasScheduler>;
  }
  static auto hasWork(T& schedulers) -> std::size_t {
    return std::any_of(schedulers.begin(), schedulers.end(),
                       [](auto const& scheduler) {
                         return schedulerTypeToPointer(scheduler)->hasWork();
                       });
  }

  static constexpr auto schedulerTypeToPointer = overload{
      []<PointerLike T>(T scheduler) {
        using R = std::conditional_t<
            std::is_const_v<typename std::pointer_traits<T>::element_type>,
            IHasScheduler const, IHasScheduler>;
        auto p = std::to_address(scheduler);
        auto* ischeduler = dynamic_cast<R*>(p);
        TRI_ASSERT((ischeduler == nullptr) == (scheduler == nullptr));
        return ischeduler;
      },
      []<typename T>(std::reference_wrapper<T>& scheduler) {
        using R = std::conditional_t<std::is_const_v<T>, IHasScheduler const,
                                     IHasScheduler>;
        auto* ischeduler = dynamic_cast<R*>(&scheduler.get());
        TRI_ASSERT(ischeduler != nullptr);
        return ischeduler;
      },
      []<typename T>(std::reference_wrapper<T> const& scheduler) {
        using R = std::conditional_t<std::is_const_v<T>, IHasScheduler const,
                                     IHasScheduler>;
        auto* ischeduler = dynamic_cast<R*>(&scheduler.get());
        TRI_ASSERT(ischeduler != nullptr);
        return ischeduler;
      },
      []<NotPointerLike T>(T& scheduler) {
        using R = std::conditional_t<std::is_const_v<T>, IHasScheduler const,
                                     IHasScheduler>;
        auto* ischeduler = dynamic_cast<R*>(&scheduler);
        TRI_ASSERT(ischeduler != nullptr);
        return ischeduler;
      },
  };
};

}  // namespace arangodb::replication2::test
