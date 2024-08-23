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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>

#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/Components/IStorageManager.h"
#include "Replication2/Storage/IStorageEngineMethods.h"

namespace arangodb::replication2::test {
struct StorageTransactionMock : replicated_log::IStorageTransaction {
  MOCK_METHOD(LogRange, getLogBounds, (), (const, noexcept, override));
  MOCK_METHOD(futures::Future<Result>, removeFront, (LogIndex),
              (noexcept, override));
  MOCK_METHOD(futures::Future<Result>, removeBack, (LogIndex),
              (noexcept, override));
  MOCK_METHOD(futures::Future<Result>, appendEntries,
              (replicated_log::InMemoryLog,
               storage::IStorageEngineMethods::WriteOptions),
              (noexcept, override));
};
}  // namespace arangodb::replication2::test
