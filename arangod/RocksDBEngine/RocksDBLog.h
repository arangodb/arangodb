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
#ifndef ARANGODB3_ROCKSDBLOG_H
#define ARANGODB3_ROCKSDBLOG_H

#include <rocksdb/db.h>

#include "Replication2/PersistedLog.h"
#include "RocksDBKeyBounds.h"

namespace arangodb {

class RocksDBLog : public replication2::PersistedLog {
 public:
  ~RocksDBLog() override = default;
  RocksDBLog(replication2::LogId id, rocksdb::ColumnFamilyHandle* cf,
             rocksdb::DB* db, uint64_t objectId);

  auto insert(std::shared_ptr<replication2::LogIterator> iter) -> Result override;
  auto read(replication2::LogIndex start)
      -> std::shared_ptr<replication2::LogIterator> override;
  auto remove(replication2::LogIndex stop) -> Result override;


  uint64_t objectId() const { return _objectId.load(); }

  auto drop() -> Result override;


  RocksDBKeyBounds getBounds() const { return RocksDBKeyBounds::LogRange(_objectId); }

 protected:
  std::atomic<uint64_t> _objectId;
  rocksdb::ColumnFamilyHandle* _cf;
  rocksdb::DB *_db;
};

}  // namespace arangodb

#endif  // ARANGODB3_ROCKSDBLOG_H
