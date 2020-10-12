////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBIterators.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

namespace {
constexpr bool AllIteratorFillBlockCache = true;
constexpr bool AnyIteratorFillBlockCache = false;
}  // namespace

// ================ All Iterator ==================

RocksDBAllIndexIterator::RocksDBAllIndexIterator(LogicalCollection* col,
                                                 transaction::Methods* trx) 
    : IndexIterator(col, trx),
      _bounds(static_cast<RocksDBMetaCollection*>(col->getPhysical())->bounds()),
      _upperBound(_bounds.end()),
      _cmp(_bounds.columnFamily()->GetComparator()),
      _mustSeek(true) {
  // acquire rocksdb transaction
  auto* mthds = RocksDBTransactionState::toMethods(trx);

  rocksdb::ReadOptions ro = mthds->iteratorReadOptions();
  TRI_ASSERT(ro.snapshot != nullptr);
  TRI_ASSERT(ro.prefix_same_as_start);
  ro.fill_cache = AllIteratorFillBlockCache;
  ro.verify_checksums = false;  // TODO evaluate
  ro.iterate_upper_bound = &_upperBound;
  // options.readahead_size = 4 * 1024 * 1024;

  _iterator = mthds->NewIterator(ro, _bounds.columnFamily());
  TRI_ASSERT(_iterator);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::ColumnFamilyDescriptor desc;
  _bounds.columnFamily()->GetDescriptor(&desc);
  TRI_ASSERT(desc.options.prefix_extractor);
#endif
}

void RocksDBAllIndexIterator::seekIfRequired() {
  if (_mustSeek) {
    _iterator->Seek(_bounds.start());
    _mustSeek = false;
  }
}

bool RocksDBAllIndexIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

bool RocksDBAllIndexIterator::nextImpl(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
  seekIfRequired();

  if (limit == 0 || ADB_UNLIKELY(!_iterator->Valid()) || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
    return false;
  }

  TRI_ASSERT(limit > 0);

  while (limit > 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_bounds.objectId() == RocksDBKey::objectId(_iterator->key()));
#endif

    cb(RocksDBKey::documentId(_iterator->key()));
    --limit;
    _iterator->Next();

    if (ADB_UNLIKELY(!_iterator->Valid())) {
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());
      return false;
    } else if (outOfRange()) {
      return false;
    }
  }

  return true;
}

bool RocksDBAllIndexIterator::nextDocumentImpl(IndexIterator::DocumentCallback const& cb,
                                               size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());
  seekIfRequired();

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()), VPackSlice(reinterpret_cast<uint8_t const*>(_iterator->value().data())));
    --limit;
    _iterator->Next();

    if (ADB_UNLIKELY(!_iterator->Valid())) {
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());
      return false;
    } else if (outOfRange()) {
      return false;
    }
  }

  return true;
}

void RocksDBAllIndexIterator::skipImpl(uint64_t count, uint64_t& skipped) {
  TRI_ASSERT(_trx->state()->isRunning());
  seekIfRequired();

  while (count > 0 && _iterator->Valid()) {
    --count;
    ++skipped;
    _iterator->Next();
  }
    
  // validate that Iterator is in a good shape and hasn't failed
  arangodb::rocksutils::checkIteratorStatus(_iterator.get());
}

void RocksDBAllIndexIterator::resetImpl() {
  TRI_ASSERT(_trx->state()->isRunning());
  _mustSeek = true;
}

// ================ Any Iterator ================
RocksDBAnyIndexIterator::RocksDBAnyIndexIterator(LogicalCollection* col,
                                                 transaction::Methods* trx) 
    : IndexIterator(col, trx),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()),
      _objectId(static_cast<RocksDBMetaCollection*>(col->getPhysical())->objectId()),
      _bounds(static_cast<RocksDBMetaCollection*>(col->getPhysical())->bounds()),
      _total(0),
      _returned(0) {
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  auto options = mthds->iteratorReadOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AnyIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  _iterator = mthds->NewIterator(options, _bounds.columnFamily());
  TRI_ASSERT(_iterator);

  _total = col->numberDocuments(trx, transaction::CountType::Normal);
  _forward = RandomGenerator::interval(uint16_t(1)) ? true : false;
  reset();  // initial seek
}

bool RocksDBAnyIndexIterator::checkIter() {
  if (/* not  valid */ !_iterator->Valid() ||
      /* out of range forward */
      (_forward && _cmp->Compare(_iterator->key(), _bounds.end()) > 0) ||
      /* out of range backward */
      (!_forward && _cmp->Compare(_iterator->key(), _bounds.start()) < 0)) {
    if (_forward) {
      _iterator->Seek(_bounds.start());
    } else {
      _iterator->SeekForPrev(_bounds.end());
    }

    if (!_iterator->Valid()) {
      return false;
    }
  }
  return true;
}

bool RocksDBAnyIndexIterator::nextImpl(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()));
    --limit;
    _returned++;
    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());

      if (_returned < _total) {
        _iterator->Seek(_bounds.start());
        continue;
      }
      return false;
    }
  }
  return true;
}

bool RocksDBAnyIndexIterator::nextDocumentImpl(IndexIterator::DocumentCallback const& cb,
                                               size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(_iterator->key()), VPackSlice(reinterpret_cast<uint8_t const*>(_iterator->value().data())));
    --limit;
    _returned++;
    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iterator.get());
      if (_returned < _total) {
        _iterator->Seek(_bounds.start());
        continue;
      }
      return false;
    }
  }
  return true;
}

void RocksDBAnyIndexIterator::resetImpl() {
  // the assumption is that we don't reset this iterator unless
  // it is out of range or invalid
  if (_total == 0 || (_iterator->Valid() && !outOfRange())) {
    return;
  }
  uint64_t steps = RandomGenerator::interval(_total - 1) % 500;
  
  RocksDBKeyLeaser key(_trx);
  key->constructDocument(_objectId,
                         LocalDocumentId(RandomGenerator::interval(UINT64_MAX)));
  _iterator->Seek(key->string());

  if (checkIter()) {
    if (_forward) {
      while (steps-- > 0) {
        _iterator->Next();
        if (!checkIter()) {
          break;
        }
      }
    } else {
      while (steps-- > 0) {
        _iterator->Prev();
        if (!checkIter()) {
          break;
        }
      }
    }
  }
    
  // validate that Iterator is in a good shape and hasn't failed
  arangodb::rocksutils::checkIteratorStatus(_iterator.get());
}

bool RocksDBAnyIndexIterator::outOfRange() const {
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

RocksDBGenericIterator::RocksDBGenericIterator(rocksdb::TransactionDB* db,
                                               rocksdb::ReadOptions& options,
                                               RocksDBKeyBounds const& bounds)
    : _bounds(bounds),
      _options(options),
      _iterator(db->NewIterator(_options, _bounds.columnFamily())),
      _cmp(_bounds.columnFamily()->GetComparator()) {
  reset();
}

bool RocksDBGenericIterator::hasMore() const {
  return _iterator->Valid() && !outOfRange();
}

bool RocksDBGenericIterator::outOfRange() const {
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

bool RocksDBGenericIterator::reset() {
  return seek(_bounds.start());
}

bool RocksDBGenericIterator::skip(uint64_t count, uint64_t& skipped) {
  bool hasMore = _iterator->Valid();
  while (count > 0 && hasMore) {
    hasMore = next(
        [&count, &skipped](rocksdb::Slice const&, rocksdb::Slice const&) {
          --count;
          ++skipped;
          return true;
        },
        count /*gets copied*/);
  }
  return hasMore;
}

bool RocksDBGenericIterator::seek(rocksdb::Slice const& key) {
  _iterator->Seek(key);
  return hasMore();
}

bool RocksDBGenericIterator::next(GenericCallback const& cb, size_t limit) {
  // @params
  // limit - maximum number of documents

  TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
  if (limit == 0) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
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
    _iterator->Next();
    
    // validate that Iterator is in a good shape and hasn't failed
    arangodb::rocksutils::checkIteratorStatus(_iterator.get());
  }

  return hasMore();
}

RocksDBGenericIterator arangodb::createPrimaryIndexIterator(transaction::Methods* trx,
                                                            LogicalCollection* col) {
  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(trx != nullptr);

  auto* mthds = RocksDBTransactionState::toMethods(trx);

  rocksdb::ReadOptions options = mthds->iteratorReadOptions();
  TRI_ASSERT(options.snapshot != nullptr);  // trx must contain a valid snapshot
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = false;
  options.verify_checksums = false;

  auto index = col->lookupIndex(IndexId::primary());  // RocksDBCollection->primaryIndex() is private
  TRI_ASSERT(index->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  auto primaryIndex = static_cast<RocksDBPrimaryIndex*>(index.get());

  auto bounds(RocksDBKeyBounds::PrimaryIndex(primaryIndex->objectId()));
  auto& selector = col->vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  auto iterator = RocksDBGenericIterator(engine.db(), options, bounds);

  TRI_ASSERT(iterator.bounds().objectId() == primaryIndex->objectId());
  TRI_ASSERT(iterator.bounds().columnFamily() == RocksDBColumnFamily::primary());
  return iterator;
}
