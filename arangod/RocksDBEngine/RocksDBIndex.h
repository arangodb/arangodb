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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "VocBase/Identifiers/IndexId.h"

#include <rocksdb/status.h>
#include <rocksdb/slice.h>

namespace rocksdb {
class Comparator;
class ColumnFamilyHandle;
}  // namespace rocksdb

namespace arangodb {
namespace cache {
class Cache;
class Manager;
}  // namespace cache

class LogicalCollection;
class RocksDBEngine;
class RocksDBMethods;
struct OperationOptions;

class RocksDBIndex : public Index {
 protected:
  // This is the number of distinct elements the index estimator can reliably
  // store
  // This correlates directly with the memory of the estimator:
  // memory == ESTIMATOR_SIZE * 6 bytes.
  // note: if this is ever adjusted, it will break the stored estimator data!
  static constexpr uint64_t ESTIMATOR_SIZE = 4096;

 public:
  ~RocksDBIndex();
  void toVelocyPackFigures(velocypack::Builder& builder) const override;

  /// @brief return a VelocyPack representation of the index
  void toVelocyPack(
      velocypack::Builder& builder,
      std::underlying_type<Index::Serialize>::type) const override;

  uint64_t objectId() const noexcept { return _objectId; }

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

  void setupCache();
  void destroyCache();

  /// performs a preflight check for an insert operation, not carrying out any
  /// modifications to the index.
  /// the default implementation does nothing. indexes can override this and
  /// perform useful checks (uniqueness checks etc.) here
  virtual Result checkInsert(transaction::Methods& trx, RocksDBMethods* methods,
                             LocalDocumentId const& documentId,
                             arangodb::velocypack::Slice doc,
                             OperationOptions const& options);

  /// performs a preflight check for an update/replace operation, not carrying
  /// out any modifications to the index. the default implementation does
  /// nothing. indexes can override this and perform useful checks (uniqueness
  /// checks etc.) here
  virtual Result checkReplace(transaction::Methods& trx,
                              RocksDBMethods* methods,
                              LocalDocumentId const& documentId,
                              arangodb::velocypack::Slice doc,
                              OperationOptions const& options);

  /// insert index elements into the specified write batch.
  virtual Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice doc,
                        OperationOptions const& options,
                        bool performChecks) = 0;

  /// remove index elements and put it in the specified write batch.
  virtual Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& documentId,
                        arangodb::velocypack::Slice doc) = 0;

  virtual Result update(transaction::Methods& trx, RocksDBMethods* methods,
                        LocalDocumentId const& oldDocumentId,
                        arangodb::velocypack::Slice oldDoc,
                        LocalDocumentId const& newDocumentId,
                        velocypack::Slice newDoc,
                        OperationOptions const& options, bool performChecks);

  rocksdb::ColumnFamilyHandle* columnFamily() const { return _cf; }

  rocksdb::Comparator const* comparator() const;

  RocksDBKeyBounds getBounds(std::uint64_t objectId) const {
    return RocksDBIndex::getBounds(type(), objectId, _unique);
  }

  RocksDBKeyBounds getBounds() const { return getBounds(_objectId); }

  static RocksDBKeyBounds getBounds(Index::IndexType type, uint64_t objectId,
                                    bool unique);

  /// @brief get index estimator, optional
  virtual RocksDBCuckooIndexEstimatorType* estimator();
  virtual void setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>);
  virtual void recalculateEstimates();

 protected:
  RocksDBIndex(IndexId id, LogicalCollection& collection,
               std::string const& name,
               std::vector<std::vector<arangodb::basics::AttributeName>> const&
                   attributes,
               bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
               uint64_t objectId, bool useCache, cache::Manager* cacheManager,
               RocksDBEngine& engine);

  RocksDBIndex(IndexId id, LogicalCollection& collection,
               arangodb::velocypack::Slice info,
               rocksdb::ColumnFamilyHandle* cf, bool useCache,
               cache::Manager* cacheManager, RocksDBEngine& engine);

  inline bool hasCache() const noexcept {
    return _cacheEnabled && (_cache != nullptr);
  }

  virtual std::shared_ptr<cache::Cache> makeCache() const;

  void invalidateCacheEntry(char const* data, std::size_t len);

  void invalidateCacheEntry(std::string_view ref) {
    invalidateCacheEntry(ref.data(), ref.size());
  }

  void invalidateCacheEntry(rocksdb::Slice ref) {
    invalidateCacheEntry(ref.data(), ref.size());
  }

  rocksdb::ColumnFamilyHandle* _cf;

  // we have to store the cacheManager's pointer here because the
  // vocbase might already be destroyed at the time the destructor is executed
  cache::Manager* _cacheManager;

  // user-side request for caching. will effectively be followed only if
  // _cacheManager != nullptr.
  bool _cacheEnabled;

  // the actual cache object. can be a nullptr, and can only be set if
  // _cacheManager != nullptr.
  mutable std::shared_ptr<cache::Cache> _cache;

  // we have to store references to the engine because the
  // vocbase might already be destroyed at the time the destructor is executed
  RocksDBEngine& _engine;

 private:
  uint64_t const _objectId;
};
}  // namespace arangodb
