////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "Replication2/Storage/IStorageEngineMethods.h"

#include <memory>

namespace arangodb::replication2::storage {

struct ILogPersistor;
struct IStatePersistor;

struct LogStorageMethods final : replication2::storage::IStorageEngineMethods {
  LogStorageMethods(std::unique_ptr<ILogPersistor> logPersistor,
                    std::unique_ptr<IStatePersistor> statePersistor);

  [[nodiscard]] auto updateMetadata(
      replication2::storage::PersistedStateInfo info) -> Result override;
  [[nodiscard]] auto readMetadata()
      -> ResultT<replication2::storage::PersistedStateInfo> override;
  [[nodiscard]] auto read(replication2::LogIndex first)
      -> std::unique_ptr<replication2::PersistedLogIterator> override;
  [[nodiscard]] auto insert(
      std::unique_ptr<replication2::PersistedLogIterator> ptr,
      WriteOptions const&) -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeFront(replication2::LogIndex stop,
                                 WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto removeBack(replication2::LogIndex start,
                                WriteOptions const&)
      -> futures::Future<ResultT<SequenceNumber>> override;
  [[nodiscard]] auto getObjectId() -> std::uint64_t override;
  [[nodiscard]] auto getLogId() -> replication2::LogId override;

  [[nodiscard]] auto getSyncedSequenceNumber() -> SequenceNumber override;
  [[nodiscard]] auto waitForSync(SequenceNumber number)
      -> futures::Future<Result> override;

  void waitForCompletion() noexcept override;

  [[nodiscard]] auto drop() -> Result override;
  [[nodiscard]] auto compact() -> Result override;

  std::unique_ptr<ILogPersistor> _logPersistor;
  std::unique_ptr<IStatePersistor> _statePersistor;
};

}  // namespace arangodb::replication2::storage
