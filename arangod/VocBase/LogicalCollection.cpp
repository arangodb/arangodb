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

#include "LogicalCollection.h"

#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/collection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

template <typename T>
static T ReadNumericValue(VPackSlice info, std::string const& name, T def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getNumericValue<T>(info, name.c_str(), def);
}

static bool ReadBooleanValue(VPackSlice info, std::string const& name,
                             bool def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getBooleanValue(info, name.c_str(), def);
}

static std::string const ReadStringValue(VPackSlice info,
                                         std::string const& name,
                                         std::string const& def) {
  if (!info.isObject()) {
    return def;
  }
  return Helper::getStringValue(info, name, def);
}

static std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> CopySliceValue(VPackSlice info,
    std::string const& name) {
  if (!info.isObject()) {
    return nullptr;
  }
  info = info.get(name);
  if (!info.isObject()) {
    return nullptr;
  }
  return VPackBuilder::clone(info).steal();
}

static int GetObjectLength(VPackSlice info, std::string const& name, int def) {
  if (!info.isObject()) {
    return def;
  }
  info = info.get(name);
  if (!info.isObject()) {
    return def;
  }
  return static_cast<int>(info.length());
}

// @brief Constructor used in DBServer/SingleServer case.
LogicalCollection::LogicalCollection(TRI_vocbase_t* vocbase,
                                     TRI_col_type_e type, TRI_voc_cid_t cid,
                                     std::string const& name,
                                     TRI_voc_cid_t planId,
                                     std::string const& path, bool isLocal)
    : _internalVersion(0),
      _cid(cid),
      _planId(planId),
      _type(type),
      _name(name),
      _status(TRI_VOC_COL_STATUS_CORRUPTED),
      _isLocal(isLocal),
      // THESE VALUES HAVE ARBITRARY VALUES. FIX THEM.
      _isDeleted(false),
      _doCompact(false),
      _isSystem(TRI_collection_t::IsSystemName(name)),
      _isVolatile(false),
      _waitForSync(false),
      _keyOptions(nullptr),
      _indexBuckets(1),
      _replicationFactor(0),
      _numberOfShards(1),
      _allowUserKeys(true),
      _shardIds(new ShardMap()),
      _vocbase(vocbase),
      // END OF ARBITRARY
      _path(path),
      _physical(nullptr),
      _collection(nullptr),
      _lock() {}

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(TRI_vocbase_t* vocbase, VPackSlice info)
    : _internalVersion(0),
      _cid(basics::StringUtils::uint64(ReadStringValue(info, "id", "0"))),
      _planId(_cid),
      _type(
          ReadNumericValue<TRI_col_type_e>(info, "type", TRI_COL_TYPE_UNKNOWN)),
      _name(ReadStringValue(info, "name", "")),
      _status(ReadNumericValue<TRI_vocbase_col_status_e>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
      _isLocal(false),
      _isDeleted(ReadBooleanValue(info, "deleted", false)),
      _doCompact(ReadBooleanValue(info, "doCompact", false)),
      _isSystem(ReadBooleanValue(info, "isSystem", false)),
      _isVolatile(ReadBooleanValue(info, "isVolatile", false)),
      _waitForSync(ReadBooleanValue(info, "waitForSync", false)),
      _keyOptions(CopySliceValue(info, "keyOptions")),
      _indexBuckets(ReadNumericValue<uint32_t>(info, "indexBuckets", 1)),
      _indexes(CopySliceValue(info, "indexes")),
      _replicationFactor(ReadNumericValue<int>(info, "replicationFactor", 1)),
      _numberOfShards(GetObjectLength(info, "shards", 0)),
      _allowUserKeys(ReadBooleanValue(info, "allowUserKeys", true)),
      _shardIds(new ShardMap()),
      _vocbase(vocbase),
      _physical(nullptr),
      _collection(nullptr),
      _lock() {
  if (info.isObject()) {
    // Otherwise the cluster communication is broken.
    // We cannot store anything further.
    auto shardKeysSlice = info.get("shardKeys");
    if (shardKeysSlice.isArray()) {
      for (auto const& shardKey : VPackArrayIterator(shardKeysSlice)) {
        _shardKeys.push_back(shardKey.copyString());
      }
    }

    auto shardsSlice = info.get("shards");
    if (shardsSlice.isObject()) {
      for (auto const& shardSlice : VPackObjectIterator(shardsSlice)) {
        if (shardSlice.key.isString() && shardSlice.value.isArray()) {
          ShardID shard = shardSlice.key.copyString();

          std::vector<ServerID> servers;
          for (auto const& serverSlice : VPackArrayIterator(shardSlice.value)) {
            servers.push_back(serverSlice.copyString());
          }
          _shardIds->emplace(shard, servers);
        }
      }
    }
  }
}

LogicalCollection::~LogicalCollection() {
  // TODO Do we have to free _physical
}

size_t LogicalCollection::journalSize() const {
  // TODO FIXME should be part of physical collection
  return 0;
}

// SECTION: Meta Information

uint32_t LogicalCollection::version() const {
  return _internalVersion;
}

TRI_voc_cid_t LogicalCollection::cid() const {
  return _cid;
}

std::string LogicalCollection::cid_as_string() const {
  return basics::StringUtils::itoa(_cid);
}

TRI_voc_cid_t LogicalCollection::planId() const {
  return _planId;
}

TRI_col_type_e LogicalCollection::type() const {
  return _type;
}

std::string const& LogicalCollection::name() const {
  // TODO Activate this lock. Right now we have some locks outside.
  // READ_LOCKER(readLocker, _lock);
  return _name;
}

std::string const& LogicalCollection::dbName() const {
  TRI_ASSERT(_vocbase != nullptr);
  return _vocbase->name();
}

std::string const& LogicalCollection::path() const {
  return _path;
}

TRI_vocbase_col_status_e LogicalCollection::status() {
  return _status;
}

TRI_vocbase_col_status_e LogicalCollection::getStatusLocked() {
  READ_LOCKER(readLocker, _lock);
  return _status;
}

TRI_vocbase_col_status_e LogicalCollection::tryFetchStatus(bool& didFetch) {
  TRY_READ_LOCKER(locker, _lock);
  if (locker.isLocked()) {
    didFetch = true;
    return _status;
  }
  didFetch = false;
  return TRI_VOC_COL_STATUS_CORRUPTED;
}

/// @brief returns a translation of a collection status
std::string const LogicalCollection::statusString() {
  READ_LOCKER(readLocker, _lock);
  switch (_status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
      return "unloaded";
    case TRI_VOC_COL_STATUS_LOADED:
      return "loaded";
    case TRI_VOC_COL_STATUS_UNLOADING:
      return "unloading";
    case TRI_VOC_COL_STATUS_DELETED:
      return "deleted";
    case TRI_VOC_COL_STATUS_LOADING:
      return "loading";
    case TRI_VOC_COL_STATUS_CORRUPTED:
    case TRI_VOC_COL_STATUS_NEW_BORN:
    default:
      return "unknown";
  }
}

// SECTION: Properties

bool LogicalCollection::isLocal() const {
  return _isLocal;
}

bool LogicalCollection::deleted() const {
  return _isDeleted;
}

bool LogicalCollection::doCompact() const {
  return _doCompact;
}

bool LogicalCollection::isSystem() const {
  return _isSystem;
}

bool LogicalCollection::isVolatile() const {
  return _isVolatile;
}

bool LogicalCollection::waitForSync() const {
  return _waitForSync;
}


// SECTION: Key Options
VPackSlice LogicalCollection::keyOptions() const {
  // TODO Maybe we can directly include the KeyGenerator here?!
  return VPackSlice(_keyOptions->data());
}

// SECTION: Indexes
uint32_t LogicalCollection::indexBuckets() const {
  return _indexBuckets;
}

VPackSlice LogicalCollection::getIndexes() const {
  // TODO Maybe we can get a list of IDX Handles here?
  return VPackSlice(_indexes->data());
}

// SECTION: Replication

int LogicalCollection::replicationFactor() const {
  return _replicationFactor;
}

// SECTION: Sharding
int LogicalCollection::numberOfShards() const {
  return _numberOfShards;
}

bool LogicalCollection::allowUserKeys() const {
  return _allowUserKeys;
}

bool LogicalCollection::usesDefaultShardKeys() const {
  return (_shardKeys.size() == 1 && _shardKeys[0] == StaticStrings::KeyString);
}

std::vector<std::string> const& LogicalCollection::shardKeys() const {
  return _shardKeys;
}

std::shared_ptr<ShardMap> LogicalCollection::shardIds() const {
  // TODO make threadsafe update on the cache.
  return _shardIds;
}

// SECTION: Modification Functions
int LogicalCollection::rename(std::string const& newName) {
  // Should only be called from inside vocbase.
  // Otherwise caching is destroyed.
  TRI_ASSERT(!ServerState::instance()->isCoordinator()); // NOT YET IMPLEMENTED

  WRITE_LOCKER_EVENTUAL(locker, _lock, 1000);

  // Check for illeagal states.
  switch (_status) {
    case TRI_VOC_COL_STATUS_CORRUPTED:
      return TRI_ERROR_ARANGO_CORRUPTED_COLLECTION;
    case TRI_VOC_COL_STATUS_DELETED:
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    default:
      // Fall through intentional
      break;
  }

  // Check for duplicate name
  auto other = _vocbase->lookupCollection(newName);
  if (other != nullptr) {
    return TRI_ERROR_ARANGO_DUPLICATE_NAME;
  }

  // actually rename.
  switch (_status) {
    case TRI_VOC_COL_STATUS_UNLOADED:
      // Nothing to do for this state
      break;
    case TRI_VOC_COL_STATUS_LOADED:
    case TRI_VOC_COL_STATUS_UNLOADING:
    case TRI_VOC_COL_STATUS_LOADING: {
      // TODO This will be removed. _collection ain't dead yet.
      TRI_ASSERT(_collection != nullptr);
      int res = _collection->rename(newName);

      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
      break;
    }
    default:
      // Unknown status
      return TRI_ERROR_INTERNAL;
  }


  // Okay we can finally rename safely
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->renameCollection(_vocbase, _cid, newName); 
  _name = newName;

  // CHECK if this ordering is okay. Before change the version was increased after swapping in vocbase mapping.
  increaseVersion();
  return TRI_ERROR_NO_ERROR;
}

void LogicalCollection::drop() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  _isDeleted = true;
}

void LogicalCollection::setStatus(TRI_vocbase_col_status_e status) {
  _status = status;

  if (status == TRI_VOC_COL_STATUS_LOADED) {
    _internalVersion = 0;
  } else if (status == TRI_VOC_COL_STATUS_UNLOADED) {
    _collection = nullptr;
  }
}

void LogicalCollection::toVelocyPack(VPackBuilder& result) const {
  result.openObject();
  result.add("id", VPackValue(_cid));
  result.add("name", VPackValue(_name));
  result.add("status", VPackValue(_status));
  result.add("deleted", VPackValue(_isDeleted));
  result.add("doCompact", VPackValue(_doCompact));
  result.add("isSystem", VPackValue(_isSystem));
  result.add("isVolatile", VPackValue(_isVolatile));
  result.add("waitForSync", VPackValue(_waitForSync));
  result.add("keyOptions", VPackSlice(_keyOptions->data()));
  result.add("indexBuckets", VPackValue(_indexBuckets));
  result.add("indexes", VPackSlice(_indexes->data()));
  result.add("replicationFactor", VPackValue(_replicationFactor));
  result.add(VPackValue("shards"));
  result.openObject();
  for (auto const& shards : *_shardIds) {
    result.add(VPackValue(shards.first));
    result.openArray();
    for (auto const& servers : shards.second) {
      result.add(VPackValue(servers));
    }
    result.close(); // server array
  }
  result.close(); // shards
  result.add("allowUserKeys", VPackValue(_allowUserKeys));
  result.add(VPackValue("shardKeys"));
  result.openArray();
  for (auto const& key : _shardKeys) {
    result.add(VPackValue(key));
  }
  result.close(); // shardKeys
  result.close(); // Base Object
}

void LogicalCollection::toVelocyPack(VPackBuilder& builder, bool includeIndexes,
                                     TRI_voc_tick_t maxTick) {
  TRI_ASSERT(!builder.isClosed());
  
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  engine->getCollectionInfo(_vocbase, _cid, builder, includeIndexes, maxTick);
}


TRI_vocbase_t* LogicalCollection::vocbase() const {
  return _vocbase;
}

void LogicalCollection::increaseVersion() {
  ++_internalVersion;
}

int LogicalCollection::update(VPackSlice const&, bool, TRI_vocbase_t const*) {
  return TRI_ERROR_NOT_IMPLEMENTED;
  /*
  return ClusterInfo::instance()->setCollectionPropertiesCoordinator(
                databaseName, StringUtils::itoa(collection->_cid), this);
                */

}
