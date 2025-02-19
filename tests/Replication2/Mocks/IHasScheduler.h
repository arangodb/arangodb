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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "Basics/overload.h"

namespace arangodb::replication2::test {

// TODO Remove this conditional as soon as we've upgraded MSVC.
#if defined(_MSC_VER) && _MSC_VER <= 1935
#pragma message(                                                               \
    "MSVC <= 19.35 has a compiler bug, failing to compile the concept below. " \
    "Please use a more recent compiler version, or don't include this file. "  \
    "See https://godbolt.org/z/8M1hbffEx for a minimal example.")
#define DISABLE_I_HAS_SCHEDULER
#else

template<class T>
concept PointerLike = requires {
  // gcc-11's STL has ::pointer defined even for non-pointers (e.g.,
  // std::pointer_traits<int>::pointer is int). So for now use element_type
  // instead, which works as intended.
  typename std::pointer_traits<T>::element_type;
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

#endif

template<class T>
concept Iterable = requires(T t) {
  t.begin();
  t.end();
};

struct IHasScheduler;

template<class T>
concept HasScheduler = std::derived_from<T, IHasScheduler> or
    std::derived_from<typename std::pointer_traits<T>::element_type,
                      IHasScheduler>;

struct IHasScheduler {
  virtual ~IHasScheduler() = default;

  // // run exactly one task, crash if there is none.
  // virtual void runOnce() noexcept = 0;
  //
  // // run all tasks currently in the queue, but nothing that is added
  // meanwhile. virtual auto runAllCurrent() noexcept -> std::size_t = 0;

  virtual auto hasWork() const noexcept -> bool = 0;

  virtual auto runAll() noexcept -> std::size_t = 0;

  // TODO Remove this conditional as soon as we've upgraded MSVC.
#ifndef DISABLE_I_HAS_SCHEDULER
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

  static auto runAll(Iterable auto& schedulers) -> std::size_t {
    auto count = std::size_t{0};
    for (auto& scheduler : schedulers) {
      count += schedulerTypeToPointer(scheduler)->runAll();
    }
    return count;
  }

  static auto hasWork(Iterable auto& schedulers) -> std::size_t {
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
#endif
};

}  // namespace arangodb::replication2::test
