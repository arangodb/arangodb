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

#include "Basics/StringUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

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

/*
LogicalCollection::LogicalCollection() :
  _cid(0),
  _planId(0),
  _type(TRI_COL_TYPE_UNKNOWN),
  _name(""),
  _status(TRI_VOC_COL_STATUS_CORRUPTED),
  _isDeleted(true),
  _doCompact(false),
  _isSystem(false),
  _isVolatile(false),
  _waitForSync(false),
  _keyOptions(nullptr),
  _indexBuckets(0),
  _indexes(nullptr),
  _replicationFactor(0),
  _numberOfShards(0),
  _allowUserKeys(false),
  _physical(nullptr) {
  // TODO FIXME
}
*/

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this collection.
LogicalCollection::LogicalCollection(VPackSlice info)
    : _cid(basics::StringUtils::uint64(ReadStringValue(info, "id", "0"))),
      _planId(_cid),
      _type(
          ReadNumericValue<TRI_col_type_e>(info, "type", TRI_COL_TYPE_UNKNOWN)),
      _name(ReadStringValue(info, "name", "")),
      _status(ReadNumericValue<TRI_vocbase_col_status_e>(
          info, "status", TRI_VOC_COL_STATUS_CORRUPTED)),
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
      _physical(nullptr) {
  // TODO read shard id map
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
          (*_shardIds).emplace(shard, servers);
        }
      }
    }
  }
}

// SECTION: Meta Information

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
  return _name;
}

TRI_vocbase_col_status_e LogicalCollection::status() const {
  return _status;
}

/// @brief returns a translation of a collection status
std::string const LogicalCollection::statusString() const {
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
void LogicalCollection::rename(std::string const& newName) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  _name = newName;
}

void LogicalCollection::drop() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  _isDeleted = true;
}
