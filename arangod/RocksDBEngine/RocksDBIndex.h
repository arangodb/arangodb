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

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"

#include <rocksdb/status.h>

namespace rocksdb {
class Comparator;
class ColumnFamilyHandle;
}
namespace arangodb {
namespace cache {
class Cache;
}
class LogicalCollection;
class RocksDBSettingsManager;
class RocksDBMethods;

class RocksDBIndex : public Index {
 protected:
  // This is the number of distinct elements the index estimator can reliably
  // store
  // This correlates directly with the memory of the estimator:
  // memory == ESTIMATOR_SIZE * 6 bytes
  static uint64_t const ESTIMATOR_SIZE;

 public:
  ~RocksDBIndex();
  void toVelocyPackFigures(VPackBuilder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type) const override;

  uint64_t objectId() const { return _objectId; }

  bool isPersistent() const override final { return true; }

  Result drop() override;

  Result insert(
      transaction::Methods& trx,
      LocalDocumentId const& documentId,
      velocypack::Slice const& doc,
      Index::OperationMode mode
  ) override {
    auto mthds = RocksDBTransactionState::toMethods(&trx);
    return insertInternal(trx, mthds, documentId, doc, mode);
  }

  Result remove(
      transaction::Methods& trx,
      LocalDocumentId const& documentId,
      arangodb::velocypack::Slice const& doc,
      Index::OperationMode mode
  ) override {
    auto mthds = RocksDBTransactionState::toMethods(&trx);
    return removeInternal(trx, mthds, documentId, doc, mode);
  }

  virtual void afterTruncate(TRI_voc_tick_t tick) override;

  void load() override;
  void unload() override;

  size_t memory() const override;

  void cleanup();

  /// @brief provides a size hint for the index
  Result sizeHint(
      transaction::Methods& /*trx*/,
      size_t /*size*/
  ) override final {
    return Result(); // nothing to do here
  }

  void setCacheEnabled(bool enable) {
    // allow disabling and enabling of caches for the primary index
    _cacheEnabled = enable;
  }
  void createCache();
  void destroyCache();

  /// insert index elements into the specified write batch.
  virtual Result insertInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    Index::OperationMode mode
  ) = 0;

  /// remove index elements and put it in the specified write batch.
  virtual Result removeInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& documentId,
    arangodb::velocypack::Slice const& doc,
    Index::OperationMode mode
  ) = 0;

  virtual Result updateInternal(
    transaction::Methods& trx,
    RocksDBMethods* methods,
    LocalDocumentId const& oldDocumentId,
    arangodb::velocypack::Slice const& oldDoc,
    LocalDocumentId const& newDocumentId,
    velocypack::Slice const& newDoc,
    Index::OperationMode mode
  );

  rocksdb::ColumnFamilyHandle* columnFamily() const { return _cf; }

  rocksdb::Comparator const* comparator() const;

  RocksDBKeyBounds getBounds() const {
    return RocksDBIndex::getBounds(type(), _objectId, _unique);
  };

  static RocksDBKeyBounds getBounds(Index::IndexType type, uint64_t objectId,
                                    bool unique);

  virtual RocksDBCuckooIndexEstimator<uint64_t>* estimator();
  virtual void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>);
  virtual void recalculateEstimates() {}

 protected:
  RocksDBIndex(
    TRI_idx_iid_t id,
    LogicalCollection& collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique,
    bool sparse,
    rocksdb::ColumnFamilyHandle* cf,
    uint64_t objectId,
    bool useCache
  );

  RocksDBIndex(
    TRI_idx_iid_t id,
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info,
    rocksdb::ColumnFamilyHandle* cf,
    bool useCache
  );

  inline bool useCache() const { return (_cacheEnabled && _cachePresent); }
  void blackListKey(char const* data, std::size_t len);
  void blackListKey(StringRef& ref) { blackListKey(ref.data(), ref.size()); };

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