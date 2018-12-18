////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_ENGINE_CLUSTER_COLLECTION_H
#define ARANGOD_CLUSTER_ENGINE_CLUSTER_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringRef.h"
#include "ClusterEngine/ClusterSelectivityEstimates.h"
#include "ClusterEngine/Common.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

namespace rocksdb {
class Transaction;
}

namespace arangodb {
namespace cache {
class Cache;
}

class LogicalCollection;
class ManagedDocumentResult;
class Result;
class RocksDBPrimaryIndex;
class RocksDBVPackIndex;
class LocalDocumentId;

class ClusterCollection final : public PhysicalCollection {
  constexpr static double defaultLockTimeout = 10.0 * 60.0;

 public:
  explicit ClusterCollection(
    LogicalCollection& collection,
    ClusterEngineType sengineType,
    arangodb::velocypack::Slice const& info
  );
  ClusterCollection(LogicalCollection& collection, PhysicalCollection const*);  // use in cluster only!!!!!

  ~ClusterCollection();

  /// @brief fetches current index selectivity estimates
  /// if allowUpdate is true, will potentially make a cluster-internal roundtrip to
  /// fetch current values!
  std::unordered_map<std::string, double> clusterIndexEstimates(bool allowUpdate) const override;

  /// @brief sets the current index selectivity estimates
  void clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates) override;

  /// @brief flushes the current index selectivity estimates
  void flushClusterIndexEstimates() override;

  std::string const& path() const override;
  void setPath(std::string const& path) override;

  arangodb::Result updateProperties(velocypack::Slice const& slice,
                                    bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalCollection* clone(LogicalCollection& collection) const override;

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  /// @brief return the figures for a collection
  std::shared_ptr<velocypack::Builder> figures() override;

  /// @brief closes an open collection
  int close() override;
  void load() override;
  void unload() override;

  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  uint64_t numberDocuments(transaction::Methods* trx) const override;

  /// @brief report extra memory used by indexes etc.
  size_t memory() const override;
  void open(bool ignoreErrors) override;

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const override;

  std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                     bool restore, bool& created) override;

  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;
  std::unique_ptr<IndexIterator> getAllIterator(
      transaction::Methods* trx) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx) const override;

  std::unique_ptr<IndexIterator> getSortedAllIterator(
      transaction::Methods* trx) const;

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback) override;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  Result truncate(
    transaction::Methods& trx,
    OperationOptions& options
  ) override;

  void deferDropCollection(
    std::function<bool(LogicalCollection&)> const& callback
  ) override;

  LocalDocumentId lookupKey(transaction::Methods* trx,
                            velocypack::Slice const& key) const override;

  Result read(transaction::Methods*, arangodb::StringRef const& key,
              ManagedDocumentResult& result, bool) override;

  Result read(transaction::Methods* trx, arangodb::velocypack::Slice const& key,
              ManagedDocumentResult& result, bool locked) override {
    return this->read(trx, arangodb::StringRef(key), result, locked);
  }

  bool readDocument(transaction::Methods* trx, LocalDocumentId const& token,
                    ManagedDocumentResult& result) const override;

  bool readDocumentWithCallback(
      transaction::Methods* trx, LocalDocumentId const& token,
      IndexIterator::DocumentCallback const& cb) const override;

  Result insert(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                arangodb::ManagedDocumentResult& result,
                OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
                bool lock, TRI_voc_tick_t& revisionId,
                KeyLockInfo* /*keyLockInfo*/,
                std::function<Result(void)> callbackDuringLock) override;

  Result update(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice const newSlice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock,
                TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                arangodb::velocypack::Slice const key,
                std::function<Result(void)> callbackDuringLock) override;

  Result replace(transaction::Methods* trx,
                 arangodb::velocypack::Slice const newSlice,
                 ManagedDocumentResult& result, OperationOptions& options,
                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                 TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                 std::function<Result(void)> callbackDuringLock) override;

  Result remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    ManagedDocumentResult& previous,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
  ) override;

 protected:
  /// @brief Inject figures that are specific to StorageEngine
  void figuresSpecific(
      std::shared_ptr<arangodb::velocypack::Builder>&) override;

 private:
  void addIndex(std::shared_ptr<arangodb::Index> idx);

  // keep locks just to adhere to behaviour in other collections
  mutable basics::ReadWriteLock _exclusiveLock;
  ClusterEngineType _engineType;
  velocypack::Builder _info;
  ClusterSelectivityEstimates _selectivityEstimates;
};

}  // namespace arangodb

#endif
