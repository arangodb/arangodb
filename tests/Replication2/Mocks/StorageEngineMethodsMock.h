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

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "immer/flex_vector_transient.hpp"
#include "Basics/Result.h"
#include "Replication2/ReplicatedLog/Components/StorageManager.h"
#include "Replication2/ReplicatedState/PersistedStateInfo.h"
#include "Replication2/Mocks/FakeStorageEngineMethods.h"
#include "Replication2/Mocks/FakeAsyncExecutor.h"

namespace arangodb::replication2::tests {

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

struct StorageEngineMethodsGMock : IStorageEngineMethods {
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
  MOCK_METHOD(futures::Future<Result>, waitForSync, (SequenceNumber),
              (override));
  MOCK_METHOD(void, waitForCompletion, (), (noexcept, override));
};

}  // namespace arangodb::replication2::tests
