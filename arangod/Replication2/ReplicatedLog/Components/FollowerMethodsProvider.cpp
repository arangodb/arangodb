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

#include "FollowerMethodsProvider.h"
#include "IStorageManager.h"
#include "ICompactionManager.h"
#include "WaitQueueManager.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

namespace {

struct MethodsProvider : IReplicatedLogFollowerMethods {
  explicit MethodsProvider(std::shared_ptr<IStorageManager> const& storage,
                           std::shared_ptr<ICompactionManager> const compaction,
                           std::shared_ptr<IWaitQueueManager> const waitQueue)
      : storage(storage), compaction(compaction), waitQueue(waitQueue) {}

  auto releaseIndex(LogIndex index) -> void override {
    compaction->updateReleaseIndex(index);
  }

  auto getCommittedLogIterator(std::optional<LogRange> range)
      -> std::unique_ptr<LogRangeIterator> override {
    return storage->getCommittedLogIterator(range);
  }

  auto waitFor(LogIndex index) -> ILogParticipant::WaitForFuture override {
    return waitQueue->waitFor(index);
  }

  auto waitForIterator(LogIndex index)
      -> ILogParticipant::WaitForIteratorFuture override {
    return waitQueue->waitForIterator(index);
  }

  auto snapshotCompleted(std::uint64_t version) -> Result override {
    std::abort();
  }

  [[nodiscard]] auto leaderConnectionEstablished() const -> bool override {
    // Having a commit index means we've got at least one append entries request
    // which was also applied *successfully*.
    // Note that this is pessimistic, in the sense that it actually waits for an
    // append entries request that was sent after leadership was established,
    // which we don't necessarily need.
    // return commit->getCommitIndex() > LogIndex{0}; -- std::abort();
  }

  auto checkSnapshotState() const noexcept
      -> replicated_log::SnapshotState override {
    std::abort();
  }

  std::shared_ptr<IStorageManager> const storage;
  std::shared_ptr<ICompactionManager> const compaction;
  std::shared_ptr<IWaitQueueManager> const waitQueue;
};

}  // namespace

std::unique_ptr<IReplicatedLogFollowerMethods>
FollowerMethodsProvider::getMethods() {
  return std::make_unique<MethodsProvider>(storage, compaction, waitQueue);
}

FollowerMethodsProvider::FollowerMethodsProvider(
    std::shared_ptr<IStorageManager> storage,
    std::shared_ptr<ICompactionManager> compaction,
    std::shared_ptr<IWaitQueueManager> waitQueue)
    : storage(std::move(storage)),
      compaction(std::move(compaction)),
      waitQueue(std::move(waitQueue)) {}
