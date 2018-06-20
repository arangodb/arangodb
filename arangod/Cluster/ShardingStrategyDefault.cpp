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
#include "Basics/VelocyPackHelper.h"
#include "Basics/hashes.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

int ShardingStrategyNone::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              std::string const& key) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected invocation of ShardingStrategyNone");
}

/// @brief base class for hash-based sharding
ShardingStrategyHash::ShardingStrategyHash(LogicalCollection* collection)
    : ShardingStrategy(),
      _collection(collection),
      _usesDefaultShardKeys(false) {
  
  // validate shard keys
  if (_collection->shardKeys().empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard keys");
  }
  for (auto const& it : _collection->shardKeys()) {
    if (it.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard keys");
    }
  }

  // whether or not the collection uses the default shard attributes (["_key"])
  _usesDefaultShardKeys = ShardingStrategy::usesDefaultShardKeys(_collection->shardKeys());

  // determine all available shards (which will stay const afterwards)
  auto shardIds = _collection->shardIds();

  if (shardIds->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard count");
  }

  for (auto const& it : *shardIds) {
    _shards.emplace_back(it.first);
  }
}

int ShardingStrategyHash::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              std::string const& key) {
  static constexpr char const* magicPhrase = "Foxx you have stolen the goose, give she back again!";
  static constexpr size_t magicLength = 52;

  int res = TRI_ERROR_NO_ERROR;
  usesDefaultShardKeys = this->usesDefaultShardKeys();
  // calls virtual "hashByAttributes" function
  uint64_t hash = hashByAttributes(slice, _collection->shardKeys(), docComplete, res, key);
  // To improve our hash function result:
  hash = TRI_FnvHashBlock(hash, magicPhrase, magicLength);
  shardID = _shards[hash % _shards.size()];
  return res;
}

/// @brief old version of the sharding used in the community edition
/// this is DEPRECATED and should not be used for new collections
ShardingStrategyCommunity::ShardingStrategyCommunity(LogicalCollection* collection)
    : ShardingStrategyHash(collection) {}

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
