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
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "RocksDBEngine/RocksDBValue.h"

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/utilities/transaction_db.h>

#include <atomic>

namespace rocksdb {
class Comparator;
class ColumnFamilyHandle;
class DB;
class Iterator;
struct ReadOptions;
class Snapshot;
class TransactionDB;
}  // namespace rocksdb

namespace arangodb {

class RocksDBMethods;
class RocksDBKeyBounds;
class RocksDBEngine;

namespace rocksutils {

/// @brief throws an exception of appropriate type if the iterator's status is !ok().
/// does nothing if the iterator's status is ok().
/// this function can be used by IndexIterators to verify that an iterator is still
/// in good shape
void checkIteratorStatus(rocksdb::Iterator const* iterator);

/// @brief count all keys in the given column family
std::size_t countKeys(rocksdb::DB*, rocksdb::ColumnFamilyHandle* cf);

/// @brief iterate over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB*, RocksDBKeyBounds const&, 
                          rocksdb::Snapshot const* snapshot, bool prefixSameAsStart);

/// @brief whether or not the specified range has keys
bool hasKeys(rocksdb::DB*, RocksDBKeyBounds const&, 
             rocksdb::Snapshot const* snapshot, bool prefixSameAsStart);

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::DB* db, RocksDBKeyBounds const& bounds,
                        bool prefixSameAsStart, bool useRangeDelete);

/// @brief compacts the entire key range of the database.
/// warning: may cause a full rewrite of the entire database, which will
/// take long for large databases - use with care!
Result compactAll(rocksdb::DB* db, bool changeLevel, bool compactBottomMostLevel,
                  std::atomic<bool>* canceled = nullptr);

// optional switch to std::function to reduce amount of includes and
// to avoid template
// this helper is not meant for transactional usage!
template <typename T>  // T is an invokeable that takes a rocksdb::Iterator*
void iterateBounds(rocksdb::TransactionDB* db, RocksDBKeyBounds const& bounds, T callback) {
  rocksdb::Slice const end = bounds.end();
  
  rocksdb::ReadOptions options;
  options.iterate_upper_bound = &end;  // safe to use on rocksb::DB directly
  options.prefix_same_as_start = true;
  options.verify_checksums = false;
  options.fill_cache = false;
  std::unique_ptr<rocksdb::Iterator> it(db->NewIterator(options, bounds.columnFamily()));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    callback(it.get());
  }
}

}  // namespace rocksutils
}  // namespace arangodb

