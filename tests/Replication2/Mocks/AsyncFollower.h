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

#include "Replication2/ReplicatedLog/LogFollower.h"

namespace arangodb::replication2::test {

struct AsyncFollower : replicated_log::ILogFollower {
  explicit AsyncFollower(std::shared_ptr<replicated_log::LogFollower> follower);
  ~AsyncFollower() noexcept override;
  [[nodiscard]] auto getStatus() const -> replicated_log::LogStatus override;
  [[nodiscard]] auto getQuickStatus() const -> replicated_log::QuickLogStatus override;
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>, DeferredAction> override;
  auto waitFor(LogIndex index) -> WaitForFuture override;
  auto release(LogIndex doneWithIdx) -> Result override;
  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override;
  auto appendEntries(replicated_log::AppendEntriesRequest request)
      -> futures::Future<replicated_log::AppendEntriesResult> override;
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;
  [[nodiscard]] auto getCommitIndex() const noexcept -> LogIndex override;
  auto getLeader() const noexcept -> std::optional<ParticipantId> const& override;
  void stop() noexcept;

  auto waitForLeaderAcked() -> WaitForFuture override;

 private:
  void runWorker();

  struct AsyncRequest {
    AsyncRequest(replicated_log::AppendEntriesRequest  request);
    replicated_log::AppendEntriesRequest request;
    futures::Promise<replicated_log::AppendEntriesResult> promise;
  };

  std::mutex _mutex;
  std::condition_variable _cv;
  std::vector<AsyncRequest> _requests;
  std::shared_ptr<replicated_log::LogFollower> const _follower;
  bool _stopping{false};

  std::thread _asyncWorker;
};

}  // namespace arangodb::replication2::test
