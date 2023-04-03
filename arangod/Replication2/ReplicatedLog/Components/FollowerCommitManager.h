////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Replication2/ReplicatedLog/Components/IFollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/IStateHandleManager.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Basics/Guarded.h"
#include "Replication2/LoggerContext.h"

namespace arangodb::replication2 {
struct IScheduler;
}
namespace arangodb::replication2::replicated_log {
inline namespace comp {

struct IStorageManager;

struct FollowerCommitManager
    : IFollowerCommitManager,
      std::enable_shared_from_this<FollowerCommitManager> {
  FollowerCommitManager(IStorageManager&, LoggerContext const& loggerContext,
                        std::shared_ptr<IScheduler> scheduler);
  [[nodiscard]] auto updateCommitIndex(LogIndex index) noexcept
      -> std::pair<std::optional<LogIndex>, DeferredAction> override;
  [[nodiscard]] auto getCommitIndex() const noexcept -> LogIndex override;

  [[nodiscard]] auto waitFor(LogIndex index) noexcept
      -> ILogParticipant::WaitForFuture override;
  [[nodiscard]] auto waitForIterator(LogIndex index) noexcept
      -> ILogParticipant::WaitForIteratorFuture override;

  void resign() noexcept;

 private:
  using ResolvePromise = futures::Promise<WaitForResult>;
  using ResolveFuture = futures::Future<WaitForResult>;
  using WaitForQueue = std::multimap<LogIndex, ResolvePromise>;

  struct GuardedData {
    explicit GuardedData(IStorageManager&);
    LogIndex commitIndex{0};
    LogIndex resolveIndex{0};
    WaitForQueue waitQueue;
    bool isResigned{false};

    IStorageManager& storage;
  };

  Guarded<GuardedData> guardedData;
  LoggerContext const loggerContext;
  std::shared_ptr<IScheduler> const scheduler;
};

}  // namespace comp

}  // namespace arangodb::replication2::replicated_log
