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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationIterator.h"

#include <rocksdb/utilities/transaction_db.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb {

RocksDBRevisionReplicationIterator::RocksDBRevisionReplicationIterator(
    LogicalCollection& collection,
    std::shared_ptr<rocksdb::ManagedSnapshot> snapshot)
    : RevisionReplicationIterator(collection),
      _snapshot(std::move(snapshot)),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(collection.getPhysical())
              ->objectId())),
      _rangeBound(_bounds.end()) {
  auto& selector =
      collection.vocbase().server().getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = *static_cast<RocksDBEngine*>(&selector.engine());
  rocksdb::TransactionDB* db = engine.db();

  rocksdb::ReadOptions ro{};
  if (_snapshot) {
    ro.snapshot = _snapshot->snapshot();
  }

  ro.verify_checksums = false;
  ro.fill_cache = false;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &_rangeBound;

  rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
  _cmp = cf->GetComparator();

  _iter.reset(db->NewIterator(ro, cf));
  TRI_ASSERT(_iter != nullptr);
  if (_iter == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "unable to build RocksDBRevisionReplicationIterator for snapshot");
  }
  _iter->Seek(_bounds.start());
}

RocksDBRevisionReplicationIterator::RocksDBRevisionReplicationIterator(
    LogicalCollection& collection, transaction::Methods& trx)
    : RevisionReplicationIterator(collection),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(collection.getPhysical())
              ->objectId())),
      _rangeBound(_bounds.end()) {
  RocksDBTransactionMethods* methods =
      RocksDBTransactionState::toMethods(&trx, collection.id());

  rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
  _cmp = cf->GetComparator();

  _iter = methods->NewIterator(cf, [this](ReadOptions& ro) {
    ro.verify_checksums = false;
    ro.fill_cache = false;
    ro.prefix_same_as_start = true;
    ro.iterate_upper_bound = &_rangeBound;
    ro.readOwnWrites = false;
  });
  if (_iter == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "unable to build RocksDBRevisionReplicationIterator for transaction");
  }
  _iter->Seek(_bounds.start());
}

bool RocksDBRevisionReplicationIterator::hasMore() const {
  // checking the comparator is actually not necessary here,
  // because we have iterate_upper_bound set. Anyway, it does
  // no harm in production and adds another line of defense
  // in maintainer mode.
  TRI_ASSERT(!_iter->Valid() ||
             _cmp->Compare(_iter->key(), _bounds.end()) <= 0);
  return _iter->Valid();
}

void RocksDBRevisionReplicationIterator::reset() {
  _iter->Seek(_bounds.start());
}

RevisionId RocksDBRevisionReplicationIterator::revision() const {
  TRI_ASSERT(hasMore());
  return RevisionId{RocksDBKey::documentId(_iter->key())};
}

VPackSlice RocksDBRevisionReplicationIterator::document() const {
  TRI_ASSERT(hasMore());
  return RocksDBValue::data(_iter->value());
}

void RocksDBRevisionReplicationIterator::next() {
  TRI_ASSERT(hasMore());
  _iter->Next();
}

void RocksDBRevisionReplicationIterator::seek(RevisionId rid) {
  uint64_t objectId =
      static_cast<RocksDBCollection*>(_collection.getPhysical())->objectId();
  RocksDBKey key;
  key.constructDocument(objectId, LocalDocumentId::create(rid));
  _iter->Seek(key.string());
}

}  // namespace arangodb
