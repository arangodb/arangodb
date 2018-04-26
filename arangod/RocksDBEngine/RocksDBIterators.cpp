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

using namespace arangodb;

namespace {
constexpr bool AllIteratorFillBlockCache = true;
constexpr bool AnyIteratorFillBlockCache = false;
}

// ================ All Iterator ==================

RocksDBAllIndexIterator::RocksDBAllIndexIterator(
    LogicalCollection* col, transaction::Methods* trx, RocksDBPrimaryIndex const* index)
    : IndexIterator(col, trx, index),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(col->getPhysical())->objectId())),
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

    cb(RocksDBKey::documentId(RocksDBEntryType::Document, _iterator->key()));
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

  if (limit == 0 || !_iterator->Valid()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(RocksDBKey::documentId(RocksDBEntryType::Document, _iterator->key()),
       VPackSlice(_iterator->value().data()));
    --limit;
    _iterator->Next();
    
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
    : IndexIterator(col, trx, index),
      _cmp(RocksDBColumnFamily::documents()->GetComparator()),
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
  TRI_ASSERT(_iterator);

  _total = col->numberDocuments(trx);
  _forward = RandomGenerator::interval(uint16_t(1)) ? true : false;

  //initial seek
  if (_total > 0) {
    uint64_t steps = RandomGenerator::interval(_total - 1) % 500;
    auto initialKey = RocksDBKey();
    initialKey.constructDocument(
      static_cast<RocksDBCollection*>(col->getPhysical())->objectId(),
      LocalDocumentId(RandomGenerator::interval(UINT64_MAX))
    );
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
}

bool RocksDBAnyIndexIterator::checkIter(){
  if ( /* not  valid */            !_iterator->Valid() ||
       /* out of range forward */  ( _forward && _cmp->Compare(_iterator->key(), _bounds.end())   > 0) ||
       /* out of range backward */ (!_forward && _cmp->Compare(_iterator->key(), _bounds.start()) < 0)  ) {

    if (_forward) {
      _iterator->Seek(_bounds.start());
    } else {
      _iterator->SeekForPrev(_bounds.end());
    }

    if(!_iterator->Valid()){
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
    cb(RocksDBKey::documentId(RocksDBEntryType::Document, _iterator->key()));
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
    cb(RocksDBKey::documentId(RocksDBEntryType::Document, _iterator->key()),
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

void RocksDBAnyIndexIterator::reset() { _iterator->Seek(_bounds.start()); }

bool RocksDBAnyIndexIterator::outOfRange() const {
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

// ================ Sorted All Iterator ==================

RocksDBSortedAllIterator::RocksDBSortedAllIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    RocksDBPrimaryIndex const* index)
    : IndexIterator(collection, trx, index),
      _trx(trx),
      _bounds(RocksDBKeyBounds::PrimaryIndex(index->objectId())),
      _cmp(index->comparator()) {

  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
  // intentional copy of the read options
  auto options = mthds->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = false; // only used for incremental sync
  options.verify_checksums = false;
  _iterator = mthds->NewIterator(options, index->columnFamily());
  TRI_ASSERT(_iterator);
  _iterator->Seek(_bounds.start());
  TRI_ASSERT(index->columnFamily() == RocksDBColumnFamily::primary());
}

bool RocksDBSortedAllIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

bool RocksDBSortedAllIterator::next(LocalDocumentIdCallback const& cb, size_t limit) {
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(RocksDBValue::documentId(_iterator->value()));

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

void RocksDBGenericAllIndexIterator::seek(StringRef const& key) {
  TRI_ASSERT(_trx->state()->isRunning());
  // don't want to get the index pointer just for this
  uint64_t objectId = _bounds.objectId();
  RocksDBKeyLeaser val(_trx);
  val->constructPrimaryIndexValue(objectId, key);
  _iterator->Seek(val->string());
  TRI_ASSERT(_iterator->Valid());
}

bool RocksDBGenericAllIndexIterator::outOfRange() const {
  TRI_ASSERT(_trx->state()->isRunning());
  return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
}

void RocksDBGenericAllIndexIterator::reset() {
  TRI_ASSERT(_trx->state()->isRunning());
    _iterator->Seek(_bounds.start());
}

bool RocksDBGenericAllIndexIterator::gnext(GenericCallback const& cb, size_t limit){
  TRI_ASSERT(_trx->state()->isRunning());

  if (limit == 0 || !_iterator->Valid() || outOfRange()) {
    // No limit no data, or we are actually done. The last call should have
    // returned false
    TRI_ASSERT(limit > 0);  // Someone called with limit == 0. Api broken
    return false;
  }

  while (limit > 0) {
    cb(_iterator->key(), _iterator->value());
    --limit;

    _iterator->Next();

    if (!_iterator->Valid() || outOfRange()) {
      return false;
    }
  }

  return true;
}

RocksDBGenericIterator::RocksDBGenericIterator(rocksdb::TransactionDB* db
                                              ,rocksdb::ColumnFamilyHandle* columnFamily
                                              ,rocksdb::ReadOptions& options
                                              ,RocksDBKeyBounds const& bounds
                                              ,bool reverse)
    : _reverse(reverse)
    , _bounds(bounds)
    , _iterator(db->NewIterator(options, columnFamily))
    , _cmp(columnFamily->GetComparator())
  {};

  //return std::unique_ptr<rocksdb::Iterator>(_db->NewIterator(opts, cf));
bool RocksDBGenericIterator::outOfRange() const {
  if (_reverse) {
    return _cmp->Compare(_iterator->key(), _bounds.start()) < 0;
  } else {
    return _cmp->Compare(_iterator->key(), _bounds.end()) > 0;
  }
}

void RocksDBGenericIterator::reset() {
  if (_reverse) {
    _iterator->SeekForPrev(_bounds.end());
  } else {
    _iterator->Seek(_bounds.start());
  }
}

void RocksDBGenericIterator::skip(uint64_t count, uint64_t& skipped) {
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

void RocksDBGenericIterator::seek(StringRef const& key) {
  uint64_t objectId = _bounds.objectId();
  RocksDBKey primaryKey;
  primaryKey.constructPrimaryIndexValue(objectId, key);
  _iterator->Seek(primaryKey.string());
  TRI_ASSERT(_iterator->Valid());
}
bool RocksDBGenericIterator::next(GenericCallback const& cb, size_t limit) {

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

    cb(_iterator->key(),_iterator->value());
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

RocksDBGenericIterator arangodb::createGenericIterator(transaction::Methods* trx
                                            ,rocksdb::ColumnFamilyHandle* columnFamily
                                            ,LogicalCollection* col
                                            ,bool reverse
                                            ){
  auto* mthds = RocksDBTransactionState::toMethods(trx);

  // intentional copy of the read options
  rocksdb::ReadOptions options = mthds->readOptions();
  TRI_ASSERT(options.snapshot != nullptr);
  TRI_ASSERT(options.prefix_same_as_start);
  options.fill_cache = AllIteratorFillBlockCache;
  options.verify_checksums = false;  // TODO evaluate
  // options.readahead_size = 4 * 1024 * 1024;
  RocksDBKeyBounds bounds(RocksDBKeyBounds::Empty());

  // TODO FIXME - more create functions or kind of switch
  // over columenfamiles or create functions that takes bounds..
  // ask jan
  if(columnFamily == RocksDBColumnFamily::documents() && col){
    bounds = RocksDBKeyBounds::CollectionDocuments(static_cast<RocksDBCollection*>(col->getPhysical())->objectId());
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL); //result? - guess exception is ok here as it should not happen
  }

  return RocksDBGenericIterator(arangodb::rocksutils::globalRocksDB(), columnFamily, options, bounds, reverse);

}

