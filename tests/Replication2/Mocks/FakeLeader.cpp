////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "FakeLeader.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

auto FakeLeader::getStatus() const -> replicated_log::LogStatus {
  auto guard = guarded.getLockedGuard();
  return replicated_log::LogStatus{replicated_log::LeaderStatus{
      .local =
          {
              .spearHead = guard->log.getLastTermIndexPair(),
              .commitIndex = guard->commitIndex,
              .firstIndex = guard->log.getFirstIndex(),
          },
      .term = term,
      .lowestIndexToKeep = LogIndex{0},
  }};
}

auto FakeLeader::getQuickStatus() const -> replicated_log::QuickLogStatus {
  auto guard = guarded.getLockedGuard();
  constexpr auto kBaseIndex = LogIndex{0};
  return replicated_log::QuickLogStatus{
      .role = replicated_log::ParticipantRole::kLeader,
      .term = term,
      .local = {{
          .spearHead = guard->log.getLastTermIndexPair(),
          .commitIndex = guard->commitIndex,
          .firstIndex = guard->log.getFirstIndex(),
      }},
      .leadershipEstablished = guard->commitIndex > kBaseIndex,
  };
}

auto FakeLeader::resign() && -> std::tuple<
    std::unique_ptr<replicated_log::LogCore>, DeferredAction> {
  resign();
  return std::make_tuple(nullptr, DeferredAction{});
}

auto FakeLeader::waitFor(LogIndex index) -> WaitForFuture {
  return waitForQueue.waitFor(index);
}

auto FakeLeader::waitForIterator(LogIndex index)
    -> replicated_log::ILogParticipant::WaitForIteratorFuture {
  return waitFor(index).thenValue(
      [this, index](auto&&) -> std::unique_ptr<LogRangeIterator> {
        auto guard = guarded.getLockedGuard();
        return guard->log.getIteratorRange(index, guard->commitIndex + 1);
      });
}

auto arangodb::replication2::test::FakeLeader::waitForResign()
    -> futures::Future<futures::Unit> {
  return waitForResignQueue.addWaitFor();
}

auto FakeLeader::getCommitIndex() const noexcept -> LogIndex {
  return guarded.getLockedGuard()->commitIndex;
}

auto FakeLeader::copyInMemoryLog() const -> replicated_log::InMemoryLog {
  return guarded.getLockedGuard()->log;
}

auto FakeLeader::release(LogIndex doneWithIdx) -> Result { return Result(); }

auto FakeLeader::insert(LogPayload payload, bool waitForSync) -> LogIndex {
  return insert(std::move(payload));
}

auto FakeLeader::insert(LogPayload payload, bool waitForSync,
                        DoNotTriggerAsyncReplication replication) -> LogIndex {
  return insert(std::move(payload));
}

void FakeLeader::triggerAsyncReplication() {}
bool FakeLeader::isLeadershipEstablished() const noexcept {
  return guarded.getLockedGuard()->commitIndex > LogIndex{0};
}

auto FakeLeader::waitForLeadership() -> WaitForFuture {
  return waitForLeaderEstablishedQueue.waitFor({});
}

void FakeLeader::resign() & {
  auto const exPtr =
      std::make_exception_ptr(replicated_log::ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE));
  waitForQueue.resolveAll(futures::Try<replicated_log::WaitForResult>(exPtr));
  waitForLeaderEstablishedQueue.resolveAll(
      futures::Try<replicated_log::WaitForResult>(exPtr));
  waitForResignQueue.resolveAll();
}

auto FakeLeader::insert(LogPayload payload) -> LogIndex {
  auto index = guarded.doUnderLock([&](GuardedLeaderData& data) {
    auto index = data.log.getNextIndex();
    auto memtry = InMemoryLogEntry(PersistingLogEntry(term, index, payload));
    data.log.appendInPlace(LoggerContext(Logger::REPLICATION2),
                           std::move(memtry));
    return index;
  });

  return index;
}

void FakeLeader::updateCommitIndex(LogIndex index) {
  guarded.getLockedGuard()->commitIndex = index;
  waitForQueue.resolve(index, replicated_log::WaitForResult{index, nullptr});
}

void FakeLeader::triggerLeaderEstablished(LogIndex commitIndex) {
  guarded.getLockedGuard()->commitIndex = commitIndex;
  waitForLeaderEstablishedQueue.resolveAll(replicated_log::WaitForResult{});
  waitForQueue.resolve(commitIndex,
                       replicated_log::WaitForResult{commitIndex, nullptr});
}
