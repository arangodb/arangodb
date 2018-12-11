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

#include "RocksDBIterators.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

namespace {
constexpr bool AllIteratorFillBlockCache = true;
constexpr bool AnyIteratorFillBlockCache = false;
}

// ================ All Iterator ==================

RocksDBAllIndexIterator::RocksDBAllIndexIterator(
    LogicalCollection* col, transaction::Methods* trx, RocksDBPrimaryIndex const* index)
    : IndexIterator(col, trx),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(col->getPhysical())->objectId())),
      _upperBound(_bounds.end()),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()) {
  // acquire rocksdb transaction
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  rocksdb::ColumnFamilyHandle* cf = RocksDBColumnFamily::documents();

  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AllIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  options.iterate_upper_bound = &_upperBound;
  // options.readahead_size = 4 * 1024 * 1024;
  _iterator = mthds->NewIterator(options, cf);
  TRI_ASSERT(_iterator);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::ColumnFamilyDescriptor desc;
  cf->GetDescriptor(&desc);
  TRI_ASSERT(desc.options.prefix_extractor);
#endif
  _iterator->Seek(_bounds.start());
}

bool RocksDBAllIndexIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

bool RocksDBAllIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_bounds.objectId() == RocksDBKey::objectId(_iterator->key()));
#endif

    cb(RocksDBKey::documentId(_iterator->key()));
    --limit;
    _iterator->Next();

    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }

  return true;
}

bool RocksDBAllIndexIterator::nextDocument(
    IndexIterator::DocumentCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()),
       VPackSlice(_iterator->value().data()));
    --limit;
    _iterator->Next();

    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }

  return true;
}

void RocksDBAllIndexIterator::skip(uint64_t count, uint64_t& skipped) {
  TRI_ASSERT(_trx->state()->isRunning());

  while (count > 0 && _iterator->Valid()) {
    --count;
    ++skipped;

    _iterator->Next();
  }
}

void RocksDBAllIndexIterator::reset() {
  TRI_ASSERT(_trx->state()->isRunning());
  _iterator->Seek(_bounds.start());
}

// ================ Any Iterator ================
RocksDBAnyIndexIterator::RocksDBAnyIndexIterator(
    LogicalCollection* col, transaction::Methods* trx,
    RocksDBPrimaryIndex const* index)
    : IndexIterator(col, trx),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()),
      _objectId(static_cast<RocksDBCollection*>(col->getPhysical())->objectId()),
      _bounds(RocksDBKeyBounds::CollectionDocuments(_objectId)),
      _total(0),
      _returned(0) {
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  auto options = mthds->iteratorReadOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AnyIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  _iterator = mthds->NewIterator(options, RocksDBColumnFamily::documents());
  TRI_ASSERT(_iterator);

  _total = col->numberDocuments(trx, transaction::CountType::Normal);
  _forward = RandomGenerator::interval(uint16_t(1)) ? true : false;
   reset();   //initial seek
}

bool RocksDBAnyIndexIterator::checkIter() {
  if ( /* not  valid */            !_iterator->Valid() ||
       /* out of range forward */  ( _forward && _cmp->Compare(_iterator->key(), _bounds.end())   > 0) ||
       /* out of range backward */ (!_forward && _cmp->Compare(_iterator->key(), _bounds.start()) < 0)  ) {

    if (_forward) {
      _iterator->Seek(_bounds.start());
    } else {
      _iterator->SeekForPrev(_bounds.end());
    }

    if(!_iterator->Valid()) {
      return false;
    }
  }
  return true;
}

bool RocksDBAnyIndexIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()));
    --limit;
    _returned++;
    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      if (_returned < _total) {
        _iterator->Seek(_bounds.start());
        continue;
      }
      return false;
    }
  }
  return true;
}

bool RocksDBAnyIndexIterator::nextDocument(
    IndexIterator::DocumentCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()),
       VPackSlice(_iterator->value().data()));
    --limit;
    _returned++;
    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      if (_returned < _total) {
        _iterator->Seek(_bounds.start());
        continue;
      }
      return false;
    }
  }
  return true;
}

void RocksDBAnyIndexIterator::reset() {
  // the assumption is that we don't reset this iterator unless
  // it is out of range or invalid
  if (_total == 0 || (_iterator->Valid() && !outOfRange())) {
    return;
  }
  uint64_t steps = RandomGenerator::interval(_total - 1) % 500;
  auto initialKey = RocksDBKey();
  
  initialKey.constructDocument(_objectId,
                               LocalDocumentId(RandomGenerator::interval(UINT64_MAX)));
  _iterator->Seek(initialKey.string());
  
  if (checkIter()) {
    if (_forward) {
      while (steps-- > 0) {
        _iterator->Next();
        if(!checkIter()) { break; }
      }
    } else {
      while (steps-- > 0) {
        _iterator->Prev();
        if(!checkIter()) { break; }
      }
    }
  }
}

bool RocksDBAnyIndexIterator::outOfRange() const {
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

RocksDBGenericIterator::RocksDBGenericIterator(rocksdb::ReadOptions& options,
                                               RocksDBKeyBounds const& bounds,
                                               bool reverse)
    : _reverse(reverse)
    , _bounds(bounds)
    , _options(options)
    , _iterator(arangodb::rocksutils::globalRocksDB()->NewIterator(_options, _bounds.columnFamily()))
    , _cmp(_bounds.columnFamily()->GetComparator()) {
  reset();
}

bool RocksDBGenericIterator::hasMore() const {
  return _iterator->Valid() && !outOfRange();
}

bool RocksDBGenericIterator::outOfRange() const {
  if (_reverse) {
    return _cmp->Compare(_iterator->key(), _bounds.start()) < 0;
  } else {
    return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
  }
}

bool RocksDBGenericIterator::reset() {
  if (_reverse) {
    return seek(_bounds.end());
  } else {
    return seek(_bounds.start());
  }
}

bool RocksDBGenericIterator::skip(uint64_t count, uint64_t& skipped) {
  bool hasMore = _iterator->Valid();
  while (count > 0 && hasMore) {
    hasMore = next([&count, &skipped](rocksdb::Slice const&, rocksdb::Slice const&) { 
      --count; 
      ++skipped;
      return true;
    }, count /*gets copied*/);
  }
  return hasMore;
}

bool RocksDBGenericIterator::seek(rocksdb::Slice const& key) {
  if (_reverse) {
    _iterator->SeekForPrev(key);
  } else {
    _iterator->Seek(key);
  }
  return hasMore();
}

bool RocksDBGenericIterator::next(GenericCallback const& cb, size_t limit) {
  // @params
  // limit - maximum number of documents

  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
  if (limit == 0) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    return false;
  }

  while (limit > 0 && hasMore()) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_bounds.objectId() == RocksDBKey::objectId(_iterator->key()));
#endif

    if (!cb(_iterator->key(), _iterator->value())) {
      // stop iteration
      return false;
    }
    --limit;
    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }
  }

  return hasMore();
}

RocksDBGenericIterator arangodb::createPrimaryIndexIterator(transaction::Methods* trx,
                                                            LogicalCollection* col) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(trx != nullptr);

  auto* mthds = RocksDBTransactionState::toMethods(trx);

  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  TRI_ASSERT(options.snapshot != nullptr); // trx must contain a valid snapshot
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = false;
  options.verify_checksums = false;

  auto index = col->lookupIndex(0); //RocksDBCollection->primaryIndex() is private
  TRI_ASSERT(index->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  auto primaryIndex = static_cast<RocksDBPrimaryIndex*>(index.get());

  auto bounds(RocksDBKeyBounds::PrimaryIndex(primaryIndex->objectId()));
  auto iterator = RocksDBGenericIterator(options, bounds);

  TRI_ASSERT(iterator.bounds().objectId() == primaryIndex->objectId());
  TRI_ASSERT(iterator.bounds().columnFamily() == RocksDBColumnFamily::primary());
  return iterator;
}
