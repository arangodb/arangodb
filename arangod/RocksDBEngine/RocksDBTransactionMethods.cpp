////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/comparator.h>
#include <rocksdb/db.h>

using namespace arangodb;

rocksdb::ReadOptions RocksDBTransactionMethods::withUdtReadTimestamp(
    rocksdb::ReadOptions const& base, rocksdb::ColumnFamilyHandle* cf) {
  TRI_ASSERT(cf != nullptr);
  if (cf->GetComparator()->timestamp_size() == 0) {
    // Not a User-Defined Timestamp column family: a timestamp would be
    // rejected, so read as-is.
    return base;
  }
  // Read the current state. MaxU64Ts() is a Slice over static storage, so it is
  // safe to keep the Slice object in a static as well.
  static const rocksdb::Slice kCurrentStateTimestamp = rocksdb::MaxU64Ts();
  rocksdb::ReadOptions ro = base;
  ro.timestamp = &kCurrentStateTimestamp;
  return ro;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
std::size_t RocksDBTransactionMethods::countInBounds(
    RocksDBKeyBounds const& bounds, bool isElementInRange) {
  std::size_t count = 0;

  // iterator is from read only / trx / writebatch
  std::unique_ptr<rocksdb::Iterator> iter =
      NewIterator(bounds.columnFamily(), [](ReadOptions& opts) {
        opts.readOwnWrites = true;
        // disable a check that we do not create read-own-write iterators with
        // intermediate commits enabled. we can safely do this here, because our
        // iterator has a limited lifetime and can therefore not be invalidated
        // by intermediate commits.
        opts.checkIntermediateCommits = false;
      });
  iter->Seek(bounds.start());
  auto end = bounds.end();
  rocksdb::Comparator const* cmp = bounds.columnFamily()->GetComparator();

  // extra check to avoid extra comparisons with isElementInRange later;
  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    ++count;
    if (isElementInRange) {
      break;
    }
    iter->Next();
  }

  return count;
}
#endif
