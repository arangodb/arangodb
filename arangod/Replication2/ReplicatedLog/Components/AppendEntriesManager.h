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
#include <optional>
#include "Basics/Guarded.h"
#include "Replication2/ReplicatedLog/Components/ExclusiveBool.h"
#include "Replication2/ReplicatedLog/Components/IAppendEntriesManager.h"
#include "Replication2/ReplicatedLog/Components/IMessageIdManager.h"
#include "Replication2/ReplicatedLog/Components/TermInformation.h"
#include "IFollowerCommitManager.h"
#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2::replicated_log {
inline namespace comp {

struct IStorageManager;
struct ISnapshotManager;
struct ICompactionManager;
struct IStateHandleManager;
struct IMessageIdManager;

struct AppendEntriesManager
    : IAppendEntriesManager,
      std::enable_shared_from_this<AppendEntriesManager> {
  AppendEntriesManager(std::shared_ptr<FollowerTermInformation const> termInfo,
                       IStorageManager& storage, ISnapshotManager& snapshot,
                       ICompactionManager& compaction,
                       IStateHandleManager& stateHandle,
                       IMessageIdManager& messageIdManager,
                       std::shared_ptr<ReplicatedLogMetrics> metrics,
                       LoggerContext const& loggerContext);

  auto appendEntries(AppendEntriesRequest request)
      -> futures::Future<AppendEntriesResult> override;

  auto resign() && noexcept -> void override;

  struct GuardedData {
    GuardedData(IStorageManager& storage, ISnapshotManager& snapshot,
                ICompactionManager& compaction,
                IStateHandleManager& stateHandle,
                IMessageIdManager& messageIdManager);
    auto preflightChecks(AppendEntriesResult::Params const& params,
                         AppendEntriesRequest const& request,
                         FollowerTermInformation const&,
                         LoggerContext const& lctx)
        -> std::optional<AppendEntriesResult>;
    auto resign() && noexcept -> void;

    bool resigned = false;
    ExclusiveBool requestInFlight;

    IStorageManager& storage;
    ISnapshotManager& snapshot;
    ICompactionManager& compaction;
    IStateHandleManager& stateHandle;
    IMessageIdManager& messageIdManager;
  };

  LoggerContext const loggerContext;
  std::shared_ptr<FollowerTermInformation const> const termInfo;
  std::shared_ptr<ReplicatedLogMetrics> const metrics;
  Guarded<GuardedData> guarded;
};
}  // namespace comp
}  // namespace arangodb::replication2::replicated_log
