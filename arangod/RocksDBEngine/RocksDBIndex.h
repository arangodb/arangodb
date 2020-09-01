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
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/Identifiers/IndexId.h"

namespace rocksdb {
class Comparator;
class ColumnFamilyHandle;
}  // namespace rocksdb
namespace arangodb {
namespace cache {
class Cache;
}
class LogicalCollection;
class RocksDBMethods;
struct OperationOptions;

class RocksDBIndex : public Index {
 protected:
  
  // This is the number of distinct elements the index estimator can reliably
  // store
  // This correlates directly with the memory of the estimator:
  // memory == ESTIMATOR_SIZE * 6 bytes
  static constexpr uint64_t ESTIMATOR_SIZE = 4096;

 public:
  ~RocksDBIndex();
  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(velocypack::Builder& builder,
                    std::underlying_type<Index::Serialize>::type) const override;

  uint64_t objectId() const { return _objectId.load(); }
  uint64_t tempObjectId() const { return _tempObjectId.load(); }

  /// @brief if true this index should not be shown externally
  virtual bool isHidden() const override {
    return false;  // do not generally hide indexes
  }

  size_t memory() const override;

  Result drop() override;

  virtual void afterTruncate(TRI_voc_tick_t tick,
                             transaction::Methods* trx) override;

  void load() override;
  void unload() override;

  /// compact the index, should reduce read amplification
  void compact();

  void setCacheEnabled(bool enable) {
    // allow disabling and enabling of caches for the primary index
    _cacheEnabled = enable;
  }
  void createCache();
  void destroyCache();

  /// insert index elements into the specified write batch.
  virtual Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const& doc,
                        OperationOptions& options) = 0;

  /// remove index elements and put it in the specified write batch.
  virtual Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice const& doc,
                        Index::OperationMode mode) = 0;

  virtual Result update(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& oldDocumentId,
                        arangodb::velocypack::Slice const& oldDoc,
                        LocalDocumentId const& newDocumentId,
                        velocypack::Slice const& newDoc, Index::OperationMode mode);

  rocksdb::ColumnFamilyHandle* columnFamily() const { return _cf; }

  rocksdb::Comparator const* comparator() const;

  RocksDBKeyBounds getBounds(std::uint64_t objectId) const {
    return RocksDBIndex::getBounds(type(), objectId, _unique);
  }

  RocksDBKeyBounds getBounds() const { return getBounds(_objectId); }

  static RocksDBKeyBounds getBounds(Index::IndexType type, uint64_t objectId, bool unique);

  /// @brief get index estimator, optional
  virtual RocksDBCuckooIndexEstimator<uint64_t>* estimator() { return nullptr; }
  virtual void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimator<uint64_t>>) {}
  virtual void recalculateEstimates() {}

  bool isPersistent() const override final { return true; }

  Result setObjectIds(std::uint64_t plannedObjectId, std::uint64_t plannedTempObjectId);

 protected:
  RocksDBIndex(IndexId id, LogicalCollection& collection, std::string const& name,
               std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
               bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
               uint64_t objectId, uint64_t tempObjectId, bool useCache);

  RocksDBIndex(IndexId id, LogicalCollection& collection,
               arangodb::velocypack::Slice const& info,
               rocksdb::ColumnFamilyHandle* cf, bool useCache);

  inline bool useCache() const { return (_cacheEnabled && _cache); }
  void blackListKey(char const* data, std::size_t len);
  void blackListKey(arangodb::velocypack::StringRef& ref) {
    blackListKey(ref.data(), ref.size());
  };

 protected:
  rocksdb::ColumnFamilyHandle* _cf;

  mutable std::shared_ptr<cache::Cache> _cache;
  bool _cacheEnabled;

 private:
  std::atomic<uint64_t> _objectId;
  std::atomic<uint64_t> _tempObjectId;
};
}  // namespace arangodb

#endif
