////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include <map>
#include <optional>
#include <mutex>
#include <variant>

#include "Futures/Future.h"

namespace arangodb::replication2::test {

template<std::totally_ordered IndexType, typename ResultType>
struct WaitForQueue {
  WaitForQueue() = default;

  static_assert(std::is_copy_constructible_v<ResultType>);

  auto waitFor(IndexType index) -> futures::Future<ResultType> {
    std::unique_lock guard(mutex);
    if (index <= resolvedIndex) {
      TRI_ASSERT(lastResult.has_value());
      return futures::Future<ResultType>{std::in_place, *lastResult};
    }

    return queue.emplace(index)->second.getFuture();
  }

  auto resolve(IndexType upTo, ResultType withValue) {
    auto resolveSet = QueueType{};
    {
      std::unique_lock guard(mutex);
      for (auto it = queue.begin(); it != queue.end();) {
        if (it->first > upTo) {
          break;
        }
        auto node = queue.extract(it++);
        resolveSet.insert(node);
      }

      resolvedIndex = upTo;
      lastResult = withValue;
    }

    for (auto [index, promise] : resolveSet) {
      promise.setValue(withValue);
    }
  }

  auto resolveAll(ResultType withValue) {
    auto resolveSet = QueueType{};
    {
      std::unique_lock guard(mutex);
      std::swap(resolveSet, queue);
    }
    for (auto [index, promise] : resolveSet) {
      promise.setValue(withValue);
    }
  }

 private:
  using QueueType = std::multimap<IndexType, futures::Promise<ResultType>>;

  std::mutex mutex;
  std::optional<IndexType> resolvedIndex;
  std::optional<ResultType> lastResult;
  QueueType queue;
};

template<typename T>
using SimpleWaitForQueue = WaitForQueue<std::monostate, T>;

}  // namespace arangodb::replication2::test
