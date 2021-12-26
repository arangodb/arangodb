////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/RocksDBMethods.h"

namespace arangodb {

/// wraps a writebatch - non transactional
class RocksDBBatchedMethods final : public RocksDBMethods {
 public:
  explicit RocksDBBatchedMethods(rocksdb::WriteBatch*);

  rocksdb::Status Get(rocksdb::ColumnFamilyHandle*, rocksdb::Slice const&,
                      rocksdb::PinnableSlice*, ReadOwnWrites) override;
  rocksdb::Status GetForUpdate(rocksdb::ColumnFamilyHandle*,
                               rocksdb::Slice const&,
                               rocksdb::PinnableSlice*) override;
  rocksdb::Status Put(rocksdb::ColumnFamilyHandle*, RocksDBKey const& key,
                      rocksdb::Slice const& val, bool assume_tracked) override;
  rocksdb::Status PutUntracked(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const& key,
                               rocksdb::Slice const& val) override;
  rocksdb::Status Delete(rocksdb::ColumnFamilyHandle*,
                         RocksDBKey const& key) override;
  rocksdb::Status SingleDelete(rocksdb::ColumnFamilyHandle*,
                               RocksDBKey const&) override;
  void PutLogData(rocksdb::Slice const&) override;

 private:
  rocksdb::WriteBatch* _wb;
};

}  // namespace arangodb
