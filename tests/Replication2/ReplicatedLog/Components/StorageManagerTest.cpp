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
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

struct StorageEngineMethodsMock : IStorageEngineMethods {
  MOCK_METHOD(Result, updateMetadata, (PersistedStateInfo), (override));
  MOCK_METHOD(ResultT<PersistedStateInfo>, readMetadata, (), (override));
  MOCK_METHOD(std::unique_ptr<PersistedLogIterator>, read, (LogIndex),
              (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, insert,
              (std::unique_ptr<PersistedLogIterator>, WriteOptions const&),
              (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, removeFront,
              (LogIndex stop, WriteOptions const&), (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, removeBack,
              (LogIndex start, WriteOptions const&), (override));
  MOCK_METHOD(std::uint64_t, getObjectId, (), (override));
  MOCK_METHOD(LogId, getLogId, (), (override));
  MOCK_METHOD(SequenceNumber, getSyncedSequenceNumber, (), (override));
  MOCK_METHOD(futures::Future<futures::Unit>, waitForSync, (SequenceNumber),
              (override));
};

struct StorageEngineMethodsMockFactory {
  auto create() -> std::unique_ptr<StorageEngineMethodsMock> {
    auto ptr = std::make_unique<StorageEngineMethodsMock>();
    lastPtr = ptr.get();
    return ptr;
  }
  auto operator->() noexcept -> StorageEngineMethodsMock* { return lastPtr; }

 private:
  StorageEngineMethodsMock* lastPtr{nullptr};
};

struct StorageManagerTest : ::testing::Test {
  StorageEngineMethodsMockFactory methods;
  std::shared_ptr<StorageManager> storageManager =
      std::make_shared<StorageManager>(methods.create());
};
