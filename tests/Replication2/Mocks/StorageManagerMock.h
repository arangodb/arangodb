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

#include <gmock/gmock.h>

#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/Storage/PersistedStateInfo.h"

namespace arangodb::replication2::test {

struct StorageManagerMock
    : arangodb::replication2::replicated_log::IStorageManager {
  MOCK_METHOD(std::unique_ptr<
                  arangodb::replication2::replicated_log::IStorageTransaction>,
              transaction, (), (override));
  MOCK_METHOD(std::unique_ptr<TypedLogRangeIterator<LogEntryView>>,
              getCommittedLogIterator, (std::optional<LogRange>),
              (const, override));
  MOCK_METHOD(std::unique_ptr<PersistedLogIterator>, getPersistedLogIterator,
              (LogIndex), (const, override));
  MOCK_METHOD(arangodb::replication2::replicated_log::TermIndexMapping,
              getTermIndexMapping, (), (const, override));
  MOCK_METHOD(storage::PersistedStateInfo, getCommittedMetaInfo, (),
              (const, override));
  MOCK_METHOD(
      std::unique_ptr<
          arangodb::replication2::replicated_log::IStateInfoTransaction>,
      beginMetaInfoTrx, (), (override));
  MOCK_METHOD(
      Result, commitMetaInfoTrx,
      (std::unique_ptr<
          arangodb::replication2::replicated_log::IStateInfoTransaction>),
      (override));
  MOCK_METHOD(LogIndex, getSyncIndex, (), (const, override));
};

}  // namespace arangodb::replication2::test
