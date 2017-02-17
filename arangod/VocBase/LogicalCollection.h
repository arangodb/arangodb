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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_LOGICAL_COLLECTION_H
#define ARANGOD_VOCBASE_LOGICAL_COLLECTION_H 1

#include "Basics/Common.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>

struct TRI_df_marker_t;

namespace arangodb {
namespace basics {
class LocalTaskQueue;
}

namespace velocypack {
class Slice;
}

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

struct DatafileStatisticsContainer;
class Ditches;
struct DocumentIdentifierToken;
class FollowerInfo;
class Index;
class KeyGenerator;
class ManagedDocumentResult;
struct OperationOptions;
class PhysicalCollection;
class MMFilesPrimaryIndex;
class StringRef;
namespace transaction {
class Methods;
}

class LogicalCollection {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalCollection(TRI_vocbase_t*, velocypack::Slice const&,
                    bool isPhysical);

  virtual ~LogicalCollection();

  enum CollectionVersions { VERSION_30 = 5, VERSION_31 = 6 };

 protected:  // If you need a copy outside the class, use clone below.
  explicit LogicalCollection(LogicalCollection const&);

 private:
  LogicalCollection& operator=(LogicalCollection const&) = delete;

 public:
  LogicalCollection() = delete;

  virtual std::unique_ptr<LogicalCollection> clone() {
    auto p = new LogicalCollection(*this);
    return std::unique_ptr<LogicalCollection>(p);
  }

  /// @brief hard-coded minimum version number for collections
  static constexpr uint32_t minimumVersion() { return VERSION_30; }
  /// @brief current version for collections
  static constexpr uint32_t currentVersion() { return VERSION_31; }

  /// @brief determine whether a collection name is a system collection name
  static inline bool IsSystemName(std::string const& name) {
    if (name.empty()) {
      return false;
    }
    return name[0] == '_';
  }

  static bool IsAllowedName(velocypack::Slice parameters);
  static bool IsAllowedName(bool isSystem, std::string const& name);

  // TODO: MOVE TO PHYSICAL?
  bool isFullyCollected(); //should not be exposed


  // SECTION: Meta Information
  uint32_t version() const { return _version; }

  void setVersion(CollectionVersions version) { _version = version; }

  uint32_t internalVersion() const;

  inline TRI_voc_cid_t cid() const { return _cid; }

  std::string cid_as_string() const;

  TRI_voc_cid_t planId() const;

  TRI_col_type_e type() const;

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }

  std::string name() const;
  std::string dbName() const;
  std::string const& distributeShardsLike() const;
  void distributeShardsLike(std::string const&);

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

  void executeWhileStatusLocked(std::function<void()> const& callback);
  bool tryExecuteWhileStatusLocked(std::function<void()> const& callback);

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always
  /// TRI_VOC_COL_STATUS_CORRUPTED
  TRI_vocbase_col_status_e tryFetchStatus(bool&);
  std::string statusString();

  TRI_voc_tick_t maxTick() const { return _maxTick; }
  void maxTick(TRI_voc_tick_t value) { _maxTick = value; }

  uint64_t numberDocuments() const;

  // TODO this should be part of physical collection!
  size_t journalSize() const;

  // SECTION: Properties
  TRI_voc_rid_t revision() const;
  bool isLocal() const;
  bool deleted() const;
  bool doCompact() const;
  bool isSystem() const;
  bool isVolatile() const;
  bool waitForSync() const;
  bool isSmart() const;

  void waitForSync(bool value) { _waitForSync = value; }

  std::unique_ptr<FollowerInfo> const& followers() const;

  void setDeleted(bool);

  Ditches* ditches() const;

  // SECTION: Key Options
  velocypack::Slice keyOptions() const;

  // Get a reference to this KeyGenerator.
  // Caller is not allowed to free it.
  inline KeyGenerator* keyGenerator() const {
    return _keyGenerator.get();
  }

  PhysicalCollection* getPhysical() const { return _physical.get(); }

  // SECTION: Indexes
  uint32_t indexBuckets() const;

  std::vector<std::shared_ptr<Index>> const& getIndexes() const;

  // WARNING: Make sure that this LogicalCollection Instance
  // is somehow protected. If it goes out of all scopes
  // or it's indexes are freed the pointer returned will get invalidated.
  MMFilesPrimaryIndex* primaryIndex() const;

  // Adds all properties to the builder (has to be an open object)
  // Does not add Shards or Indexes
  void getPropertiesVPack(velocypack::Builder&,
                          bool translateCids) const;
  
  void getIndexesVPack(velocypack::Builder&, bool) const;

  // SECTION: Replication
  int replicationFactor() const;
  bool isSatellite() const;

  // SECTION: Sharding
  int numberOfShards() const;
  bool allowUserKeys() const;
  virtual bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;
  std::shared_ptr<ShardMap> shardIds() const;
  void setShardMap(std::shared_ptr<ShardMap>& map);

  /// @brief a method to skip certain documents in AQL write operations,
  /// this is only used in the enterprise edition for smart graphs
  virtual bool skipForAqlWrite(velocypack::Slice document,
                               std::string const& key) const;

  // SECTION: Modification Functions
  int rename(std::string const&);
  void unload();
  virtual void drop();

  virtual void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialisation
  void toVelocyPack(velocypack::Builder&, bool withPath) const;
  virtual void toVelocyPackForAgency(velocypack::Builder&);
  virtual void toVelocyPackForClusterInventory(velocypack::Builder&,
                                               bool useSystem) const;

  /// @brief transform the information for this collection to velocypack
  ///        The builder has to be an opened Type::Object
  void toVelocyPack(velocypack::Builder&, bool, TRI_voc_tick_t);

  inline TRI_vocbase_t* vocbase() const { return _vocbase; }

  // Update this collection.
  virtual int updateProperties(velocypack::Slice const&, bool);

  /// @brief return the figures for a collection
  virtual std::shared_ptr<velocypack::Builder> figures();

  /// @brief opens an existing collection
  void open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();

  /// datafile management

  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal();

  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid,
                   DatafileStatisticsContainer const& values);

  bool applyForTickRange(
      TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
      std::function<bool(TRI_voc_tick_t foundTick,
                         TRI_df_marker_t const* marker)> const& callback);

  void sizeHint(transaction::Methods* trx, int64_t hint);

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  virtual std::shared_ptr<Index> createIndex(
      transaction::Methods*, velocypack::Slice const&, bool&);

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(TRI_idx_iid_t) const;

  // SECTION: Indexes (local only)

  /// @brief Detect all indexes form file
  int detectIndexes(transaction::Methods* trx);

  /// @brief Exposes a pointer to index list
  std::vector<std::shared_ptr<Index>> const* indexList() const;

 bool dropIndex(TRI_idx_iid_t iid, bool writeMarker);

  // SECTION: Index access (local only)

  int read(transaction::Methods*, std::string const&,
           ManagedDocumentResult& result, bool);
  int read(transaction::Methods*, StringRef const&,
           ManagedDocumentResult& result, bool);

  /// @brief processes a truncate operation
  /// NOTE: This function throws on error
  void truncate(transaction::Methods* trx, OperationOptions&);

  int insert(transaction::Methods*, velocypack::Slice const,
             ManagedDocumentResult& result, OperationOptions&,
             TRI_voc_tick_t&, bool);
  int update(transaction::Methods*, velocypack::Slice const,
             ManagedDocumentResult& result, OperationOptions&,
             TRI_voc_tick_t&, bool, TRI_voc_rid_t& prevRev,
             ManagedDocumentResult& previous);
  int replace(transaction::Methods*, velocypack::Slice const,
              ManagedDocumentResult& result, OperationOptions&,
              TRI_voc_tick_t&, bool, TRI_voc_rid_t& prevRev,
              ManagedDocumentResult& previous);
  int remove(transaction::Methods*, velocypack::Slice const,
             OperationOptions&, TRI_voc_tick_t&, bool,
             TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous);

  bool readDocument(transaction::Methods*, ManagedDocumentResult& result,
                    DocumentIdentifierToken const& token);
  bool readDocumentConditional(transaction::Methods*,
                               ManagedDocumentResult& result,
                               DocumentIdentifierToken const& token,
                               TRI_voc_tick_t maxTick, bool excludeWal);

  bool readRevision(transaction::Methods*, ManagedDocumentResult& result,
                    TRI_voc_rid_t revisionId);
  bool readRevisionConditional(transaction::Methods*,
                               ManagedDocumentResult& result,
                               TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick,
                               bool excludeWal);

 private:
  // SECTION: Index creation

  /// @brief creates the initial indexes for the collection
  void createInitialIndexes();

  bool removeIndex(TRI_idx_iid_t iid);

 public:
  // TODO Fix Visibility
  void addIndex(std::shared_ptr<Index>);
 private:
  void addIndexCoordinator(std::shared_ptr<Index>, bool);

  // SECTION: Indexes (local only)

  // @brief create index with the given definition.
  bool openIndex(velocypack::Slice const&, transaction::Methods*);

  // SECTION: Document pre commit preperation (only local)

  /// @brief new object for insert, value must have _key set correctly.
  int newObjectForInsert(transaction::Methods* trx,
                         velocypack::Slice const& value,
                         velocypack::Slice const& fromSlice,
                         velocypack::Slice const& toSlice,
                         bool isEdgeCollection,
                         velocypack::Builder& builder,
                         bool isRestore);

 public: // TODO FIXME
 /// @brief new object for remove, must have _key set
  void newObjectForRemove(transaction::Methods* trx,
                          velocypack::Slice const& oldValue,
                          std::string const& rev,
                          velocypack::Builder& builder);
 private:
  void increaseInternalVersion();

 protected:
  void toVelocyPackInObject(velocypack::Builder& result,
                            bool translateCids) const;

  // SECTION: Meta Information
  //
  // @brief Internal version used for caching
  uint32_t _internalVersion;

  // @brief Local collection id
  TRI_voc_cid_t const _cid;

  // @brief Global collection id
  TRI_voc_cid_t const _planId;

  // @brief Collection type
  TRI_col_type_e const _type;

  // @brief Collection Name
  std::string _name;

  // @brief Name of other collection this shards should be distributed like
  std::string _distributeShardsLike;

  // @brief Name of other collection this shards should be distributed like
  std::vector<std::string> _avoidServers;

  // @brief Flag if this collection is a smart one. (Enterprise only)
  bool _isSmart;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<FollowerInfo> _followers;

  // @brief Current state of this colletion
  TRI_vocbase_col_status_e _status;

  // SECTION: Properties
  bool _isLocal;
  bool _isDeleted;
  bool _doCompact;
  bool const _isSystem;
  bool const _isVolatile;
  bool _waitForSync;
  TRI_voc_size_t _journalSize;

  // SECTION: Key Options
  // TODO Really VPack?
  std::shared_ptr<velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation

  uint32_t _version;

  // SECTION: Indexes
  uint32_t _indexBuckets;

  std::vector<std::shared_ptr<Index>> _indexes;

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

  TRI_vocbase_t* _vocbase;

  // SECTION: Local Only has to be moved to PhysicalCollection
 public:
  // TODO MOVE ME
  size_t _cleanupIndexes;
  size_t _persistentIndexes;
 protected:

  std::unique_ptr<PhysicalCollection> _physical;

  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;

  TRI_voc_tick_t _maxTick;

  std::unique_ptr<KeyGenerator> _keyGenerator;

  mutable basics::ReadWriteLock
      _lock;  // lock protecting the status and name

  mutable basics::ReadWriteLock
      _infoLock;  // lock protecting the info
};

}  // namespace arangodb

#endif
