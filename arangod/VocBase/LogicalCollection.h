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
/// @author Michael Hackstein
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_LOGICAL_COLLECTION_H
#define ARANGOD_VOCBASE_LOGICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/CountCache.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
typedef std::string ServerID;      // ID of a server
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class FollowerInfo;
class Index;
class IndexIterator;
class KeyGenerator;
struct KeyLockInfo;
class LocalDocumentId;
class ManagedDocumentResult;
struct OperationOptions;
class PhysicalCollection;
class Result;
class ShardingInfo;
class StringRef;

namespace transaction {
class Methods;
}

class ChecksumResult : public Result {
 public:
  explicit ChecksumResult(Result&& result) : Result(std::move(result)) {}
  explicit ChecksumResult(velocypack::Builder&& builder): Result(TRI_ERROR_NO_ERROR), _builder(std::move(builder)) {}

  velocypack::Builder builder() {
    return _builder;
  }

  velocypack::Slice slice() {
    return _builder.slice();
  }

 private:
  velocypack::Builder _builder;
};

/// please note that coordinator-based logical collections are frequently
/// created and discarded, so ctor & dtor need to be as efficient as possible.
/// additionally, do not put any volatile state into this object in the coordinator, 
/// as the ClusterInfo may create many different temporary physical LogicalCollection 
/// objects (one after the other) even for the same "logical" LogicalCollection.
/// this which will also discard the collection's volatile state each time!
/// all state of a LogicalCollection in the coordinator case needs to be derived
/// from the JSON info in the agency's plan entry for the collection...
class LogicalCollection : public LogicalDataSource {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalCollection() = delete;
  LogicalCollection(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    bool isAStub,
    uint64_t planVersion = 0
  );
  LogicalCollection(LogicalCollection const&) = delete;
  LogicalCollection& operator=(LogicalCollection const&) = delete;
  virtual ~LogicalCollection();
  
  enum CollectionVersions {
    VERSION_30 = 5,
    VERSION_31 = 6,
    VERSION_33 = 7,
    VERSION_34 = 8
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical collection
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  /// @brief hard-coded minimum version number for collections
  static constexpr uint32_t minimumVersion() { return VERSION_30; }
  /// @brief current version for collections
  static constexpr uint32_t currentVersion() { return VERSION_34; }

  // SECTION: Meta Information
  uint32_t version() const { return _version; }

  void setVersion(CollectionVersions version) { _version = version; }

  uint32_t internalVersion() const;

  TRI_col_type_e type() const;

  std::string globallyUniqueId() const;

  // For normal collections the realNames is just a vector of length 1
  // with its name. For smart edge collections (enterprise only) this is
  // different.
  virtual std::vector<std::string> realNames() const {
    return std::vector<std::string>{name()};
  }
  // Same here, this is for reading in AQL:
  virtual std::vector<std::string> realNamesForRead() const {
    return std::vector<std::string>{name()};
  }

  TRI_vocbase_col_status_e status() const;
  TRI_vocbase_col_status_e getStatusLocked();

  void executeWhileStatusWriteLocked(std::function<void()> const& callback);
  void executeWhileStatusLocked(std::function<void()> const& callback);
  bool tryExecuteWhileStatusLocked(std::function<void()> const& callback);

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always
  /// TRI_VOC_COL_STATUS_CORRUPTED
  TRI_vocbase_col_status_e tryFetchStatus(bool&);
  std::string statusString() const;

  uint64_t numberDocuments(transaction::Methods*, transaction::CountType type);

  // SECTION: Properties
  TRI_voc_rid_t revision(transaction::Methods*) const;
  bool waitForSync() const;
  bool isSmart() const;
  bool isAStub() const { return _isAStub; }

  void waitForSync(bool value) { _waitForSync = value; }

  // SECTION: sharding
  ShardingInfo* shardingInfo() const;

  // proxy methods that will use the sharding info in the background
  size_t numberOfShards() const;
  size_t replicationFactor() const;
  std::string distributeShardsLike() const;
  std::vector<std::string> const& avoidServers() const;
  bool isSatellite() const;
  bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;
  TEST_VIRTUAL std::shared_ptr<ShardMap> shardIds() const;
  
  // mutation options for sharding
  void setShardMap(std::shared_ptr<ShardMap> const& map);
  void distributeShardsLike(std::string const& cid, ShardingInfo const* other);
 
  // query shard for a given document 
  int getResponsibleShard(arangodb::velocypack::Slice,
                          bool docComplete, std::string& shardID);

  int getResponsibleShard(arangodb::velocypack::Slice,
                          bool docComplete, std::string& shardID,
                          bool& usesDefaultShardKeys,
                          std::string const& key = "");

  /// @briefs creates a new document key, the input slice is ignored here
  /// this method is overriden in derived classes
  virtual std::string createKey(arangodb::velocypack::Slice input);

  PhysicalCollection* getPhysical() const { return _physical.get(); }

  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx);
  std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx);

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback);

  /// @brief fetches current index selectivity estimates
  /// if allowUpdate is true, will potentially make a cluster-internal roundtrip to
  /// fetch current values!
  std::unordered_map<std::string, double> clusterIndexEstimates(bool allowUpdate);
  
  /// @brief sets the current index selectivity estimates
  void clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates);
  
  /// @brief flushes the current index selectivity estimates
  void flushClusterIndexEstimates();

  std::vector<std::shared_ptr<Index>> getIndexes() const;

  void getIndexesVPack(velocypack::Builder&, uint8_t,
                       std::function<bool(arangodb::Index const*)> const& filter =
                         [](arangodb::Index const*) -> bool { return true; }) const;

  /// @brief a method to skip certain documents in AQL write operations,
  /// this is only used in the enterprise edition for smart graphs
  virtual bool skipForAqlWrite(velocypack::Slice document,
                               std::string const& key) const;
  
  bool allowUserKeys() const;

  // SECTION: Modification Functions
  void load();
  void unload();

  virtual arangodb::Result drop() override;
  virtual Result rename(std::string&& name) override;
  virtual void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialization
  void toVelocyPackIgnore(velocypack::Builder& result,
      std::unordered_set<std::string> const& ignoreKeys, bool translateCids,
      bool forPersistence) const;

  velocypack::Builder toVelocyPackIgnore(
      std::unordered_set<std::string> const& ignoreKeys, bool translateCids,
      bool forPersistence) const;

  virtual void toVelocyPackForClusterInventory(velocypack::Builder&,
                                               bool useSystem,
                                               bool isReady,
                                               bool allInSync) const;

  // Update this collection.
  using LogicalDataSource::properties;
  virtual arangodb::Result properties(
    velocypack::Slice const& slice,
    bool partialUpdate
  ) override;

  /// @brief return the figures for a collection
  virtual std::shared_ptr<velocypack::Builder> figures() const;

  /// @brief opens an existing collection
  void open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  virtual std::shared_ptr<Index> createIndex(velocypack::Slice const&, bool&);

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(TRI_idx_iid_t) const;

  bool dropIndex(TRI_idx_iid_t iid);

  // SECTION: Index access (local only)

  /// @brief reads an element from the document collection
  Result read(transaction::Methods* trx, StringRef const& key,
              ManagedDocumentResult& mdr, bool lock);
  Result read(transaction::Methods*, arangodb::velocypack::Slice const&,
              ManagedDocumentResult& result, bool);

  /// @brief processes a truncate operation
  Result truncate(transaction::Methods& trx, OperationOptions& options);

  // convenience function for downwards-compatibility
  Result insert(transaction::Methods* trx, velocypack::Slice const slice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock) {
    TRI_voc_tick_t unused;
    return insert(trx, slice, result, options, resultMarkerTick, lock, unused,
                  nullptr, nullptr);
  }

  /**
  * @param callbackDuringLock Called immediately after a successful insert. If
  * it returns a failure, the insert will be rolled back. If the insert wasn't
  * successful, it isn't called. May be nullptr.
  */
  Result insert(transaction::Methods* trx, velocypack::Slice slice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock,
                TRI_voc_tick_t& revisionId,
                KeyLockInfo* keyLockInfo,
                std::function<Result(void)> callbackDuringLock);

  Result update(transaction::Methods*, velocypack::Slice,
                ManagedDocumentResult& result, OperationOptions&,
                TRI_voc_tick_t&, bool lock, TRI_voc_rid_t& prevRev,
                ManagedDocumentResult& previous,
                std::function<Result(void)> callbackDuringLock);

  Result replace(transaction::Methods*, velocypack::Slice,
                 ManagedDocumentResult& result, OperationOptions&,
                 TRI_voc_tick_t&, bool lock, TRI_voc_rid_t& prevRev,
                 ManagedDocumentResult& previous,
                 std::function<Result(void)> callbackDuringLock);

  Result remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    ManagedDocumentResult& previous,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
  );

  bool readDocument(transaction::Methods* trx,
                    LocalDocumentId const& token,
                    ManagedDocumentResult& result) const;

  bool readDocumentWithCallback(transaction::Methods* trx,
                                LocalDocumentId const& token,
                                IndexIterator::DocumentCallback const& cb) const;

  /// @brief Persist the connected physical collection.
  ///        This should be called AFTER the collection is successfully
  ///        created and only on Sinlge/DBServer
  void persistPhysicalCollection();

  basics::ReadWriteLock& lock() { return _lock; }

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  void deferDropCollection(
    std::function<bool(arangodb::LogicalCollection&)> const& callback
  );

  // SECTION: Key Options
  velocypack::Slice keyOptions() const;

  // Get a reference to this KeyGenerator.
  // Caller is not allowed to free it.
  inline KeyGenerator* keyGenerator() const { return _keyGenerator.get(); }
  
  transaction::CountCache& countCache() { return _countCache; }

  ChecksumResult checksum(bool, bool) const;
  
  std::unique_ptr<FollowerInfo> const& followers() const;

 protected:
  virtual arangodb::Result appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
  ) const override;
  
 private:
  void prepareIndexes(velocypack::Slice indexesSlice);

  void increaseInternalVersion();

  transaction::CountCache _countCache;

 protected:
  virtual void includeVelocyPackEnterprise(velocypack::Builder& result) const;

  // SECTION: Meta Information
  
  mutable basics::ReadWriteLock _lock;  // lock protecting the status and name
  
  /// @brief collection format version
  uint32_t _version;
  
  // @brief Internal version used for caching
  uint32_t _internalVersion;
  
  // @brief Collection type
  TRI_col_type_e const _type;
  
  // @brief Current state of this colletion
  TRI_vocbase_col_status_e _status;

  /// @brief is this a global collection on a DBServer
  bool const _isAStub;

  // @brief Flag if this collection is a smart one. (Enterprise only)
  bool _isSmart;

  // SECTION: Properties
  bool _waitForSync;

  bool const _allowUserKeys;

  // SECTION: Key Options
  
  // @brief options for key creation, TODO Really VPack?
  std::shared_ptr<velocypack::Buffer<uint8_t> const> _keyOptions;
  std::unique_ptr<KeyGenerator> _keyGenerator;
 
  std::unique_ptr<PhysicalCollection> _physical;

  mutable arangodb::Mutex _infoLock;  // lock protecting the info

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<FollowerInfo> _followers;
  
  /// @brief sharding information
  std::unique_ptr<ShardingInfo> _sharding; 
};

}  // namespace arangodb

#endif
