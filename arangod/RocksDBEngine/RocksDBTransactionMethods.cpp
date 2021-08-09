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

#include "RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

using namespace arangodb;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
std::size_t RocksDBTransactionMethods::countInBounds(RocksDBKeyBounds const& bounds, bool isElementInRange) {
  std::size_t count = 0;

  // iterator is from read only / trx / writebatch
  std::unique_ptr<rocksdb::Iterator> iter =
      this->NewIterator(bounds.columnFamily(), {});
  iter->Seek(bounds.start());
  auto end = bounds.end();
  rocksdb::Comparator const* cmp = bounds.columnFamily()->GetComparator();

  // extra check to avoid extra comparisons with isElementInRage later;
  if (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    ++count;
    if (isElementInRange) {
      return count;
    }
    iter->Next();
  }

  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    iter->Next();
    ++count;
  }
  return count;
};
#endif
