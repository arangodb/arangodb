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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"

#include <rocksdb/options.h>

namespace rocksdb {
class Iterator;
class Comparator;
class TransactionDB;
}  // namespace rocksdb

namespace arangodb {
/// @brief return false to stop iteration
typedef std::function<bool(rocksdb::Slice const& key,
                           rocksdb::Slice const& value)>
    GenericCallback;

/// @brief a forward-only iterator over the primary index, only reading from the
/// database, not taking into account changes done in the current transaction
class RocksDBGenericIterator {
 public:
  RocksDBGenericIterator(rocksdb::TransactionDB* db,
                         rocksdb::ReadOptions& options,
                         RocksDBKeyBounds const& bounds);
  RocksDBGenericIterator(RocksDBGenericIterator&&) = default;

  ~RocksDBGenericIterator() = default;

  //* The following functions returns true if the iterator is valid within
  // bounds on return.
  //  @param limit - number of documents the callback should be applied to
  bool next(GenericCallback const& cb, size_t limit);

  bool seek(rocksdb::Slice const& key);
  bool hasMore() const;

  // return bounds
  RocksDBKeyBounds const& bounds() const { return _bounds; }

 private:
  bool outOfRange() const;

  RocksDBKeyBounds const _bounds;
  rocksdb::ReadOptions const _options;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  rocksdb::Comparator const* _cmp;
};

RocksDBGenericIterator createPrimaryIndexIterator(transaction::Methods* trx,
                                                  LogicalCollection* col);

namespace rocksdb_iterators {
std::unique_ptr<IndexIterator> createAllIterator(LogicalCollection* collection,
                                                 transaction::Methods* trx,
                                                 ReadOwnWrites readOwnWrites);

std::unique_ptr<IndexIterator> createAnyIterator(LogicalCollection* collection,
                                                 transaction::Methods* trx);
}  // namespace rocksdb_iterators

}  // namespace arangodb
