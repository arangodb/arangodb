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

struct IDelayedScheduler {
  virtual ~IDelayedScheduler() = default;

  // run exactly one task, crash of there is none.
  virtual void runOnce() noexcept = 0;

  // run all tasks currently in the queue, but nothing that is added meanwhile.
  virtual auto runAllCurrent() noexcept -> std::size_t = 0;

  virtual auto hasWork() const noexcept -> bool = 0;

  auto runAll() noexcept -> std::size_t {
    auto count = std::size_t{0};
    while (hasWork()) {
      runOnce();
      ++count;
    }
    return count;
  }

  // convenience function: run all tasks in all passed schedulers, until there
  // are no more tasks in any of them.
  static auto runAll(auto&... schedulersArg) {
    auto schedulers = {schedulerTypeToPointer(schedulersArg)...};
    while (std::any_of(
        schedulers.begin(), schedulers.end(),
        [](auto const& scheduler) { return scheduler->hasWork(); })) {
      for (auto& scheduler : schedulers) {
        scheduler->runAll();
      }
    }
  }

  static auto hasWork(auto&... schedulersArg) -> bool {
    auto schedulers = {schedulerTypeToPointer(schedulersArg)...};
    return std::any_of(
        schedulers.begin(), schedulers.end(),
        [](auto const& scheduler) { return scheduler->hasWork(); });
  }

  static constexpr auto schedulerTypeToPointer = overload{
      [](auto* scheduler) {
        return dynamic_cast<IDelayedScheduler*>(scheduler);
      },
      []<typename T>(std::shared_ptr<T>& scheduler) -> IDelayedScheduler* {
        return dynamic_cast<IDelayedScheduler*>(scheduler.get());
      },
      [](auto& scheduler) -> IDelayedScheduler* { return &scheduler; },
  };
};

}  // namespace arangodb::replication2::test
