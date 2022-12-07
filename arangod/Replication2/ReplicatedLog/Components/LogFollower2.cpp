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
    return follower.snapshot->updateSnapshotState(SnapshotState::AVAILABLE);
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
    std::shared_ptr<ReplicatedLogGlobalSettings const> options)
    : loggerContext(deriveLoggerContext(*termInfo)),
      options(options),
      storage(
          std::make_shared<StorageManager>(std::move(methods), loggerContext)),
      compaction(std::make_shared<CompactionManager>(*storage, options)),
      stateHandle(
          std::make_shared<StateHandleManager>(std::move(stateHandlePtr))),
      snapshot(
          std::make_shared<SnapshotManager>(*storage, *stateHandle, termInfo)),
      commit(std::make_shared<FollowerCommitManager>(*storage, *stateHandle,
                                                     loggerContext)),
      appendEntriesManager(std::make_shared<AppendEntriesManager>(
          termInfo, *storage, *snapshot, *compaction, *commit)),
      termInfo(termInfo) {
  auto provider = std::make_unique<MethodsProvider>(*this);
  stateHandle->becomeFollower(std::move(provider));
}

auto FollowerManager::getStatus() const -> LogStatus {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto FollowerManager::getQuickStatus() const -> QuickLogStatus {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
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
