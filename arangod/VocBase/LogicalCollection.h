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
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/PhysicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>

struct TRI_datafile_t;
struct TRI_df_marker_t;

namespace arangodb {
namespace velocypack {
class Slice;
}

namespace wal {
struct DocumentOperation;
}

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class FollowerInfo;
class Index;
class KeyGenerator;
struct OperationOptions;
class PhysicalCollection;
class PrimaryIndex;
class StringRef;
class Transaction;

class LogicalCollection {
 public:
  LogicalCollection(
      TRI_vocbase_t* vocbase, TRI_col_type_e type, TRI_voc_cid_t cid,
      std::string const& name, TRI_voc_cid_t planId, std::string const& path,
      std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>& keyOpts,
      bool isVolatile, bool isLocal);

  LogicalCollection(TRI_vocbase_t*, arangodb::velocypack::Slice);

  explicit LogicalCollection(std::shared_ptr<LogicalCollection> const&);

  ~LogicalCollection();

  LogicalCollection(LogicalCollection const&) = delete;
  LogicalCollection& operator=(LogicalCollection const&) = delete;
  LogicalCollection() = delete;

  /// @brief determine whether a collection name is a system collection name
  static inline bool IsSystemName(std::string const& name) {
    if (name.empty()) {
      return false;
    }
    return name[0] == '_';
  }

  static bool IsAllowedName(arangodb::velocypack::Slice parameters);

  // SECTION: Meta Information
  uint32_t version() const;

  TRI_voc_cid_t cid() const;
  std::string cid_as_string() const;

  TRI_voc_cid_t planId() const;

  TRI_col_type_e type() const;

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }
  
  std::string name() const;
  std::string dbName() const;
  std::string const& path() const;

  TRI_vocbase_col_status_e status();
  TRI_vocbase_col_status_e getStatusLocked();

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always TRI_VOC_COL_STATUS_CORRUPTED 
  TRI_vocbase_col_status_e tryFetchStatus(bool&);
  std::string statusString();

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
  
  void waitForSync(bool value) { _waitForSync = value; }

  std::unique_ptr<arangodb::FollowerInfo> const& followers() const;

  void setDeleted(bool);
  void setRevision(TRI_voc_rid_t, bool);

  // SECTION: Key Options
  arangodb::velocypack::Slice keyOptions() const;

  // Get a reference to this KeyGenerator.
  // Caller is not allowed to free it.
  arangodb::KeyGenerator* keyGenerator() const;

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


  // SECTION: Sharding
  int numberOfShards() const;
  bool allowUserKeys() const;
  bool usesDefaultShardKeys() const;
  std::vector<std::string> const& shardKeys() const;
  std::shared_ptr<ShardMap> shardIds() const;

  // SECTION: Modification Functions
  int rename(std::string const&);
  void drop();

  void setStatus(TRI_vocbase_col_status_e);

  // SECTION: Serialisation
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  /// @brief transform the information for this collection to velocypack
  ///        The builder has to be an opened Type::Object
  void toVelocyPack(arangodb::velocypack::Builder&, bool, TRI_voc_tick_t);

  TRI_vocbase_t* vocbase() const;

  // Only Local
  void updateCount(size_t);

  // Update this collection.
  void increaseVersion();
  int update(arangodb::velocypack::Slice const&, bool);

  /// @brief return the figures for a collection
  std::shared_ptr<arangodb::velocypack::Builder> figures();
  
  
  /// @brief opens an existing collection
  int open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();

  /// datafile management

  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal() {
    return getPhysical()->rotateActiveJournal();
  }
  
  /// @brief iterates over a collection
  bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& callback) {
    return getPhysical()->iterateDatafiles(callback);
  }
  
  /// @brief increase dead stats for a datafile, if it exists
  void increaseDeadStats(TRI_voc_fid_t fid, int64_t number, int64_t size) {
    return getPhysical()->increaseDeadStats(fid, number, size);
  }
  
  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) {
    return getPhysical()->updateStats(fid, values);
  }
  
  /// @brief create statistics for a datafile, using the stats provided
  void createStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) {
    return getPhysical()->createStats(fid, values);
  }

  
  int applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
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


  PhysicalCollection* getPhysical() const {
    TRI_ASSERT(_physical != nullptr);
    return _physical;
  }

  // SECTION: Indexes

  /// @brief Create a new Index based on VelocyPack description
  std::shared_ptr<arangodb::Index> createIndex(
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
  
  int read(arangodb::Transaction*, std::string const&, TRI_doc_mptr_t*, bool);
  int read(arangodb::Transaction*, arangodb::StringRef const&, TRI_doc_mptr_t*, bool);
  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool);
  int update(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int replace(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const,
             arangodb::OperationOptions&, TRI_voc_tick_t&, bool, 
             VPackSlice&, TRI_doc_mptr_t&);

  int rollbackOperation(arangodb::Transaction*, TRI_voc_document_operation_e, 
                        TRI_doc_mptr_t*, TRI_doc_mptr_t const*);

  // TODO Make Private and IndexFiller als friend
  /// @brief initializes an index with all existing documents
  int fillIndex(arangodb::Transaction*, arangodb::Index*,
                bool skipPersistent = true);

  int beginRead();
  int endRead();
  int beginWrite();
  int endWrite();
  int beginReadTimed(uint64_t, uint64_t);
  int beginWriteTimed(uint64_t, uint64_t);



 private:
  // SECTION: Private functions

  PhysicalCollection* createPhysical();

  // SECTION: Index creation

  /// @brief creates the initial indexes for the collection
 public:
  // FIXME Should be private
  int createInitialIndexes();
 private:

  bool removeIndex(TRI_idx_iid_t iid);

  // TODO: THROW OR RETURN ERROR?
  void addIndex(std::shared_ptr<arangodb::Index>);
  void addIndexCoordinator(arangodb::velocypack::Slice);

  // SECTION: Indexes (local only)

  // @brief create index with the given definition.

  bool openIndex(arangodb::velocypack::Slice const&, arangodb::Transaction*);
  /// @brief fill an index in batches
  int fillIndexBatch(arangodb::Transaction* trx, arangodb::Index* idx);

  /// @brief fill an index sequentially
  int fillIndexSequential(arangodb::Transaction* trx, arangodb::Index* idx);

  // SECTION: Index access (local only)
  int lookupDocument(arangodb::Transaction*, VPackSlice const,
                     TRI_doc_mptr_t*&);

  int checkRevision(arangodb::Transaction*, arangodb::velocypack::Slice const,
                    arangodb::velocypack::Slice const);

  int updateDocument(arangodb::Transaction*, TRI_voc_rid_t, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&,
                     TRI_doc_mptr_t*, bool&);
  int insertDocument(arangodb::Transaction*, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&, TRI_doc_mptr_t*,
                     bool&);

  int insertPrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t*);
 public:
  // FIXME needs to be private
  int deletePrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t const*);
 private:

  int insertSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

  // SECTION: Document pre commit preperation (only local)

  /// @brief new object for insert, value must have _key set correctly.
  int newObjectForInsert(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& value,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      uint64_t& hash,
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



 private:
  // SECTION: Private variables

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

  // SECTION: Indexes
  uint32_t _indexBuckets;

  std::vector<std::shared_ptr<arangodb::Index>> _indexes;

  // SECTION: Replication
  int const _replicationFactor;

 private:
  // SECTION: Sharding
  int const _numberOfShards;
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
  PhysicalCollection* _physical;

 public:
  // FIXME Must be private. OpenIterator uses this.
  arangodb::MasterPointers _masterPointers;

 private:
  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;

  // FIXME Both of them are not initialized properly!
 public:
  // FIXME Must be private. OpenIterator uses this.
  int64_t _numberDocuments;
 private:
  std::unique_ptr<arangodb::KeyGenerator> _keyGenerator;

  // TODO REMOVE ME!
 public:
  TRI_collection_t* _collection;

  mutable arangodb::basics::ReadWriteLock
      _lock;  // lock protecting the status and name
  mutable arangodb::basics::ReadWriteLock
      _idxLock;  // lock protecting the indexes
  mutable arangodb::basics::ReadWriteLock
      _infoLock;  // lock protecting the info
};

}  // namespace arangodb

#endif
