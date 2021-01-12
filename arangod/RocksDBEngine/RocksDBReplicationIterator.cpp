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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationIterator.h"

#include <rocksdb/utilities/transaction_db.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb {

RocksDBRevisionReplicationIterator::RocksDBRevisionReplicationIterator(
    LogicalCollection& collection, rocksdb::Snapshot const* snapshot)
    : RevisionReplicationIterator(collection),
      _readOptions(),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(collection.getPhysical())->objectId())) {
  auto& selector = collection.vocbase().server().getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = *static_cast<RocksDBEngine*>(&selector.engine());
  rocksdb::TransactionDB* db = engine.db();

  if (snapshot) {
    _readOptions.snapshot = snapshot;
  }

  _readOptions.verify_checksums = false;
  _readOptions.fill_cache = false;
  _readOptions.prefix_same_as_start = true;

  rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
  _cmp = cf->GetComparator();

  _iter.reset(db->NewIterator(_readOptions, cf));
  _iter->Seek(_bounds.start());
}

RocksDBRevisionReplicationIterator::RocksDBRevisionReplicationIterator(
    LogicalCollection& collection, transaction::Methods& trx)
    : RevisionReplicationIterator(collection),
      _readOptions(),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(collection.getPhysical())->objectId())) {
  RocksDBMethods* methods = RocksDBTransactionState::toMethods(&trx);
  _readOptions = methods->iteratorReadOptions();

  _readOptions.verify_checksums = false;
  _readOptions.fill_cache = false;
  _readOptions.prefix_same_as_start = true;

  rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
  _cmp = cf->GetComparator();

  _iter = methods->NewIterator(_readOptions, cf);
  _iter->Seek(_bounds.start());
}

bool RocksDBRevisionReplicationIterator::hasMore() const {
  return _iter->Valid() && _cmp->Compare(_iter->key(), _bounds.end()) <= 0;
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
