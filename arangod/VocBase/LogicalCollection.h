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
#include "VocBase/PhysicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>

struct TRI_df_marker_t;

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace wal {
struct DocumentOperation;
class Marker;
}

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class CollectionRevisionsCache;
struct DatafileStatisticsContainer;
class Ditches;
class FollowerInfo;
class Index;
class KeyGenerator;
class ManagedDocumentResult;
struct OperationOptions;
class PhysicalCollection;
class PrimaryIndex;
class StringRef;
class Transaction;

class LogicalCollection {
  friend struct ::TRI_vocbase_t;

 public:
  LogicalCollection(TRI_vocbase_t*, arangodb::velocypack::Slice const&, bool isPhysical);

  virtual ~LogicalCollection();
  
  enum CollectionVersions {
    VERSION_30 = 5,
    VERSION_31 = 6
  };

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

  static bool IsAllowedName(arangodb::velocypack::Slice parameters);
  static bool IsAllowedName(bool isSystem, std::string const& name);

  void ensureRevisionsCache();
      
  void isInitialIteration(bool value) { _isInitialIteration = value; }

  // TODO: MOVE TO PHYSICAL?  
  bool isFullyCollected();
  int64_t uncollectedLogfileEntries() const { return _uncollectedLogfileEntries.load(); }
  
  void increaseUncollectedLogfileEntries(int64_t value) {
    _uncollectedLogfileEntries += value;
  }

  void decreaseUncollectedLogfileEntries(int64_t value) {
    _uncollectedLogfileEntries -= value;
    if (_uncollectedLogfileEntries < 0) {
      _uncollectedLogfileEntries = 0;
    }
  }

  void setNextCompactionStartIndex(size_t);
  size_t getNextCompactionStartIndex();
  void setCompactionStatus(char const*);
  double lastCompactionStamp() const { return _lastCompactionStamp; }
  void lastCompactionStamp(double value) { _lastCompactionStamp = value; }

  void setRevisionError() { _revisionError = true; }
  

  // SECTION: Meta Information
  uint32_t version() const { 
    return _version; 
  }
  
  void setVersion(CollectionVersions version) { _version = version; }

  uint32_t internalVersion() const;

  inline TRI_voc_cid_t cid() const {
    return _cid;
  }

  std::string cid_as_string() const;

  TRI_voc_cid_t planId() const;

  TRI_col_type_e type() const;

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }
  
  std::string name() const;
  std::string dbName() const;
  std::string const& path() const;
  std::string const& distributeShardsLike() const;
  void distributeShardsLike(std::string const&);

  // For normal collections the realNames is just a vector of length 1
  // with its name. For smart edge collections (enterprise only) this is
  // different.
  virtual std::vector<std::string> realNames() const {
    std::vector<std::string> res {name()};
    return res;
  }
  // Same here, this is for reading in AQL:
  virtual std::vector<std::string> realNamesForRead() const {
    std::vector<std::string> res {name()};
    return res;
  }

  TRI_vocbase_col_status_e status() const;
  TRI_vocbase_col_status_e getStatusLocked();

  void executeWhileStatusLocked(std::function<void()> const& callback);
  bool tryExecuteWhileStatusLocked(std::function<void()> const& callback);

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always TRI_VOC_COL_STATUS_CORRUPTED 
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

  std::unique_ptr<arangodb::FollowerInfo> const& followers() const;
  
  void setDeleted(bool);

  Ditches* ditches() const {
    return getPhysical()->ditches();
  }

  void setRevision(TRI_voc_rid_t, bool);

  // SECTION: Key Options
  arangodb::velocypack::Slice keyOptions() const;

  // Get a reference to this KeyGenerator.
  // Caller is not allowed to free it.
  inline arangodb::KeyGenerator* keyGenerator() const {
    return _keyGenerator.get();
  }
  
  PhysicalCollection* getPhysical() const { return _physical.get(); }

  // SECTION: Indexes
  uint32_t indexBuckets() const;

  std::vector<std::shared_ptr<arangodb::Index>> const& getIndexes() const;

  // WARNING: Make sure that this LogicalCollection Instance
  // is somehow protected. If it goes out of all scopes
  // or it's indexes are freed the pointer returned will get invalidated.
  arangodb::PrimaryIndex* primaryIndex() const;
  void getIndexesVPack(arangodb::velocypack::Builder&, bool) const;

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
  virtual bool skipForAqlWrite(arangodb::velocypack::Slice document,
                               std::string const& key) const;

  // SECTION: Modification Functions
  int rename(std::string const&);
  void unload();
  virtual void drop();

  virtual void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialisation
  void toVelocyPack(arangodb::velocypack::Builder&, bool withPath) const;
  virtual void toVelocyPackForAgency(arangodb::velocypack::Builder&);

  /// @brief transform the information for this collection to velocypack
  ///        The builder has to be an opened Type::Object
  void toVelocyPack(arangodb::velocypack::Builder&, bool, TRI_voc_tick_t);

  inline TRI_vocbase_t* vocbase() const {
    return _vocbase;
  }

  // Update this collection.
  virtual int update(arangodb::velocypack::Slice const&, bool);

  /// @brief return the figures for a collection
  virtual std::shared_ptr<arangodb::velocypack::Builder> figures();
  
  /// @brief opens an existing collection
  void open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();

  /// datafile management

  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal() {
    return getPhysical()->rotateActiveJournal();
  }
  
  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) {
    return getPhysical()->updateStats(fid, values);
  }
  
  bool applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                         std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) {
    return getPhysical()->applyForTickRange(dataMin, dataMax, callback);
  }
  
  /// @brief disallow starting the compaction of the collection
  void preventCompaction() { getPhysical()->preventCompaction(); }
  bool tryPreventCompaction() { return getPhysical()->tryPreventCompaction(); }
  /// @brief re-allow starting the compaction of the collection
  void allowCompaction() { getPhysical()->allowCompaction(); }

  /// @brief compaction finished
  void lockForCompaction() { getPhysical()->lockForCompaction(); }
  bool tryLockForCompaction() { return getPhysical()->tryLockForCompaction(); }
  void finishCompaction() { getPhysical()->finishCompaction(); }

  void sizeHint(arangodb::Transaction* trx, int64_t hint);

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  virtual std::shared_ptr<arangodb::Index> createIndex(
      arangodb::Transaction*, arangodb::velocypack::Slice const&, bool&);

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(arangodb::velocypack::Slice const&) const;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(TRI_idx_iid_t) const;

  // SECTION: Indexes (local only)

  /// @brief Detect all indexes form file
  int detectIndexes(arangodb::Transaction* trx);

  /// @brief Restores an index from VelocyPack.
  int restoreIndex(arangodb::Transaction*, arangodb::velocypack::Slice const&,
                   std::shared_ptr<arangodb::Index>&);

  /// @brief Fill indexes used in recovery
  int fillIndexes(arangodb::Transaction*);

  /// @brief Saves Index to file
  int saveIndex(arangodb::Index* idx, bool writeMarker);

  bool dropIndex(TRI_idx_iid_t iid, bool writeMarker);

  int cleanupIndexes();

  // SECTION: Index access (local only)
  
  int read(arangodb::Transaction*, std::string const&, ManagedDocumentResult& result, bool);
  int read(arangodb::Transaction*, arangodb::StringRef const&, ManagedDocumentResult& result, bool);

  /// @brief processes a truncate operation (note: currently this only clears
  /// the read-cache
  int truncate(Transaction* trx);
  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const,
             ManagedDocumentResult& result, arangodb::OperationOptions&, TRI_voc_tick_t&, bool);
  int update(arangodb::Transaction*, arangodb::velocypack::Slice const,
             ManagedDocumentResult& result, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous);
  int replace(arangodb::Transaction*, arangodb::velocypack::Slice const,
             ManagedDocumentResult& result, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const,
             arangodb::OperationOptions&, TRI_voc_tick_t&, bool, 
             TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous);
  /// @brief removes a document or edge, fast path function for database documents
  int remove(arangodb::Transaction*, TRI_voc_rid_t oldRevisionId, arangodb::velocypack::Slice const,
             arangodb::OperationOptions&, TRI_voc_tick_t&, bool);

  int rollbackOperation(arangodb::Transaction*, TRI_voc_document_operation_e, 
                        TRI_voc_rid_t oldRevisionId, arangodb::velocypack::Slice const& oldDoc,
                        TRI_voc_rid_t newRevisionId, arangodb::velocypack::Slice const& newDoc);

  // TODO Make Private and IndexFiller as friend
  /// @brief initializes an index with all existing documents
  int fillIndex(arangodb::Transaction*, arangodb::Index*,
                bool skipPersistent = true);

  int beginReadTimed(bool useDeadlockDetector, uint64_t, uint64_t);
  int beginWriteTimed(bool useDeadlockDetector, uint64_t, uint64_t);
  int endRead(bool useDeadlockDetector);
  int endWrite(bool useDeadlockDetector);
  
  bool readRevision(arangodb::Transaction*, ManagedDocumentResult& result, TRI_voc_rid_t revisionId);
  bool readRevisionConditional(arangodb::Transaction*, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal);

  void insertRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal);
  void updateRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal);
  bool updateRevisionConditional(TRI_voc_rid_t revisionId, TRI_df_marker_t const* oldPosition, TRI_df_marker_t const* newPosition, TRI_voc_fid_t newFid, bool isInWal);
  void removeRevision(TRI_voc_rid_t revisionId, bool updateStats);

 private:
  // SECTION: Index creation

  /// @brief creates the initial indexes for the collection
  void createInitialIndexes();

  int openWorker(bool ignoreErrors);

  bool removeIndex(TRI_idx_iid_t iid);

  void addIndex(std::shared_ptr<arangodb::Index>);
  void addIndexCoordinator(std::shared_ptr<arangodb::Index>, bool);

  // SECTION: Indexes (local only)

  // @brief create index with the given definition.

  bool openIndex(arangodb::velocypack::Slice const&, arangodb::Transaction*);
  /// @brief fill an index in batches
  int fillIndexBatch(arangodb::Transaction* trx, arangodb::Index* idx);

  /// @brief fill an index sequentially
  int fillIndexSequential(arangodb::Transaction* trx, arangodb::Index* idx);

  // SECTION: Index access (local only)
  int lookupDocument(arangodb::Transaction*, VPackSlice const,
                     ManagedDocumentResult& result);

  int checkRevision(arangodb::Transaction*, TRI_voc_rid_t expected, TRI_voc_rid_t found);

  int updateDocument(arangodb::Transaction*, 
                     TRI_voc_rid_t oldRevisionId, arangodb::velocypack::Slice const& oldDoc,
                     TRI_voc_rid_t newRevisionId, arangodb::velocypack::Slice const& newDoc,
                     arangodb::wal::DocumentOperation&, arangodb::wal::Marker const*,
                     bool& waitForSync);
  int insertDocument(arangodb::Transaction*, TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&,
                     arangodb::wal::DocumentOperation&, arangodb::wal::Marker const*,
                     bool& waitForSync);

  int insertPrimaryIndex(arangodb::Transaction*, TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&);
  
  int deletePrimaryIndex(arangodb::Transaction*, TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&);

  int insertSecondaryIndexes(arangodb::Transaction*, TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&,
                             bool isRollback);

  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_voc_rid_t revisionId, arangodb::velocypack::Slice const&,
                             bool isRollback);

  // SECTION: Document pre commit preperation (only local)

  /// @brief new object for insert, value must have _key set correctly.
  int newObjectForInsert(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& value,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      arangodb::velocypack::Builder& builder,
      bool isRestore);

  /// @brief new object for replace
  void newObjectForReplace(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);

  /// @brief merge two objects for update
  void mergeObjectsForUpdate(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      bool isEdgeCollection,
      std::string const& rev,
      bool mergeObjects, bool keepNull,
      arangodb::velocypack::Builder& b);

  /// @brief new object for remove, must have _key set
  void newObjectForRemove(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);

  void increaseInternalVersion();

 protected:
  void toVelocyPackInObject(arangodb::velocypack::Builder& result) const;

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

  // @brief Flag if this collection is a smart one. (Enterprise only)

  bool _isSmart;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<arangodb::FollowerInfo> _followers;

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
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation
  
  uint32_t _version;

  // SECTION: Indexes
  uint32_t _indexBuckets;

  std::vector<std::shared_ptr<arangodb::Index>> _indexes;

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

  // SECTION: Local Only
  size_t _cleanupIndexes;
  size_t _persistentIndexes;
  std::string _path;

  std::unique_ptr<PhysicalCollection> _physical;
  std::unique_ptr<CollectionRevisionsCache> _revisionsCache;

  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;

  TRI_voc_tick_t _maxTick;

  std::unique_ptr<arangodb::KeyGenerator> _keyGenerator;
  
  mutable arangodb::basics::ReadWriteLock
      _lock;  // lock protecting the status and name
 
  mutable arangodb::basics::ReadWriteLock
      _idxLock;  // lock protecting the indexes

  mutable arangodb::basics::ReadWriteLock
      _infoLock;  // lock protecting the info
  
  arangodb::Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  double _lastCompactionStamp;
  
  std::atomic<int64_t> _uncollectedLogfileEntries;

  /// @brief: flag that is set to true when the documents are
  /// initial enumerated and the primary index is built
  bool _isInitialIteration;

  bool _revisionError;
};

}  // namespace arangodb

#endif
