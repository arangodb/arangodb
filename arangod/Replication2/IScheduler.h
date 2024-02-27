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

#include <chrono>
#include <function2.hpp>
#include "Futures/Future.h"

namespace arangodb::replication2 {
struct IScheduler {
  virtual ~IScheduler() = default;
  struct WorkItem {
    virtual ~WorkItem() = default;
  };
  using WorkItemHandle = std::shared_ptr<WorkItem>;

  virtual auto delayedFuture(std::chrono::nanoseconds duration,
                             std::string_view name = {})
      -> arangodb::futures::Future<arangodb::futures::Unit> = 0;

  virtual auto queueDelayed(
      std::string_view name, std::chrono::nanoseconds delay,
      fu2::unique_function<void(bool canceled)>
          handler) noexcept(true)  // otherwise cppcheck chokes on this line
                                   // 04/2023
      -> WorkItemHandle = 0;
  virtual void queue(fu2::unique_function<void()>) noexcept = 0;
};
}  // namespace arangodb::replication2
