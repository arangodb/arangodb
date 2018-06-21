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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ShardingStrategyDefault.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/hashes.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ShardingInfo.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

std::string const ShardingStrategyNone::NAME("none");
std::string const ShardingStrategyCommunity::NAME("community");

int ShardingStrategyNone::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              std::string const& key) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected invocation of ShardingStrategyNone");
}

/// @brief base class for hash-based sharding
ShardingStrategyHash::ShardingStrategyHash(ShardingInfo* sharding)
    : ShardingStrategy(),
      _sharding(sharding),
      _shards(),
      _usesDefaultShardKeys(false) {

  auto shardKeys = _sharding->shardKeys();
    
  // validate shard keys
  if (shardKeys.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard keys");
  }
  for (auto const& it : shardKeys) {
    if (it.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard keys");
    }
  }
}

int ShardingStrategyHash::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              std::string const& key) {
  static constexpr char const* magicPhrase = "Foxx you have stolen the goose, give she back again!";
  static constexpr size_t magicLength = 52;

  determineShards();

  int res = TRI_ERROR_NO_ERROR;
  usesDefaultShardKeys = _usesDefaultShardKeys;
  // calls virtual "hashByAttributes" function
  uint64_t hash = hashByAttributes(slice, _sharding->shardKeys(), docComplete, res, key);
  // To improve our hash function result:
  hash = TRI_FnvHashBlock(hash, magicPhrase, magicLength);
  shardID = _shards[hash % _shards.size()];
  return res;
}

void ShardingStrategyHash::determineShards() {
  if (!_shards.empty()) {
    return;
  }

  // determine all available shards (which will stay const afterwards)
  auto ci = ClusterInfo::instance();
  auto shards = ci->getShardList(std::to_string(_sharding->collection()->id()));

  _shards = *shards;

  if (_shards.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard count");
  }
}

/// @brief old version of the sharding used in the community edition
/// this is DEPRECATED and should not be used for new collections
ShardingStrategyCommunity::ShardingStrategyCommunity(ShardingInfo* sharding)
    : ShardingStrategyHash(sharding) {
  // whether or not the collection uses the default shard attributes (["_key"])
  TRI_ASSERT(_usesDefaultShardKeys);
  
  auto shardKeys = _sharding->shardKeys();
  if (shardKeys.size() == 1 && shardKeys[0] == StaticStrings::KeyString) {
    _usesDefaultShardKeys = true;
  }
}

uint64_t ShardingStrategyCommunity::hashByAttributes(
    VPackSlice slice, std::vector<std::string> const& attributes,
    bool docComplete, int& error, std::string const& key) {

  uint64_t hash = TRI_FnvHashBlockInitial();
  error = TRI_ERROR_NO_ERROR;
  slice = slice.resolveExternal();
  if (slice.isObject()) {
    for (auto const& attr : attributes) {
      VPackSlice sub = slice.get(attr).resolveExternal();
      if (sub.isNone()) {
        if (attr == StaticStrings::KeyString && !key.empty()) {
          VPackBuilder tempBuilder;
          tempBuilder.add(VPackValue(key));
          sub = tempBuilder.slice();
        } else {
          if (!docComplete) {
            error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
          }
          // Null is equal to None/not present
          sub = VPackSlice::nullSlice();
        }
      }
      hash = sub.normalizedHash(hash);
    }
  } else if (slice.isString() && attributes.size() == 1 &&
             attributes[0] == StaticStrings::KeyString) {
    arangodb::StringRef subKey(slice);
    size_t pos = subKey.find('/');
    if (pos != std::string::npos) {
      // We have an _id. Split it.
      subKey = subKey.substr(pos + 1);
      VPackBuilder tempBuilder;
      tempBuilder.add(VPackValuePair(subKey.data(), subKey.length(), VPackValueType::String));
      VPackSlice tmp = tempBuilder.slice();
      hash = tmp.normalizedHash(hash);
    } else {
      hash = slice.normalizedHash(hash);
    }
  }

  return hash;
}
