////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedLog/ILogInterfaces.h"

#include <gmock/gmock.h>

namespace arangodb::replication2::test {

struct LogFollowerMock : replicated_log::ILogFollower {
  MOCK_METHOD(replicated_log::LogStatus, getStatus, (), (const, override));
  MOCK_METHOD(replicated_log::QuickLogStatus, getQuickStatus, (),
              (const, override));
  MOCK_METHOD(
      (std::tuple<std::unique_ptr<storage::IStorageEngineMethods>,
                  std::unique_ptr<replicated_log::IReplicatedStateHandle>,
                  DeferredAction>),
      resign, (), (override, ref(&&)));

  MOCK_METHOD(WaitForFuture, waitFor, (LogIndex), (override));
  MOCK_METHOD(WaitForIteratorFuture, waitForIterator, (LogIndex), (override));
  MOCK_METHOD(std::unique_ptr<LogIterator>, getInternalLogIterator,
              (std::optional<LogRange> bounds), (const, override));
  MOCK_METHOD(ResultT<arangodb::replication2::replicated_log::CompactionResult>,
              compact, (), (override));

  MOCK_METHOD(ParticipantId const&, getParticipantId, (),
              (const, noexcept, override));
  MOCK_METHOD(futures::Future<replicated_log::AppendEntriesResult>,
              appendEntries, (replicated_log::AppendEntriesRequest),
              (override));
};
}  // namespace arangodb::replication2::test
