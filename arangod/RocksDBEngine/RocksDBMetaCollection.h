////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#pragma once
#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_META_COLLECTION_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_META_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "RocksDBEngine/RocksDBMetadata.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

class RocksDBMetaCollection : public PhysicalCollection {
 public:
  explicit RocksDBMetaCollection(LogicalCollection& collection,
                                 arangodb::velocypack::Slice const& info);
  RocksDBMetaCollection(LogicalCollection& collection,
                        PhysicalCollection const*);  // use in cluster only!!!!!
  virtual ~RocksDBMetaCollection() {}
  
  std::string const& path() const override final;
  void setPath(std::string const& path) override final {}
  arangodb::Result persistProperties() override final {
    // only code path calling this causes these properties to be
    // already written in RocksDBEngine::changeCollection()
    return Result();
  }
  void open(bool /*ignoreErrors*/) override final {
    TRI_ASSERT(_objectId != 0);
  }
  
  void deferDropCollection(std::function<bool(LogicalCollection&)> const&) override final  {}
  
  /// @brief report extra memory used by indexes etc.
  size_t memory() const override final { return 0; }
  uint64_t objectId() const { return _objectId; }
  
  RocksDBMetadata& meta() { return _meta; }
  
  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override final;
  uint64_t numberDocuments(transaction::Methods* trx) const override final;
  
  void invokeOnAllElements(transaction::Methods* trx,
                           std::function<bool(LocalDocumentId const&)> callback) override final;
  
  int lockWrite(double timeout = 0.0);
  void unlockWrite();
  int lockRead(double timeout = 0.0);
  void unlockRead();
  
  /// recalculte counts for collection in case of failure
  uint64_t recalculateCounts();
  
  /// @brief compact-data operation
  /// triggers rocksdb compaction for documentDB and indexes
  Result compact() override final;
  
  /// estimate size of collection and indexes
  void estimateSize(velocypack::Builder& builder);

 public:
  
  /// return bounds for all documents
  virtual RocksDBKeyBounds bounds() const = 0;
  
 protected:
  
  /// @brief track the usage of waitForSync option in an operation
  void trackWaitForSync(arangodb::transaction::Methods* trx, OperationOptions& options);
  
 protected:
  RocksDBMetadata _meta;  /// collection metadata
  uint64_t const _objectId; /// rocksdb-specific object id for collection
  /// @brief collection lock used for write access
  mutable basics::ReadWriteLock _exclusiveLock;
};

} // namespace arangodb
  
#endif
