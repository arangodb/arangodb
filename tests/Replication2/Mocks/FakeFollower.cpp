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

#include "FakeFollower.h"

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Replication2/Exceptions/ParticipantResignedException.h"
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/LogStatus.h"
#include "Replication2/ReplicatedLog/NetworkMessages.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::test;

auto FakeFollower::release(arangodb::replication2::LogIndex doneWithIdx)
    -> arangodb::Result {
  guarded.getLockedGuard()->doneWithIdx = doneWithIdx;
  return {};  // return ok
}

auto FakeFollower::getLeader() const noexcept
    -> std::optional<ParticipantId> const& {
  return leaderId;
}

auto FakeFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return id;
}

auto FakeFollower::getCommitIndex() const noexcept -> LogIndex {
  return guarded.getLockedGuard()->commitIndex;
}

auto FakeFollower::resign() && -> std::tuple<
    std::unique_ptr<replicated_log::LogCore>, DeferredAction> {
  resign();
  return std::make_tuple(nullptr, DeferredAction{});
}

FakeFollower::FakeFollower(ParticipantId id,
                           std::optional<ParticipantId> leader, LogTerm term)
    : id(std::move(id)), leaderId(std::move(leader)), term(term) {}

auto FakeFollower::getStatus() const -> replicated_log::LogStatus {
  auto guard = guarded.getLockedGuard();
  return replicated_log::LogStatus{replicated_log::FollowerStatus{
      .local =
          {
              .spearHead = guard->log.getLastTermIndexPair(),
              .commitIndex = guard->commitIndex,
              .firstIndex = guard->log.getFirstIndex(),
          },
      .leader = leaderId,
      .term = term,
      .largestCommonIndex = LogIndex{0},
  }};
}

auto FakeFollower::getQuickStatus() const -> replicated_log::QuickLogStatus {
  auto guard = guarded.getLockedGuard();
  constexpr auto kBaseIndex = LogIndex{0};
  return replicated_log::QuickLogStatus{
      .role = replicated_log::ParticipantRole::kFollower,
      .term = term,
      .local = {{
          .spearHead = guard->log.getLastTermIndexPair(),
          .commitIndex = guard->commitIndex,
          .firstIndex = guard->log.getFirstIndex(),
      }},
      .leadershipEstablished = guard->commitIndex > kBaseIndex,
  };
}

auto FakeFollower::waitForLeaderAcked() -> WaitForFuture {
  return waitForLeaderAckedQueue.waitFor({});
}

auto FakeFollower::addEntry(LogPayload payload) -> LogIndex {
  auto index = guarded.doUnderLock([&](GuardedFollowerData& data) {
    auto index = data.log.getNextIndex();
    auto memtry = InMemoryLogEntry(PersistingLogEntry(term, index, payload));
    data.log.appendInPlace(LoggerContext(Logger::REPLICATION2),
                           std::move(memtry));
    return index;
  });

  return index;
}
auto FakeFollower::appendEntries(replicated_log::AppendEntriesRequest request)
    -> futures::Future<replicated_log::AppendEntriesResult> {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void FakeFollower::triggerLeaderAcked() {
  waitForLeaderAckedQueue.resolveAll(replicated_log::WaitForResult{});
}

auto FakeFollower::waitFor(LogIndex index) -> WaitForFuture {
  return waitForQueue.waitFor(index);
}

auto FakeFollower::waitForIterator(LogIndex index)
    -> replicated_log::ILogParticipant::WaitForIteratorFuture {
  return waitFor(index).thenValue(
      [this, index](auto&&) -> std::unique_ptr<LogRangeIterator> {
        auto guard = guarded.getLockedGuard();
        return guard->log.getIteratorRange(index, guard->commitIndex + 1);
      });
}

void FakeFollower::updateCommitIndex(LogIndex index) {
  guarded.getLockedGuard()->commitIndex = index;
  waitForQueue.resolve(index, replicated_log::WaitForResult{index, nullptr});
}

void FakeFollower::resign() & {
  waitForQueue.resolveAll(futures::Try<replicated_log::WaitForResult>(
      std::make_exception_ptr(replicated_log::ParticipantResignedException(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED, ADB_HERE))));
  waitForLeaderAckedQueue.resolveAll(
      futures::Try<replicated_log::WaitForResult>(
          std::make_exception_ptr(replicated_log::ParticipantResignedException(
              TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED,
              ADB_HERE))));
}
