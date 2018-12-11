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
#include "RocksDBEngine/RocksDBColumnFamily.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

namespace rocksdb {
class Iterator;
class Comparator;
class TransactionDB;
}

namespace arangodb {
class RocksDBCollection;
class RocksDBPrimaryIndex;

/// @brief return false to stop iteration
typedef std::function<bool(rocksdb::Slice const& key, rocksdb::Slice const& value)> GenericCallback;

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

 private:
  RocksDBKeyBounds const _bounds;
  rocksdb::Slice _upperBound; // used for iterate_upper_bound
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
  bool checkIter();

  rocksdb::Comparator const* _cmp;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  uint64_t const _objectId;
  RocksDBKeyBounds const _bounds;
  
  uint64_t _total;
  uint64_t _returned;
  bool _forward;
};

class RocksDBGenericIterator {
 public:
  RocksDBGenericIterator(rocksdb::ReadOptions& options,
                         RocksDBKeyBounds const& bounds,
                         bool reverse = false);
  RocksDBGenericIterator(RocksDBGenericIterator&&) = default;

  ~RocksDBGenericIterator() {}

  // the following functions return if the iterator
  // is valid and in bounds on return.
  bool next(GenericCallback const& cb, size_t count); //number of documents the callback should be applied to

  // documents to skip, skipped documents
  bool skip(uint64_t count, uint64_t& skipped);
  bool seek(rocksdb::Slice const& key);
  bool reset();
  bool hasMore() const;

  // return bounds
  RocksDBKeyBounds const& bounds() const { return _bounds; }

 private:
  bool outOfRange() const;
  
 private:
  bool _reverse;
  RocksDBKeyBounds const _bounds;
  rocksdb::ReadOptions const _options;
  std::unique_ptr<rocksdb::Iterator> _iterator;
  rocksdb::Comparator const* _cmp;
};

RocksDBGenericIterator createPrimaryIndexIterator(transaction::Methods* trx,
                                                  LogicalCollection* col);
} //namespace arangodb
#endif
