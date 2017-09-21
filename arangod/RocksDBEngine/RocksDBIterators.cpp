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
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

using namespace arangodb;

namespace {
constexpr bool AllIteratorFillBlockCache = true;
constexpr bool AnyIteratorFillBlockCache = false;
}

// ================ All Iterator ==================

RocksDBAllIndexIterator::RocksDBAllIndexIterator(
    LogicalCollection* col, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex const* index, bool reverse)
    : IndexIterator(col, trx, mmdr, index),
      _reverse(reverse),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(col->getPhysical())->objectId())),
      _iterator(),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()) {
  // acquire rocksdb transaction
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  rocksdb::ColumnFamilyHandle* cf = RocksDBColumnFamily::documents();

  // intentional copy of the read options
  rocksdb::ReadOptions options = mthds->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AllIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  // options.readahead_size = 4 * 1024 * 1024;
  _iterator = mthds->NewIterator(options, cf);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  rocksdb::ColumnFamilyDescriptor desc;
  cf->GetDescriptor(&desc);
  TRI_ASSERT(desc.options.prefix_extractor);
#endif

  if (reverse) {
    _iterator->SeekForPrev(_bounds.end());
  } else {
    _iterator->Seek(_bounds.start());
  }
}

bool RocksDBAllIndexIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  if (_reverse) {
    return _cmp->Compare(_iterator->key(), _bounds.start()) < 0;
  } else {
    return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
  }
}

bool RocksDBAllIndexIterator::next(TokenCallback const& cb, size_t limit) {
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

    TRI_voc_rid_t revisionId =
        RocksDBKey::revisionId(RocksDBEntryType::Document, _iterator->key());
    cb(RocksDBToken(revisionId));

    --limit;
    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }

  return true;
}

bool RocksDBAllIndexIterator::nextDocument(
    IndexIterator::DocumentCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    TRI_voc_rid_t revisionId = RocksDBKey::revisionId(RocksDBEntryType::Document, _iterator->key());
    cb(RocksDBToken(revisionId), VPackSlice(_iterator->value().data()));
    --limit;

    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }

    if (!_iterator->Valid()) {
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

    if (_reverse) {
      _iterator->Prev();
    } else {
      _iterator->Next();
    }
  }
}

void RocksDBAllIndexIterator::reset() {
  TRI_ASSERT(_trx->state()->isRunning());

  if (_reverse) {
    _iterator->SeekForPrev(_bounds.end());
  } else {
    _iterator->Seek(_bounds.start());
  }
}

// ================ Any Iterator ================

RocksDBAnyIndexIterator::RocksDBAnyIndexIterator(
    LogicalCollection* col, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex const* index)
    : IndexIterator(col, trx, mmdr, index),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()),
      _iterator(),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(col->getPhysical())->objectId())),
      _total(0),
      _returned(0) {
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  // intentional copy of the read options
  auto options = mthds->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AnyIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  _iterator = mthds->NewIterator(options, RocksDBColumnFamily::documents());

  _total = col->numberDocuments(trx);
  uint64_t off = RandomGenerator::interval(_total - 1);
  if (_total > 0) {
    if (off <= _total / 2) {
      _iterator->Seek(_bounds.start());
      while (_iterator->Valid() && off-- > 0) {
        _iterator->Next();
      }
    } else {
      off = _total - (off + 1);
      _iterator->SeekForPrev(_bounds.end());
      while (_iterator->Valid() && off-- > 0) {
        _iterator->Prev();
      }
    }
    if (!_iterator->Valid() || outOfRange()) {
      _iterator->Seek(_bounds.start());
    }
  }
}

bool RocksDBAnyIndexIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    TRI_voc_rid_t revisionId =
        RocksDBKey::revisionId(RocksDBEntryType::Document, _iterator->key());
    cb(RocksDBToken(revisionId));
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
    TRI_voc_rid_t revisionId = RocksDBKey::revisionId(RocksDBEntryType::Document, _iterator->key());
    cb(RocksDBToken(revisionId), VPackSlice(_iterator->value().data()));
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

void RocksDBAnyIndexIterator::reset() { _iterator->Seek(_bounds.start()); }

bool RocksDBAnyIndexIterator::outOfRange() const {
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

// ================ Sorted All Iterator ==================

RocksDBSortedAllIterator::RocksDBSortedAllIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBPrimaryIndex const* index)
    : IndexIterator(collection, trx, mmdr, index),
      _trx(trx),
      _bounds(RocksDBKeyBounds::PrimaryIndex(index->objectId())),
      _iterator(),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _index(index),
#endif
      _cmp(index->comparator()) {
 
  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
  // intentional copy of the read options
  auto options = mthds->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = false; // only used for incremental sync
  options.verify_checksums = false;
  _iterator = mthds->NewIterator(options, index->columnFamily());
  _iterator->Seek(_bounds.start());
  TRI_ASSERT(index->columnFamily() == RocksDBColumnFamily::primary());
}

bool RocksDBSortedAllIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

bool RocksDBSortedAllIterator::next(TokenCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    RocksDBToken token(RocksDBValue::revisionId(_iterator->value()));
    cb(token);

    --limit;

    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }

  return true;
}

/// special method to expose the document key for incremental replication
bool RocksDBSortedAllIterator::nextWithKey(TokenKeyCallback const& cb,
                                           size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_index->objectId() == RocksDBKey::objectId(_iterator->key()));
#endif
    RocksDBToken token(RocksDBValue::revisionId(_iterator->value()));
    StringRef key = RocksDBKey::primaryKey(_iterator->key());
    cb(token, key);
    --limit;

    _iterator->Next();
    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }
  return true;
}

void RocksDBSortedAllIterator::seek(StringRef const& key) {
  TRI_ASSERT(_trx->state()->isRunning());
  // don't want to get the index pointer just for this
  uint64_t objectId = _bounds.objectId();
  RocksDBKeyLeaser val(_trx);
  val->constructPrimaryIndexValue(objectId, key);
  _iterator->Seek(val->string());
  TRI_ASSERT(_iterator->Valid());
}

void RocksDBSortedAllIterator::reset() {
  TRI_ASSERT(_trx->state()->isRunning());
  _iterator->Seek(_bounds.start());
}
