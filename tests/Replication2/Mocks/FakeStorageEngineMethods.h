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

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/Storage/IStorageEngineMethods.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteBatcher.h"

namespace arangodb::replication2::storage::test {

struct FakeStorageEngineMethodsContext {
  using LogContainerType = std::map<LogIndex, PersistingLogEntry>;
  using SequenceNumber = IStorageEngineMethods::SequenceNumber;

  FakeStorageEngineMethodsContext(
      std::uint64_t objectId, LogId logId,
      std::shared_ptr<rocksdb::AsyncLogWriteBatcher::IAsyncExecutor> executor,
      LogRange range = {}, std::optional<storage::PersistedStateInfo> = {});

  auto getMethods() -> std::unique_ptr<IStorageEngineMethods>;
  void emplaceLogRange(LogRange, LogTerm term = LogTerm{1});

  std::uint64_t const objectId;
  LogId const logId;
  std::shared_ptr<rocksdb::AsyncLogWriteBatcher::IAsyncExecutor> const executor;
  std::optional<storage::PersistedStateInfo> meta;
  LogContainerType log;
  std::unordered_set<LogIndex> writtenWithWaitForSync;
  SequenceNumber lastSequenceNumber{0};
};

struct FakeStorageEngineMethods : IStorageEngineMethods {
  auto updateMetadata(storage::PersistedStateInfo info) -> Result override;

  auto readMetadata() -> ResultT<storage::PersistedStateInfo> override;

  auto read(LogIndex first) -> std::unique_ptr<PersistedLogIterator> override;

  auto insert(std::unique_ptr<PersistedLogIterator> ptr,
              const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override;

  auto removeFront(LogIndex stop, const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override;

  auto removeBack(LogIndex start, const WriteOptions& options)
      -> futures::Future<ResultT<SequenceNumber>> override;

  auto getObjectId() -> std::uint64_t override;
  auto getLogId() -> LogId override;
  auto getSyncedSequenceNumber() -> SequenceNumber override;
  auto waitForSync(SequenceNumber number) -> futures::Future<Result> override;
  void waitForCompletion() noexcept override;

  FakeStorageEngineMethods(FakeStorageEngineMethodsContext& self);

  FakeStorageEngineMethodsContext& _self;
};

}  // namespace arangodb::replication2::storage::test
