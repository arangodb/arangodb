////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>

#include "Replication2/ReplicatedLog/Components/IFollowerCommitManager.h"

namespace arangodb::replication2::test {
struct FollowerCommitManagerMock : replicated_log::IFollowerCommitManager {
  MOCK_METHOD((std::pair<std::optional<LogIndex>, DeferredAction>),
              updateCommitIndex, (LogIndex commitIndex, bool snapshotAvailable),
              (noexcept, override));
  MOCK_METHOD(LogIndex, getCommitIndex, (), (const, noexcept, override));
  MOCK_METHOD(replicated_log::ILogParticipant::WaitForFuture, waitFor,
              (LogIndex index), (noexcept, override));
  MOCK_METHOD(replicated_log::ILogParticipant::WaitForIteratorFuture,
              waitForIterator, (LogIndex index), (noexcept, override));
};
}  // namespace arangodb::replication2::test