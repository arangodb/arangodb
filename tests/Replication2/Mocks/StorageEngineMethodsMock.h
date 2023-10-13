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

#include "Basics/ResultT.h"
#include "Futures/Future.h"
#include "Replication2/Storage/IStorageEngineMethods.h"

namespace arangodb::replication2::storage::tests {

struct StorageEngineMethodsGMock : IStorageEngineMethods {
  MOCK_METHOD(Result, updateMetadata, (PersistedStateInfo), (override));
  MOCK_METHOD(ResultT<PersistedStateInfo>, readMetadata, (), (override));
  MOCK_METHOD(std::unique_ptr<PersistedLogIterator>, getIterator,
              (IteratorPosition), (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, insert,
              (std::unique_ptr<LogIterator>, WriteOptions const&), (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, removeFront,
              (LogIndex stop, WriteOptions const&), (override));
  MOCK_METHOD(futures::Future<ResultT<SequenceNumber>>, removeBack,
              (LogIndex start, WriteOptions const&), (override));
  MOCK_METHOD(LogId, getLogId, (), (override));
  MOCK_METHOD(futures::Future<Result>, waitForSync, (SequenceNumber),
              (override));
  MOCK_METHOD(void, waitForCompletion, (), (noexcept, override));
  MOCK_METHOD(Result, compact, (), (override));
  MOCK_METHOD(Result, drop, (), (override));
};

}  // namespace arangodb::replication2::storage::tests
