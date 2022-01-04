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

#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/Helper/WaitForQueue.h"
#include "Basics/UnshackledMutex.h"
#include "Basics/Guarded.h"
#include "Replication2/Streams/MultiplexedValues.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"

namespace arangodb::replication2::test {

struct FakeFollower final : replicated_log::ILogFollower,
                            std::enable_shared_from_this<FakeFollower> {
  FakeFollower(ParticipantId id, std::optional<ParticipantId> leader,
               LogTerm term);

  auto getStatus() const -> replicated_log::LogStatus override;
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>,
                                 DeferredAction> override;
  void resign() &;
  auto waitFor(LogIndex index) -> WaitForFuture override;
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;
  auto getCommitIndex() const noexcept -> LogIndex override;

  auto release(LogIndex doneWithIdx) -> Result override;

  auto waitForLeaderAcked() -> WaitForFuture override;

  auto getLeader() const noexcept
      -> std::optional<ParticipantId> const& override;

  auto getParticipantId() const noexcept -> ParticipantId const& override;

  auto appendEntries(replicated_log::AppendEntriesRequest request)
      -> futures::Future<replicated_log::AppendEntriesResult> override;

  void updateCommitIndex(LogIndex index);
  auto addEntry(LogPayload) -> LogIndex;
  void triggerLeaderAcked();

  template<typename State>
  auto insertMultiplexedValue(typename State::EntryType const& t) -> LogIndex {
    using streamSpec =
        typename replicated_state::ReplicatedStateStreamSpec<State>;
    velocypack::UInt8Buffer buffer;
    velocypack::Builder builder(buffer);
    using descriptor = streams::stream_descriptor_by_id_t<1, streamSpec>;
    streams::MultiplexedValues::toVelocyPack<descriptor>(t, builder);
    return addEntry(LogPayload(std::move(buffer)));
  }

 private:
  struct GuardedFollowerData {
    LogIndex commitIndex{0};
    LogIndex doneWithIdx{0};
    replicated_log::InMemoryLog log;
  };

  test::WaitForQueue<LogIndex, replicated_log::WaitForResult> waitForQueue;
  test::SimpleWaitForQueue<replicated_log::WaitForResult>
      waitForLeaderAckedQueue;
  Guarded<GuardedFollowerData, basics::UnshackledMutex> guarded;
  ParticipantId const id;
  std::optional<ParticipantId> const leaderId;
  LogTerm const term;
};

}  // namespace arangodb::replication2::test
