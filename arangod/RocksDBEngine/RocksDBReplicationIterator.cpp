////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBValue.h"
#include "StorageEngine/EngineSelectorFeature.h"

namespace arangodb {

RocksDBRevisionReplicationIterator::RocksDBRevisionReplicationIterator(
    LogicalCollection& collection, rocksdb::Snapshot const* snapshot)
    : RevisionReplicationIterator(collection),
      _readOptions(),
      _bounds(RocksDBKeyBounds::CollectionDocuments(
          static_cast<RocksDBCollection*>(collection.getPhysical())->objectId())) {
  TRI_ASSERT(snapshot != nullptr);

  _readOptions.snapshot = snapshot;
  _readOptions.verify_checksums = false;
  _readOptions.fill_cache = false;
  _readOptions.prefix_same_as_start = true;

  auto& selector = collection.vocbase().server().getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = *static_cast<RocksDBEngine*>(&selector.engine());
  rocksdb::TransactionDB* db = engine.db();
  rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
  _cmp = cf->GetComparator();

  _iter.reset(db->NewIterator(_readOptions, cf));
  _iter->Seek(_bounds.start());
}

void RocksDBRevisionReplicationIterator::reset() {
  _iter->Seek(_bounds.start());
}

bool RocksDBRevisionReplicationIterator::hasMore() const {
  return _iter->Valid() && _cmp->Compare(_iter->key(), _bounds.end()) <= 0;
}

TRI_voc_rid_t RocksDBRevisionReplicationIterator::revision() const {
  if (hasMore()) {
    return RocksDBKey::documentId(_iter->key()).id();
  }
  // iterator invalid, return garbage
  return 0;
}

bool RocksDBRevisionReplicationIterator::next(RevisionCallback const& callback) {
  if (!hasMore()) {
    return false;
  }
  TRI_voc_rid_t rid = RocksDBKey::documentId(_iter->key()).id();
  return callback(rid);
}

bool RocksDBRevisionReplicationIterator::nextDocument(DocumentCallback const& callback) {
  if (!hasMore()) {
    return false;
  }
  TRI_voc_rid_t rid = RocksDBKey::documentId(_iter->key()).id();
  VPackSlice doc = RocksDBValue::data(_iter->value());
  return callback(rid, doc);
}

void RocksDBRevisionReplicationIterator::seek(TRI_voc_rid_t rid) {
  uint64_t objectId =
      static_cast<RocksDBCollection*>(_collection.getPhysical())->objectId();
  RocksDBKey key;
  key.constructDocument(objectId, LocalDocumentId::create(rid));
  _iter->Seek(key.string());
}

}  // namespace arangodb
