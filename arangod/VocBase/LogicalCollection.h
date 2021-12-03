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
/// @author Michael Hackstein
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Futures/Future.h"
#include "Indexes/IndexIterator.h"
#include "Transaction/CountCache.h"
#include "Utils/OperationResult.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/Validators.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
typedef std::string ServerID;  // ID of a server
typedef std::string ShardID;   // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class FollowerInfo;
class Index;
class IndexIterator;
class KeyGenerator;
class LocalDocumentId;
class ManagedDocumentResult;
struct OperationOptions;
class PhysicalCollection;
class Result;
class ShardingInfo;

namespace transaction {
class Methods;
}

/// please note that coordinator-based logical collections are frequently
/// created and discarded, so ctor & dtor need to be as efficient as possible.
/// additionally, do not put any volatile state into this object in the
/// coordinator, as the ClusterInfo may create many different temporary physical
/// LogicalCollection objects (one after the other) even for the same "logical"
/// LogicalCollection. this which will also discard the collection's volatile
/// state each time! all state of a LogicalCollection in the coordinator case
/// needs to be derived from the JSON info in the agency's plan entry for the
/// collection...

typedef std::shared_ptr<LogicalCollection> LogicalCollectionPtr;

class LogicalCollection : public LogicalDataSource {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalCollection() = delete;
  LogicalCollection(TRI_vocbase_t& vocbase, velocypack::Slice info, bool isAStub);
  LogicalCollection(LogicalCollection const&) = delete;
  LogicalCollection& operator=(LogicalCollection const&) = delete;
  ~LogicalCollection() override;

  enum class Version { v30 = 5, v31 = 6, v33 = 7, v34 = 8, v37 = 9 };

  /*
   * @brief Available types of internal validators. These validators
   * are managed by the database, and bound to features of collections.
   * They cannot be modified by the user, and should therefore not be exposed.
   * Mostly used for Enterprise/Smart collection types which have
   * some specialities over community collections.
   * This enum is used to generate bitmap entries. So whenever you
   * add a new value make sure it is the next free 2^n value.
   * For Backwards Compatibility a value can never be reused.
   */
  enum InternalValidatorType {
    None = 0,
    LogicalSmartEdge = 1,
    LocalSmartEdge = 2,
    RemoteSmartEdge = 4,
    SmartToSatEdge = 8,
    SatToSmartEdge = 16,
  };

  /// @brief the category representing a logical collection
  static Category const& category() noexcept;

  /// @brief hard-coded minimum version number for collections
  static constexpr Version minimumVersion() { return Version::v30; }
  /// @brief current version for collections
  static constexpr Version currentVersion() { return Version::v37; }

  // SECTION: Meta Information
  Version version() const { return _version; }

  void setVersion(Version version) { _version = version; }

  uint32_t v8CacheVersion() const;

  TRI_col_type_e type() const;

  std::string globallyUniqueId() const;

  // For normal collections the realNames is just a vector of length 1
  // with its name. For smart edge collections (Enterprise Edition only)
  // this is different.
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

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always
  /// TRI_VOC_COL_STATUS_CORRUPTED
  TRI_vocbase_col_status_e tryFetchStatus(bool&);

  uint64_t numberDocuments(transaction::Methods*, transaction::CountType type);

  // SECTION: Properties
  RevisionId revision(transaction::Methods*) const;
  bool waitForSync() const { return _waitForSync; }
  void waitForSync(bool value) { _waitForSync = value; }
#ifdef USE_ENTERPRISE
  bool isDisjoint() const { return _isDisjoint; }
  bool isSmart() const { return _isSmart; }
  bool isSmartChild() const { return _isSmartChild; }
#else
  bool isDisjoint() const { return false; }
  bool isSmart() const { return false; }
  bool isSmartChild() const { return false; }
#endif
  bool usesRevisionsAsDocumentIds() const;
  /// @brief is this a cluster-wide Plan (ClusterInfo) collection
  bool isAStub() const { return _isAStub; }

  bool hasSmartJoinAttribute() const { return !smartJoinAttribute().empty(); }

  bool hasClusterWideUniqueRevs() const;

  /// @brief return the name of the SmartJoin attribute (empty string
  /// if no SmartJoin attribute is present)
  std::string const& smartJoinAttribute() const { return _smartJoinAttribute; }

  // SECTION: sharding
  ShardingInfo* shardingInfo() const;

  // proxy methods that will use the sharding info in the background
  size_t numberOfShards() const;
  size_t replicationFactor() const;
  size_t writeConcern() const;
  std::string const& distributeShardsLike() const;
  std::vector<std::string> const& avoidServers() const;
  bool isSatellite() const;
  bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;
  TEST_VIRTUAL std::shared_ptr<ShardMap> shardIds() const;

  // mutation options for sharding
  void setShardMap(std::shared_ptr<ShardMap> map) noexcept;
  void distributeShardsLike(std::string const& cid, ShardingInfo const* other);

  // query shard for a given document
  ErrorCode getResponsibleShard(velocypack::Slice slice,
                                bool docComplete, std::string& shardID);
  ErrorCode getResponsibleShard(std::string_view key, std::string& shardID);

  ErrorCode getResponsibleShard(velocypack::Slice slice, bool docComplete,
                                std::string& shardID, bool& usesDefaultShardKeys,
                                std::string_view key = std::string_view());

  /// @briefs creates a new document key, the input slice is ignored here
  /// this method is overriden in derived classes
  virtual std::string createKey(velocypack::Slice input);

  PhysicalCollection* getPhysical() const { return _physical.get(); }

  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx, ReadOwnWrites readOwnWrites);
  std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx);

  /// @brief fetches current index selectivity estimates
  /// if allowUpdate is true, will potentially make a cluster-internal roundtrip
  /// to fetch current values!
  /// @param tid the optional transaction ID to use
  IndexEstMap clusterIndexEstimates(bool allowUpdating,
                                    TransactionId tid = TransactionId::none());

  /// @brief flushes the current index selectivity estimates
  void flushClusterIndexEstimates();

  /// @brief return all indexes of the collection
  std::vector<std::shared_ptr<Index>> getIndexes() const;

  void getIndexesVPack(velocypack::Builder&,
                       std::function<bool(Index const*, uint8_t&)> const& filter) const;

  /// @brief a method to skip certain documents in AQL write operations,
  /// this is only used in the Enterprise Edition for SmartGraphs
  virtual bool skipForAqlWrite(velocypack::Slice document, std::string const& key) const;

  bool allowUserKeys() const;

  // SECTION: Modification Functions
  virtual Result drop() override;
  virtual Result rename(std::string&& name) override;
  virtual void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialization
  void toVelocyPackIgnore(velocypack::Builder& result,
                          std::unordered_set<std::string> const& ignoreKeys,
                          Serialization context) const;

  velocypack::Builder toVelocyPackIgnore(std::unordered_set<std::string> const& ignoreKeys,
                                         Serialization context) const;

  void toVelocyPackForInventory(velocypack::Builder&) const;

  virtual void toVelocyPackForClusterInventory(velocypack::Builder&, bool useSystem,
                                               bool isReady, bool allInSync) const;

  using LogicalDataSource::properties;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing DataSource
  /// @param definition the properties being updated
  /// @param partialUpdate modify only the specified properties (false == all)
  //////////////////////////////////////////////////////////////////////////////
  virtual Result properties(velocypack::Slice definition, bool partialUpdate);

  /// @brief return the figures for a collection
  virtual futures::Future<OperationResult> figures(bool details,
                                                   OperationOptions const& options) const;

  /// @brief closes an open collection
  ErrorCode close();

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  virtual std::shared_ptr<Index> createIndex(velocypack::Slice, bool&);

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice) const;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(IndexId) const;

  /// @brief Find index by name
  std::shared_ptr<Index> lookupIndex(std::string const&) const;

  bool dropIndex(IndexId iid);

  // SECTION: Index access (local only)

  /// @brief processes a truncate operation
  Result truncate(transaction::Methods& trx, OperationOptions& options);

  /// @brief compact-data operation
  void compact();

  Result insert(transaction::Methods* trx, velocypack::Slice slice,
                ManagedDocumentResult& result, OperationOptions& options);

  Result update(transaction::Methods*, velocypack::Slice newSlice,
                ManagedDocumentResult& result, OperationOptions&,
                ManagedDocumentResult& previousMdr);

  Result replace(transaction::Methods*, velocypack::Slice newSlice,
                 ManagedDocumentResult& result, OperationOptions&,
                 ManagedDocumentResult& previousMdr);

  Result remove(transaction::Methods& trx, velocypack::Slice slice,
                OperationOptions& options, ManagedDocumentResult& previousMdr);

  /// @brief Persist the connected physical collection.
  ///        This should be called AFTER the collection is successfully
  ///        created and only on Sinlge/DBServer
  void persistPhysicalCollection();

  /// lock protecting the status and name
  basics::ReadWriteLock& statusLock() noexcept;

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  void deferDropCollection(std::function<bool(LogicalCollection&)> const& callback);

  void schemaToVelocyPack(VPackBuilder&) const;
  Result validate(VPackSlice newDoc, VPackOptions const*) const;  // insert
  Result validate(VPackSlice modifiedDoc, VPackSlice oldDoc, VPackOptions const*) const;  // update / replace

  // Get a reference to this KeyGenerator.
  // Caller is not allowed to free it.
  inline KeyGenerator* keyGenerator() const { return _keyGenerator.get(); }

  transaction::CountCache& countCache() { return _countCache; }

  std::unique_ptr<FollowerInfo> const& followers() const;

  /// @brief returns the value of _syncByRevision
  bool syncByRevision() const;

  /// @brief returns the value of _syncByRevision, but only for "real" collections with data backing.
  /// returns false for all collections with no data backing.
  bool useSyncByRevision() const;

  /// @brief set the internal validator types. This should be handled with care
  /// and be set before the collection is persisted into the Agency.
  /// The value should not be modified at runtime.
  // (Technically no issue but will have side-effects on shards)
  void setInternalValidatorTypes(uint64_t type);

  uint64_t getInternalValidatorTypes() const;

  bool isLocalSmartEdgeCollection() const noexcept;

  bool isRemoteSmartEdgeCollection() const noexcept;

  bool isSmartEdgeCollection() const noexcept;

  bool isSatToSmartEdgeCollection() const noexcept;

  bool isSmartToSatEdgeCollection() const noexcept;

 protected:
  void addInternalValidator(std::unique_ptr<ValidatorBase>);

  virtual Result appendVelocyPack(velocypack::Builder& builder,
                                  Serialization context) const override;

  Result updateSchema(VPackSlice schema);

  /**
   * Enterprise only method. See enterprise code for implementation
   * Community has a dummy stub.
   */
  std::string createSmartToSatKey(arangodb::velocypack::Slice input);

  void decorateWithInternalEEValidators();

 private:
  void prepareIndexes(velocypack::Slice indexesSlice);

  void increaseV8Version();

  bool determineSyncByRevision() const;

  void decorateWithInternalValidators();

 protected:
  virtual void includeVelocyPackEnterprise(velocypack::Builder& result) const;

  // SECTION: Meta Information

  /// lock protecting the status and name
  mutable basics::ReadWriteLock _statusLock;

  /// @brief collection format version
  Version _version;

  // @brief Internal version used for caching
  uint32_t _v8CacheVersion;

  // @brief Collection type
  TRI_col_type_e const _type;

  // @brief Current state of this colletion
  std::atomic<TRI_vocbase_col_status_e> _status;

  /// @brief is this a global collection on a DBServer
  bool const _isAStub;

#ifdef USE_ENTERPRISE
  // @brief Flag if this collection is a disjoint smart one. (Enterprise Edition
  // only) can only be true if _isSmart is also true
  bool const _isDisjoint;
  // @brief Flag if this collection is a smart one. (Enterprise Edition only)
  bool const _isSmart;
  // @brief Flag if this collection is a child of a smart collection (Enterprise Edition only)
  bool const _isSmartChild;
#endif

  // SECTION: Properties
  std::atomic<bool> _waitForSync;

  bool const _allowUserKeys;

  std::atomic<bool> _usesRevisionsAsDocumentIds;

  std::atomic<bool> _syncByRevision;

  std::string _smartJoinAttribute;

  transaction::CountCache _countCache;

  // SECTION: Key Options

  // @brief options for key creation
  std::shared_ptr<velocypack::Buffer<uint8_t> const> _keyOptions;
  std::unique_ptr<KeyGenerator> _keyGenerator;

  std::unique_ptr<PhysicalCollection> _physical;

  mutable Mutex _infoLock;  // lock protecting the info

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<FollowerInfo> _followers;

  /// @brief sharding information
  std::unique_ptr<ShardingInfo> _sharding;

  // `_schema` must be used with atomic accessors only!!
  // We use relaxed access (load/store) as we only care about atomicity.
  std::shared_ptr<ValidatorBase> _schema;

  // This is a bitmap entry of InternalValidatorType entries.
  uint64_t _internalValidatorTypes;

  std::vector<std::unique_ptr<ValidatorBase>> _internalValidators;
};

}  // namespace arangodb

