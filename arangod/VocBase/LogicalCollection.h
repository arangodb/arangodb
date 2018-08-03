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

#include "LogicalDataSource.h"
#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class LocalDocumentId;
class FollowerInfo;
class Index;
class IndexIterator;
class ManagedDocumentResult;
struct OperationOptions;
class PhysicalCollection;
class Result;
class StringRef;
class KeyGenerator;
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

class LogicalCollection: public LogicalDataSource {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalCollection() = delete;
  LogicalCollection(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    bool isAStub,
    uint64_t planVersion = 0
  );

  virtual ~LogicalCollection();

  enum CollectionVersions { VERSION_30 = 5, VERSION_31 = 6, VERSION_33 = 7 };

 protected:  // If you need a copy outside the class, use clone below.
  explicit LogicalCollection(LogicalCollection const&);

 private:
  LogicalCollection& operator=(LogicalCollection const&) = delete;

 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical collection
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  virtual std::unique_ptr<LogicalCollection> clone() {
    return std::unique_ptr<LogicalCollection>(new LogicalCollection(*this));
  }

  /// @brief hard-coded minimum version number for collections
  static constexpr uint32_t minimumVersion() { return VERSION_30; }
  /// @brief current version for collections
  static constexpr uint32_t currentVersion() { return VERSION_33; }

  // SECTION: Meta Information
  uint32_t version() const { return _version; }

  void setVersion(CollectionVersions version) { _version = version; }

  uint32_t internalVersion() const;

  TRI_col_type_e type() const;

  std::string globallyUniqueId() const;

  // Does always return the cid
  std::string const distributeShardsLike() const;
  void distributeShardsLike(std::string const& cid);

  std::vector<std::string> const& avoidServers() const;
  void avoidServers(std::vector<std::string> const&);

  // For normal collections the realNames is just a vector of length 1
  // with its name. For smart edge collections (enterprise only) this is
  // different.
  virtual std::vector<std::string> realNames() const {
    std::vector<std::string> res{name()};
    return res;
  }
  // Same here, this is for reading in AQL:
  virtual std::vector<std::string> realNamesForRead() const {
    std::vector<std::string> res{name()};
    return res;
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

  uint64_t numberDocuments(transaction::Methods*) const;

  // SECTION: Properties
  TRI_voc_rid_t revision(transaction::Methods*) const;
  bool isLocal() const;
  bool waitForSync() const;
  bool isSmart() const;
  bool isAStub() const { return _isAStub; }

  void waitForSync(bool value) { _waitForSync = value; }

  std::unique_ptr<FollowerInfo> const& followers() const;

  PhysicalCollection* getPhysical() const { return _physical.get(); }

  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx);
  std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx);

  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback);

  //// SECTION: Indexes

  // Estimates
  std::unordered_map<std::string, double> clusterIndexEstimates(bool doNotUpdate=false);
  void clusterIndexEstimates(std::unordered_map<std::string, double>&& estimates);

  double clusterIndexEstimatesTTL() const {
    return _clusterEstimateTTL;
  }

  void clusterIndexEstimatesTTL(double ttl) {
    _clusterEstimateTTL = ttl;
  }
  // End - Estimates

  std::vector<std::shared_ptr<Index>> getIndexes() const;

  void getIndexesVPack(velocypack::Builder&, bool withFigures, bool forPersistence,
                       std::function<bool(arangodb::Index const*)> const& filter =
                         [](arangodb::Index const*) -> bool { return true; }) const;

  // SECTION: Replication
  int replicationFactor() const;
  void replicationFactor(int);
  bool isSatellite() const;

  // SECTION: Sharding
  int numberOfShards() const noexcept;
  void numberOfShards(int);
  bool allowUserKeys() const;
  virtual bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;

  virtual std::shared_ptr<ShardMap> shardIds() const;

  // return a filtered list of the collection's shards
  std::shared_ptr<ShardMap> shardIds(
      std::unordered_set<std::string> const& includedShards) const;
  void setShardMap(std::shared_ptr<ShardMap>& map);

  /// @brief a method to skip certain documents in AQL write operations,
  /// this is only used in the enterprise edition for smart graphs
  virtual bool skipForAqlWrite(velocypack::Slice document,
                               std::string const& key) const;

  // SECTION: Modification Functions
  void load();
  void unload();

  virtual arangodb::Result drop() override;
  virtual Result rename(std::string&& name, bool doSync) override;
  virtual void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialisation
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
  virtual arangodb::Result updateProperties(velocypack::Slice const&, bool);

  /// @brief return the figures for a collection
  virtual std::shared_ptr<velocypack::Builder> figures() const;

  /// @brief opens an existing collection
  void open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  virtual std::shared_ptr<Index> createIndex(transaction::Methods*,
                                             velocypack::Slice const&, bool&);

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
  /// NOTE: This function throws on error
  void truncate(transaction::Methods* trx, OperationOptions&);

  Result insert(transaction::Methods*, velocypack::Slice const,
                ManagedDocumentResult& result, OperationOptions&,
                TRI_voc_tick_t&, bool lock, TRI_voc_tick_t& revisionId);
  // convenience function for downwards-compatibility
  Result insert(transaction::Methods* trx, velocypack::Slice const slice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock) {
    TRI_voc_tick_t unused;
    return insert(trx, slice, result, options, resultMarkerTick, lock, unused);
  }

  Result update(transaction::Methods*, velocypack::Slice const,
                ManagedDocumentResult& result, OperationOptions&,
                TRI_voc_tick_t&, bool, TRI_voc_rid_t& prevRev,
                ManagedDocumentResult& previous,
                velocypack::Slice const pattern);

  Result replace(transaction::Methods*, velocypack::Slice const,
                 ManagedDocumentResult& result, OperationOptions&,
                 TRI_voc_tick_t&, bool /*lock*/, TRI_voc_rid_t& prevRev,
                 ManagedDocumentResult& previous,
                 velocypack::Slice const pattern);


  Result remove(transaction::Methods*, velocypack::Slice const,
                OperationOptions&, TRI_voc_tick_t&, bool,     //options should be const
                TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous);

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

  ChecksumResult checksum(bool, bool) const;

  // compares the checksum value passed in the Slice (must be of type String)
  // with the checksum provided in the reference checksum
  Result compareChecksums(velocypack::Slice checksumSlice, std::string const& referenceChecksum) const;

 protected:
  virtual arangodb::Result appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
  ) const override;

 private:
  void prepareIndexes(velocypack::Slice indexesSlice);

  // SECTION: Indexes (local only)
  // @brief create index with the given definition.
  bool openIndex(velocypack::Slice const&, transaction::Methods*);

  void increaseInternalVersion();

 protected:
  virtual void includeVelocyPackEnterprise(velocypack::Builder& result) const;

  int checkRevision(transaction::Methods* trx, TRI_voc_rid_t expected,
                    TRI_voc_rid_t found) const;


  TRI_voc_rid_t newRevisionId() const;

  bool isValidEdgeAttribute(velocypack::Slice const& slice) const;

  /// @brief new object for insert, value must have _key set correctly.
  Result newObjectForInsert(transaction::Methods* trx,
                            velocypack::Slice const& value,
                            bool isEdgeCollection, velocypack::Builder& builder,
                            bool isRestore,
                            TRI_voc_rid_t& revisionId) const;

  /// @brief new object for remove, must have _key set
  void newObjectForRemove(transaction::Methods* trx,
                          velocypack::Slice const& oldValue,
                          velocypack::Builder& builder,
                          bool isRestore, TRI_voc_rid_t& revisionId) const;

  /// @brief merge two objects for update
  Result mergeObjectsForUpdate(transaction::Methods* trx,
                               velocypack::Slice const& oldValue,
                               velocypack::Slice const& newValue,
                               bool isEdgeCollection,
                               bool mergeObjects, bool keepNull,
                               velocypack::Builder& builder,
                               bool isRestore, TRI_voc_rid_t& revisionId) const;

  /// @brief new object for replace
  Result newObjectForReplace(transaction::Methods* trx,
                             velocypack::Slice const& oldValue,
                             velocypack::Slice const& newValue,
                             bool isEdgeCollection,
                             velocypack::Builder& builder,
                             bool isRestore, TRI_voc_rid_t& revisionId) const;



  // SECTION: Meta Information
  //
  // @brief Internal version used for caching
  uint32_t _internalVersion;

  bool const _isAStub;

  // @brief Collection type
  TRI_col_type_e const _type;

  // @brief Name of other collection this shards should be distributed like
  std::string _distributeShardsLike;

  // @brief Name of other collection this shards should be distributed like
  std::vector<std::string> _avoidServers;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<FollowerInfo> _followers;

  // @brief Current state of this colletion
  TRI_vocbase_col_status_e _status;

  // @brief Flag if this collection is a smart one. (Enterprise only)
  bool _isSmart;

  // SECTION: Properties
  bool _isLocal;
  bool const _isDBServer;
  bool _waitForSync;

  uint32_t _version;

  // SECTION: Replication
  size_t _replicationFactor;

  // SECTION: Sharding
  size_t _numberOfShards;
  bool const _allowUserKeys;
  std::vector<std::string> _shardKeys;
  // This is shared_ptr because it is thread-safe
  // A thread takes a copy of this, another one updates this
  // the first one still has a valid copy
  std::shared_ptr<ShardMap> _shardIds;

  // SECTION: Key Options
  // TODO Really VPack?
  std::shared_ptr<velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation
  std::unique_ptr<KeyGenerator> _keyGenerator;

  std::unique_ptr<PhysicalCollection> _physical;

  mutable basics::ReadWriteLock _lock;  // lock protecting the status and name

  mutable arangodb::Mutex _infoLock;  // lock protecting the info

  std::unordered_map<std::string, double> _clusterEstimates;
  double _clusterEstimateTTL; //only valid if above vector is not empty
  basics::ReadWriteLock _clusterEstimatesLock;
};

}  // namespace arangodb

#endif
