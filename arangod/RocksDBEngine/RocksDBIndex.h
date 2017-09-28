////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_INDEX_H 1

#include <rocksdb/status.h>
#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

namespace rocksdb {
class Comparator;
class ColumnFamilyHandle;
}
namespace arangodb {
namespace cache {
class Cache;
}
class LogicalCollection;
class RocksDBCounterManager;
class RocksDBMethods;

class RocksDBIndex : public Index {
 protected:
  // This is the number of distinct elements the index estimator can reliably
  // store
  // This correlates directly with the memory of the estimator:
  // memory == ESTIMATOR_SIZE * 6 bytes
  static uint64_t const ESTIMATOR_SIZE;

 protected:
  RocksDBIndex(TRI_idx_iid_t, LogicalCollection*,
               std::vector<std::vector<arangodb::basics::AttributeName>> const&
                   attributes,
               bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
               uint64_t objectId, bool useCache);

  RocksDBIndex(TRI_idx_iid_t, LogicalCollection*,
               arangodb::velocypack::Slice const&,
               rocksdb::ColumnFamilyHandle* cf, bool useCache);

 public:
  ~RocksDBIndex();
  void toVelocyPackFigures(VPackBuilder& builder) const override;
  
  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder, bool withFigures,
                    bool forPersistence) const override;

  uint64_t objectId() const { return _objectId; }

  bool isPersistent() const override final { return true; }

  int drop() override;

  void load() override;
  void unload() override;

  virtual void truncate(transaction::Methods*);

  size_t memory() const override;

  void cleanup();

  /// @brief provides a size hint for the index
  int sizeHint(transaction::Methods* /*trx*/, size_t /*size*/) override final {
    // nothing to do here
    return TRI_ERROR_NO_ERROR;
  }

  Result insert(transaction::Methods* trx, TRI_voc_rid_t rid,
                velocypack::Slice const& doc, bool) override {
    auto mthds = RocksDBTransactionState::toMethods(trx);
    return insertInternal(trx, mthds, rid, doc);
  }

  Result remove(transaction::Methods* trx, TRI_voc_rid_t rid,
                arangodb::velocypack::Slice const& doc, bool) override {
    auto mthds = RocksDBTransactionState::toMethods(trx);
    return removeInternal(trx, mthds, rid, doc);
  }

  void setCacheEnabled(bool enable) {
    // allow disabling and enabling of caches for the primary index
    _cacheEnabled = enable;
  }
  void createCache();
  void destroyCache();

  virtual void serializeEstimate(std::string& output) const;

  virtual bool deserializeEstimate(RocksDBCounterManager* mgr);

  virtual void recalculateEstimates();

  /// insert index elements into the specified write batch.
  virtual Result insertInternal(transaction::Methods* trx, RocksDBMethods*,
                                TRI_voc_rid_t,
                                arangodb::velocypack::Slice const&) = 0;
  
  virtual Result updateInternal(transaction::Methods* trx, RocksDBMethods*,
                                TRI_voc_rid_t oldRevision,
                                arangodb::velocypack::Slice const& oldDoc,
                                TRI_voc_rid_t newRevision,
                                velocypack::Slice const& newDoc);

  /// remove index elements and put it in the specified write batch.
  virtual Result removeInternal(transaction::Methods* trx, RocksDBMethods*,
                                TRI_voc_rid_t,
                                arangodb::velocypack::Slice const&) = 0;

  rocksdb::ColumnFamilyHandle* columnFamily() const { return _cf; }

  rocksdb::Comparator const* comparator() const;
  
  RocksDBKeyBounds getBounds() const {
    return RocksDBIndex::getBounds(type(), _objectId, _unique);
  };

  static RocksDBKeyBounds getBounds(Index::IndexType type, uint64_t objectId,
                                    bool unique);

 protected:
  // Will be called during truncate to allow the index to update selectivity
  // estimates, blacklist keys, etc.
  virtual Result postprocessRemove(transaction::Methods* trx,
                                   rocksdb::Slice const& key,
                                   rocksdb::Slice const& value);

  inline bool useCache() const { return (_cacheEnabled && _cachePresent); }
  void blackListKey(char const* data, std::size_t len);
  void blackListKey(StringRef& ref) { blackListKey(ref.data(), ref.size()); };

 protected:
  uint64_t _objectId;
  rocksdb::ColumnFamilyHandle* _cf;

  mutable std::shared_ptr<cache::Cache> _cache;
  // we use this boolean for testing whether _cache is set.
  // it's quicker than accessing the shared_ptr each time
  bool _cachePresent;
  bool _cacheEnabled;
};
}  // namespace arangodb

#endif
