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

#include "Replication2/ReplicatedLog/LogCommon.h"

#include "Futures/Promise.h"

#include <map>

namespace arangodb::futures {
template<typename T>
class Future;
template<typename T>
class Try;
}  // namespace arangodb::futures

namespace arangodb::replication2 {

struct WaitForQueue {
  using WaitForPromise = futures::Promise<LogIndex>;
  using WaitForFuture = futures::Future<LogIndex>;

 private:
  std::multimap<LogIndex, WaitForPromise> _queue;

 public:
  WaitForQueue() = default;
  WaitForQueue(WaitForQueue const&) = delete;
  WaitForQueue(WaitForQueue&&) = default;
  ~WaitForQueue();

  [[nodiscard]] auto waitFor(LogIndex) -> WaitForFuture;
  [[nodiscard]] auto splitLowerThan(LogIndex) noexcept -> WaitForQueue;
  template<typename F>
  void resolveAllWith(futures::Try<LogIndex> result, F&& run) noexcept;
};

template<typename F>
void WaitForQueue::resolveAllWith(futures::Try<LogIndex> result,
                                  F&& run) noexcept {
  // TODO Add some logging in case of exceptions before aborting.
  //      However, we can't continue if we fail to resolve a promise.
  for (auto& it : _queue) {
    // TODO enable log message
    // LOG_CTX("37d9d", TRACE, _self._logContext)
    //     << "resolving promise for index " << it->first;
    run([promise = std::move(it.second), result]() mutable noexcept {
      promise.setTry(std::move(result));
    });
  }
  _queue.clear();
}

}  // namespace arangodb::replication2
