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

  // CTOR needs planId, cid
  explicit LogicalCollection(arangodb::velocypack::Slice);

  ~LogicalCollection();

  // SECTION: Meta Information
  TRI_voc_cid_t cid() const;
  std::string cid_as_string() const;

  TRI_voc_cid_t planId() const;

  TRI_col_type_e type() const;

  std::string const& name() const;

  TRI_vocbase_col_status_e status() const;
  std::string const statusString() const;

  // TODO this should be part of physical collection!
  size_t journalSize() const;

  // SECTION: Properties
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
  void rename(std::string const&);
  void drop();


  // SECTION: Serialisation
  void toVelocyPack(arangodb::velocypack::Builder&) const;

  TRI_vocbase_t* vocbase() const;

  // Only Local
  void updateCount(size_t);
  // Path will be taken from physical
  // Probably this can be handled internally only!
  int saveToFile(bool) const;

  // Update this collection.
  int update(arangodb::velocypack::Slice const&, bool, TRI_vocbase_t const*);
  int update(VocbaseCollectionInfo const&);

  PhysicalCollection* getPhysical() const;

 private:
  // SECTION: Private variables

  // SECTION: Meta Information

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
  bool _isDeleted;
  bool _doCompact;
  bool const _isSystem;
  bool const _isVolatile;
  bool const _waitForSync;

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

  PhysicalCollection* _physical;
};
}  // namespace arangodb

#endif
