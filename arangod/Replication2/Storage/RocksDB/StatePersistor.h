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

#include "Replication2/Storage/IStatePersistor.h"

namespace rocksdb {
class DB;
class ColumnFamilyHandle;
}  // namespace rocksdb

namespace arangodb::replication2::storage::rocksdb {

struct AsyncLogWriteContext;

struct StatePersistor final : storage::IStatePersistor {
  StatePersistor(LogId logId, AsyncLogWriteContext& ctx,
                 ::rocksdb::DB* const db,
                 ::rocksdb::ColumnFamilyHandle* const metaCf);
  [[nodiscard]] auto updateMetadata(
      replication2::storage::PersistedStateInfo info) -> Result override;
  [[nodiscard]] auto readMetadata()
      -> ResultT<replication2::storage::PersistedStateInfo> override;

  auto drop() -> Result override;

 private:
  LogId const logId;
  AsyncLogWriteContext& ctx;
  ::rocksdb::DB* const db;
  ::rocksdb::ColumnFamilyHandle* const metaCf;
};

}  // namespace arangodb::replication2::storage::rocksdb
