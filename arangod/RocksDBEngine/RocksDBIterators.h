////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ITERATORS_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ITERATORS_H 1

#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class Iterator;
class Comparator;
}

namespace arangodb {
class RocksDBCollection;
class RocksDBPrimaryIndex;

/// @brief iterator over all documents in the collection
/// basically sorted after LocalDocumentId
class RocksDBAllIndexIterator final : public IndexIterator {
 public:
  RocksDBAllIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          RocksDBPrimaryIndex const* index);
  ~RocksDBAllIndexIterator() {}

  char const* typeName() const override { return "all-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;
  void skip(uint64_t count, uint64_t& skipped) override;

  void reset() override;

 private:
  bool outOfRange() const;

  RocksDBKeyBounds const _bounds;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  rocksdb::Comparator const* _cmp;
};

class RocksDBAnyIndexIterator final : public IndexIterator {
 public:
  RocksDBAnyIndexIterator(LogicalCollection* collection,
                          transaction::Methods* trx,
                          RocksDBPrimaryIndex const* index);

  ~RocksDBAnyIndexIterator() {}

  char const* typeName() const override { return "any-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  bool nextDocument(DocumentCallback const& cb, size_t limit) override;

  void reset() override;

 private:
  bool outOfRange() const;
  static uint64_t newOffset(LogicalCollection* collection,
                            transaction::Methods* trx);

  bool checkIter();
  rocksdb::Comparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  RocksDBKeyBounds const _bounds;
  uint64_t _total;
  uint64_t _returned;
  bool _forward;
};

/// @brief iterates over the primary index and does lookups
/// into the document store. E.g. used for incremental sync
class RocksDBSortedAllIterator final : public IndexIterator {
 public:
  RocksDBSortedAllIterator(LogicalCollection* collection,
                           transaction::Methods* trx,
                           RocksDBPrimaryIndex const* index);

  ~RocksDBSortedAllIterator() {}

  char const* typeName() const override { return "sorted-all-index-iterator"; }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override;
  void reset() override;

  // engine specific optimizations
  void seek(StringRef const& key);

 private:
  bool outOfRange() const;

  transaction::Methods* _trx;
  RocksDBKeyBounds const _bounds;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  rocksdb::Comparator const* _cmp;
};
}

#endif
