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

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogFollower.h"

namespace arangodb::replication2::test {
/**
 * Resolves promises in a separate thread.
 */

struct AsyncLeader : replicated_log::ILogLeader,
                     std::enable_shared_from_this<AsyncLeader> {
  explicit AsyncLeader(std::shared_ptr<replicated_log::ILogLeader> leader);
  ~AsyncLeader();
  [[nodiscard]] auto getStatus() const -> replicated_log::LogStatus override;
  [[nodiscard]] auto getQuickStatus() const
      -> replicated_log::QuickLogStatus override;
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>,
                                 DeferredAction> override;
  auto waitFor(LogIndex index) -> WaitForFuture override;
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;
  [[nodiscard]] auto getCommitIndex() const noexcept -> LogIndex override;
  auto release(LogIndex doneWithIdx) -> Result override;
  auto insert(LogPayload payload, bool waitForSync) -> LogIndex override;
  auto insert(LogPayload payload, bool waitForSync,
              DoNotTriggerAsyncReplication replication) -> LogIndex override;
  void triggerAsyncReplication() override;
  [[nodiscard]] auto isLeadershipEstablished() const noexcept -> bool override;
  auto waitForLeadership() -> WaitForFuture override;
  [[nodiscard]] auto copyInMemoryLog() const
      -> replicated_log::InMemoryLog override;

  void stop() noexcept;

 private:
  template<typename T>
  auto resolveFutureAsync(futures::Future<T> f) -> futures::Future<T>;
  template<typename T>
  void resolvePromiseAsync(futures::Promise<T>, futures::Try<T>) noexcept;

  void runWorker();

  struct Action {
    virtual ~Action() = default;
    virtual void execute() noexcept = 0;
  };

  std::mutex _mutex;
  std::condition_variable _cv;
  std::vector<std::unique_ptr<Action>> _queue;
  bool _stopping{false};

  std::shared_ptr<replicated_log::ILogLeader> _leader;
  std::thread _asyncResolver;
};
}  // namespace arangodb::replication2::test
