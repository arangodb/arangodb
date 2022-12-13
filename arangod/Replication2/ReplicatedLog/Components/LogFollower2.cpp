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

#include "LogFollower2.h"
#include "Logger/LogContextKeys.h"

#include "Metrics/Gauge.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Replication2/ReplicatedLog/Components/SnapshotManager.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedLog/Components/CompactionManager.h"
#include "Replication2/ReplicatedLog/Components/FollowerCommitManager.h"
#include "Replication2/ReplicatedLog/Components/AppendEntriesManager.h"
#include "Replication2/ReplicatedLog/Components/StateHandleManager.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_log::refactor;

struct refactor::MethodsProvider : IReplicatedLogFollowerMethods {
  explicit MethodsProvider(FollowerManager& fm) : follower(fm) {}
  auto releaseIndex(LogIndex index) -> void override {
    follower.compaction->updateReleaseIndex(index);
  }

  auto getLogSnapshot() -> InMemoryLog override {
    return follower.storage->getCommittedLog();
  }

  auto waitFor(LogIndex index) -> ILogParticipant::WaitForFuture override {
    return follower.commit->waitFor(index);
  }

  auto waitForIterator(LogIndex index)
      -> ILogParticipant::WaitForIteratorFuture override {
    return follower.commit->waitForIterator(index);
  }

  auto snapshotCompleted() -> Result override {
    return follower.snapshot->setSnapshotStateAvailable(
        follower.appendEntriesManager->getLastReceivedMessageId());
  }

 private:
  FollowerManager& follower;
};

namespace {

auto deriveLoggerContext(FollowerTermInformation const& info,
                         LoggerContext inCtx = LoggerContext{
                             Logger::REPLICATION2}) -> LoggerContext {
  return inCtx.with<logContextKeyStateRole>("follower")
      .with<logContextKeyTerm>(info.term)
      .with<logContextKeyLeaderId>(info.leader.value_or("<none>"));
}

}  // namespace

FollowerManager::FollowerManager(
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
    std::unique_ptr<IReplicatedStateHandle> stateHandlePtr,
    std::shared_ptr<FollowerTermInformation const> termInfo,
    std::shared_ptr<ReplicatedLogGlobalSettings const> options,
    std::shared_ptr<ReplicatedLogMetrics> metrics,
    std::shared_ptr<ILeaderCommunicator> leaderComm)
    : loggerContext(deriveLoggerContext(*termInfo)),
      options(options),
      metrics(metrics),
      storage(
          std::make_shared<StorageManager>(std::move(methods), loggerContext)),
      compaction(std::make_shared<CompactionManager>(*storage, options,
                                                     loggerContext)),
      stateHandle(
          std::make_shared<StateHandleManager>(std::move(stateHandlePtr))),
      snapshot(std::make_shared<SnapshotManager>(
          *storage, *stateHandle, termInfo, leaderComm, loggerContext)),
      commit(std::make_shared<FollowerCommitManager>(*storage, *stateHandle,
                                                     loggerContext)),
      appendEntriesManager(std::make_shared<AppendEntriesManager>(
          termInfo, *storage, *snapshot, *compaction, *commit, metrics,
          loggerContext)),
      termInfo(termInfo) {
  metrics->replicatedLogFollowerNumber->operator++();
  auto provider = std::make_unique<MethodsProvider>(*this);
  stateHandle->becomeFollower(std::move(provider));
}

FollowerManager::~FollowerManager() {
  metrics->replicatedLogFollowerNumber->operator--();
}

auto FollowerManager::getStatus() const -> LogStatus {
  auto commitIndex = commit->getCommitIndex();
  auto log = storage->getCommittedLog();
  auto [releaseIndex, lowestIndexToKeep] = compaction->getIndexes();
  return LogStatus{FollowerStatus{
      .local =
          LogStatistics{
              .spearHead = log.getLastTermIndexPair(),
              .commitIndex = commitIndex,
              .firstIndex = log.getFirstIndex(),
              .releaseIndex = releaseIndex,
          },
      .leader = termInfo->leader,
      .term = termInfo->term,
      .lowestIndexToKeep = lowestIndexToKeep,
      .compactionStatus = compaction->getCompactionStatus(),
      .snapshotAvailable =
          snapshot->checkSnapshotState() == SnapshotState::AVAILABLE,
  }};
}

auto FollowerManager::getQuickStatus() const -> QuickLogStatus {
  auto commitIndex = commit->getCommitIndex();
  auto log = storage->getCommittedLog();
  auto [releaseIndex, largestIndexToKeep] = compaction->getIndexes();
  return QuickLogStatus{
      .role = ParticipantRole::kFollower,
      .term = termInfo->term,
      .local =
          LogStatistics{
              .spearHead = log.getLastTermIndexPair(),
              .commitIndex = commitIndex,
              .firstIndex = log.getFirstIndex(),
              .releaseIndex = releaseIndex,
          },
      .leadershipEstablished = commitIndex.value > 0,
      .snapshotAvailable =
          snapshot->checkSnapshotState() == SnapshotState::AVAILABLE,
  };
}

auto FollowerManager::resign()
    -> std::tuple<std::unique_ptr<replicated_state::IStorageEngineMethods>,
                  std::unique_ptr<IReplicatedStateHandle>, DeferredAction> {
  // 1. resign the state and receive its handle
  auto handle = stateHandle->resign();
  // 2. resign the storage manager to receive the storage engine methods
  auto methods = storage->resign();
  // TODO resign CommitManager
  return std::make_tuple(std::move(methods), std::move(handle),
                         DeferredAction{});
}

auto FollowerManager::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  return appendEntriesManager->appendEntries(std::move(request));
}

auto LogFollowerImpl::getQuickStatus() const -> QuickLogStatus {
  return guarded.getLockedGuard()->getQuickStatus();
}

auto LogFollowerImpl::getStatus() const -> LogStatus {
  return guarded.getLockedGuard()->getStatus();
}

auto LogFollowerImpl::resign2() && -> std::tuple<
    std::unique_ptr<replicated_state::IStorageEngineMethods>,
    std::unique_ptr<IReplicatedStateHandle>, DeferredAction> {
  return guarded.getLockedGuard()->resign();
}

auto LogFollowerImpl::waitFor(LogIndex index)
    -> ILogParticipant::WaitForFuture {
  return guarded.getLockedGuard()->commit->waitFor(index);
}

auto LogFollowerImpl::waitForIterator(LogIndex index)
    -> ILogParticipant::WaitForIteratorFuture {
  return guarded.getLockedGuard()->commit->waitForIterator(index);
}

auto LogFollowerImpl::copyInMemoryLog() const -> InMemoryLog {
  return guarded.getLockedGuard()->storage->getCommittedLog();
}

auto LogFollowerImpl::release(LogIndex doneWithIdx) -> Result {
  guarded.getLockedGuard()->compaction->updateReleaseIndex(doneWithIdx);
  return {};
}

auto LogFollowerImpl::compact() -> ResultT<CompactionResult> {
  // TODO clean up CompacionResult vs ICompactionManager::CompactResult
  auto result = guarded.getLockedGuard()->compaction->compact().get();
  if (result.error) {
    return Result{result.error->errorNumber(), result.error->errorMessage()};
  }
  return CompactionResult{.numEntriesCompacted = result.compactedRange.count(),
                          .range = result.compactedRange,
                          .stopReason = result.stopReason};
}

auto LogFollowerImpl::getParticipantId() const noexcept
    -> ParticipantId const& {
  return myself;
}

auto LogFollowerImpl::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  return guarded.getLockedGuard()->appendEntriesManager->appendEntries(
      std::move(request));
}

LogFollowerImpl::LogFollowerImpl(
    ParticipantId myself,
    std::unique_ptr<replicated_state::IStorageEngineMethods> methods,
    std::unique_ptr<IReplicatedStateHandle> stateHandlePtr,
    std::shared_ptr<const FollowerTermInformation> termInfo,
    std::shared_ptr<const ReplicatedLogGlobalSettings> options,
    std::shared_ptr<ReplicatedLogMetrics> metrics,
    std::shared_ptr<ILeaderCommunicator> leaderComm)
    : myself(std::move(myself)),
      guarded(std::move(methods), std::move(stateHandlePtr),
              std::move(termInfo), std::move(options), std::move(metrics),
              std::move(leaderComm)) {}
