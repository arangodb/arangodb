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

#pragma once
#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/InMemoryLog.h"
#include "Replication2/ReplicatedLog/WaitForBag.h"
#include "Replication2/Helper/WaitForQueue.h"
#include "Basics/UnshackledMutex.h"
#include "Basics/Guarded.h"
#include "Replication2/Streams/MultiplexedValues.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"

namespace arangodb::replication2::test {

struct FakeLeader final : replicated_log::ILogLeader,
                          std::enable_shared_from_this<FakeLeader> {
  auto getStatus() const -> replicated_log::LogStatus override;
  auto getQuickStatus() const -> replicated_log::QuickLogStatus override;
  auto resign() && -> std::tuple<std::unique_ptr<replicated_log::LogCore>,
                                 DeferredAction> override;
  auto waitFor(LogIndex index) -> WaitForFuture override;
  auto waitForIterator(LogIndex index) -> WaitForIteratorFuture override;
  auto waitForResign() -> futures::Future<futures::Unit> override;
  auto getCommitIndex() const noexcept -> LogIndex override;
  auto copyInMemoryLog() const -> replicated_log::InMemoryLog override;
  auto release(LogIndex doneWithIdx) -> Result override;
  auto insert(LogPayload payload, bool waitForSync) -> LogIndex override;
  auto insert(LogPayload payload, bool waitForSync,
              DoNotTriggerAsyncReplication replication) -> LogIndex override;

  void triggerAsyncReplication() override;
  auto isLeadershipEstablished() const noexcept -> bool override;
  auto waitForLeadership() -> WaitForFuture override;

  template<typename State>
  auto insertMultiplexedValue(typename State::EntryType const& t) -> LogIndex {
    using streamSpec =
        typename replicated_state::ReplicatedStateStreamSpec<State>;
    velocypack::Builder builder;
    using descriptor = streams::stream_descriptor_by_id_t<1, streamSpec>;
    streams::MultiplexedValues::toVelocyPack<descriptor>(t, builder);
    return insert(LogPayload::createFromSlice(builder.slice()));
  }

  auto insert(LogPayload payload) -> LogIndex;
  void resign() &;
  void updateCommitIndex(LogIndex index);
  void triggerLeaderEstablished(LogIndex commitIndex);

 private:
  struct GuardedLeaderData {
    LogIndex commitIndex{0};
    LogIndex doneWithIdx{0};
    replicated_log::InMemoryLog log;
  };

  test::WaitForQueue<LogIndex, replicated_log::WaitForResult> waitForQueue;
  test::SimpleWaitForQueue<replicated_log::WaitForResult>
      waitForLeaderEstablishedQueue;
  WaitForBag waitForResignQueue;
  Guarded<GuardedLeaderData, basics::UnshackledMutex> guarded;
  ParticipantId const id;
  LogTerm const term;
};

}  // namespace arangodb::replication2::test
