////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBSingleOperationReadOnlyMethods.h"

#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/db.h>

using namespace arangodb;

Result RocksDBSingleOperationReadOnlyMethods::beginTransaction() { return {}; }

Result RocksDBSingleOperationReadOnlyMethods::commitTransaction() { return {}; }

Result RocksDBSingleOperationReadOnlyMethods::abortTransaction() { return {}; }

rocksdb::ReadOptions
RocksDBSingleOperationReadOnlyMethods::iteratorReadOptions() const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call iteratorReadOptions for single operation methods");
}

rocksdb::Status RocksDBSingleOperationReadOnlyMethods::Get(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::Slice const& key,
    rocksdb::PinnableSlice* val, ReadOwnWrites) {
  TRI_ASSERT(cf != nullptr);
  return _db->Get(_readOptions, cf, key, val);
}

std::unique_ptr<rocksdb::Iterator>
RocksDBSingleOperationReadOnlyMethods::NewIterator(rocksdb::ColumnFamilyHandle*,
                                                   ReadOptionsCallback) {
  // This should never be called for a single operation transaction.
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      "should not call NewIterator for single operation methods");
}
