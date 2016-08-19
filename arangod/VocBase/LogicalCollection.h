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
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>

namespace arangodb {
namespace velocypack {
class Slice;
}

typedef std::string ServerID;      // ID of a server
typedef std::string DatabaseID;    // ID/name of a database
typedef std::string CollectionID;  // ID of a collection
typedef std::string ShardID;       // ID of a shard
typedef std::unordered_map<ShardID, std::vector<ServerID>> ShardMap;

class PhysicalCollection;

class LogicalCollection {
 public:

  LogicalCollection(TRI_vocbase_t* vocbase, TRI_col_type_e type,
                    TRI_voc_cid_t cid, std::string const& name, TRI_voc_cid_t planId,
                    std::string const& path, bool isLocal);

  LogicalCollection(TRI_vocbase_t*, arangodb::velocypack::Slice);

  LogicalCollection(std::shared_ptr<LogicalCollection> const);

  ~LogicalCollection();

  LogicalCollection(LogicalCollection const&) = delete;
  LogicalCollection& operator=(LogicalCollection const&) = delete;
  LogicalCollection() = delete;

  // SECTION: Meta Information
  uint32_t version() const;

  TRI_voc_cid_t cid() const;
  std::string cid_as_string() const;

  TRI_voc_cid_t planId() const;

  TRI_col_type_e type() const;

  std::string name() const;
  std::string dbName() const;
  std::string const& path() const;

  TRI_vocbase_col_status_e status();
  TRI_vocbase_col_status_e getStatusLocked();

  /// @brief try to fetch the collection status under a lock
  /// the boolean value will be set to true if the lock could be acquired
  /// if the boolean is false, the return value is always TRI_VOC_COL_STATUS_CORRUPTED 
  TRI_vocbase_col_status_e tryFetchStatus(bool&);
  std::string const statusString();

  // TODO this should be part of physical collection!
  size_t journalSize() const;

  // SECTION: Properties
  bool isLocal() const;
  bool deleted() const;
  bool doCompact() const;
  bool isSystem() const;
  bool isVolatile() const;
  bool waitForSync() const;

  // SECTION: Key Options
  arangodb::velocypack::Slice keyOptions() const;

  // SECTION: Indexes
  uint32_t indexBuckets() const;

  arangodb::velocypack::Slice getIndexes() const;

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
  // Path will be taken from physical
  // Probably this can be handled internally only!
  int saveToFile(bool) const;

  // Update this collection.
  void increaseVersion();
  int update(arangodb::velocypack::Slice const&, bool, TRI_vocbase_t const*);
  int update(VocbaseCollectionInfo const&);

  PhysicalCollection* getPhysical() const;

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

  // @brief Current state of this colletion
  TRI_vocbase_col_status_e _status;

  // SECTION: Properties
  bool _isLocal;
  bool _isDeleted;
  bool _doCompact;
  bool const _isSystem;
  bool const _isVolatile;
  bool const _waitForSync;
  TRI_voc_size_t const _journalSize;

  // SECTION: Key Options
  // TODO Really VPack?
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation

  // SECTION: Indexes
  uint32_t const _indexBuckets;

  // TODO Really VPack?
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _indexes;  // options for key creation


  // SECTION: Replication
  int const _replicationFactor;

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
  std::string const _path;
  PhysicalCollection* _physical;

// TODO REMOVE ME!
 public:
  TRI_collection_t* _collection;

  mutable arangodb::basics::ReadWriteLock _lock;  // lock protecting the status and name

};
}  // namespace arangodb

#endif
