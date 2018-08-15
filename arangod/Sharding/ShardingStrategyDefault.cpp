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
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/hashes.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Sharding/ShardingInfo.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {

enum class Part : uint8_t { ALL, FRONT, BACK };

inline void parseAttributeAndPart(std::string const& attr,
                                  std::string& realAttr, Part& part) {
  if (attr.size() > 0 && attr.back() == ':') {
    realAttr = attr.substr(0, attr.size() - 1);
    part = Part::FRONT;
  } else if (attr.size() > 0 && attr.front() == ':') {
    realAttr = attr.substr(1);
    part = Part::BACK;
  } else {
    realAttr = attr;
    part = Part::ALL;
  }
}

VPackSlice buildTemporarySlice(VPackSlice const& sub, Part const& part,
                               VPackBuilder& temporaryBuilder,
                               bool splitSlash) {
  if (sub.isString()) {
    switch (part) {
      case Part::ALL: {
        if (splitSlash) {
          arangodb::StringRef key(sub);
          size_t pos = key.find('/');
          if (pos != std::string::npos) {
            // We have an _id. Split it.
            key = key.substr(pos + 1);
            temporaryBuilder.clear();
            temporaryBuilder.add(VPackValue(key.toString()));
            return temporaryBuilder.slice();
          }
        }
        return sub;
      }
      case Part::FRONT: {
        arangodb::StringRef prefix(sub);
        size_t pos;
        if (splitSlash) {
          pos = prefix.find('/');
          if (pos != std::string::npos) {
            // We have an _id. Split it.
            prefix = prefix.substr(pos + 1);
          }
        }
        pos = prefix.find(':');
        if (pos != std::string::npos) {
          prefix = prefix.substr(0, pos);
          temporaryBuilder.clear();
          temporaryBuilder.add(VPackValue(prefix.toString()));
          return temporaryBuilder.slice();
        }
        break;
      }
      case Part::BACK: {
        std::string prefix = sub.copyString();
        size_t pos = prefix.rfind(':');
        if (pos != std::string::npos) {
          temporaryBuilder.clear();
          temporaryBuilder.add(VPackValue(prefix.substr(pos + 1)));
          return temporaryBuilder.slice();
        }
        break;
      }
    }
  }
  return sub;
}

}

std::string const ShardingStrategyNone::NAME("none");
std::string const ShardingStrategyCommunityCompat::NAME("community-compat");
std::string const ShardingStrategyHash::NAME("hash");

ShardingStrategyNone::ShardingStrategyNone()
    : ShardingStrategy() {

  if (ServerState::instance()->isCoordinator()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("sharding strategy ") + NAME + " cannot be used for sharded collections");
  }
}

int ShardingStrategyNone::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              std::string const& key) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected invocation of ShardingStrategyNone");
}

/// @brief base class for hash-based sharding
ShardingStrategyHashBase::ShardingStrategyHashBase(ShardingInfo* sharding)
    : ShardingStrategy(),
      _sharding(sharding),
      _shards(),
      _usesDefaultShardKeys(false),
      _shardsSet(false) {

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

int ShardingStrategyHashBase::getResponsibleShard(arangodb::velocypack::Slice slice,
                                                  bool docComplete, ShardID& shardID,
                                                  bool& usesDefaultShardKeys,
                                                  std::string const& key) {
  static constexpr char const* magicPhrase = "Foxx you have stolen the goose, give she back again!";
  static constexpr size_t magicLength = 52;

  determineShards();
  TRI_ASSERT(!_shards.empty());

  TRI_ASSERT(!_sharding->shardKeys().empty());

  int res = TRI_ERROR_NO_ERROR;
  usesDefaultShardKeys = _usesDefaultShardKeys;
  // calls virtual "hashByAttributes" function

  uint64_t hash = hashByAttributes(slice, _sharding->shardKeys(), docComplete, res, key);
  // To improve our hash function result:
  hash = TRI_FnvHashBlock(hash, magicPhrase, magicLength);
  shardID = _shards[hash % _shards.size()];
  return res;
}

void ShardingStrategyHashBase::determineShards() {
  if (_shardsSet) {
    TRI_ASSERT(!_shards.empty());
    return;
  }

  MUTEX_LOCKER(mutex, _shardsSetMutex);
  if (_shardsSet) {
    TRI_ASSERT(!_shards.empty());
    return;
  }

  // determine all available shards (which will stay const afterwards)
  auto ci = ClusterInfo::instance();
  auto shards = ci->getShardList(std::to_string(_sharding->collection()->id()));

  _shards = *shards;

  if (_shards.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid shard count");
  }

  TRI_ASSERT(!_shards.empty());
  _shardsSet = true;
}

uint64_t ShardingStrategyHashBase::hashByAttributes(
    VPackSlice slice, std::vector<std::string> const& attributes,
    bool docComplete, int& error, std::string const& key) {

  uint64_t hash = TRI_FnvHashBlockInitial();
  error = TRI_ERROR_NO_ERROR;
  slice = slice.resolveExternal();
  if (slice.isObject()) {
    std::string realAttr;
    ::Part part;
    for (auto const& attr : attributes) {
      ::parseAttributeAndPart(attr, realAttr, part);
      VPackSlice sub = slice.get(realAttr).resolveExternal();
      VPackBuilder temporaryBuilder;
      if (sub.isNone()) {
        if (realAttr == StaticStrings::KeyString && !key.empty()) {
          temporaryBuilder.add(VPackValue(key));
          sub = temporaryBuilder.slice();
        } else {
          if (!docComplete) {
            error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
          }
          // Null is equal to None/not present
          sub = VPackSlice::nullSlice();
        }
      }
      sub = ::buildTemporarySlice(sub, part, temporaryBuilder, false);
      hash = sub.normalizedHash(hash);
    }
  } else if (slice.isString() && attributes.size() == 1) {
    std::string realAttr;
    ::Part part;
    ::parseAttributeAndPart(attributes[0], realAttr, part);
    if (realAttr == StaticStrings::KeyString && key.empty()) {
      // We always need the _key part. Everything else should be ignored
      // beforehand.
      VPackBuilder temporaryBuilder;
      VPackSlice sub =
          ::buildTemporarySlice(slice, part, temporaryBuilder, true);
      hash = sub.normalizedHash(hash);
    }
  }

  return hash;
}

/// @brief old version of the sharding used in the community edition
/// this is DEPRECATED and should not be used for new collections
ShardingStrategyCommunityCompat::ShardingStrategyCommunityCompat(ShardingInfo* sharding)
    : ShardingStrategyHashBase(sharding) {
  // whether or not the collection uses the default shard attributes (["_key"])
  // this setting is initialized to false, and we may change it now
  TRI_ASSERT(!_usesDefaultShardKeys);
  auto shardKeys = _sharding->shardKeys();
  if (shardKeys.size() == 1 && shardKeys[0] == StaticStrings::KeyString) {
    _usesDefaultShardKeys = true;
  }

  if (_sharding->collection()->isSmart() &&
      _sharding->collection()->type() == TRI_COL_TYPE_EDGE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("sharding strategy ") + NAME + " cannot be used for smart edge collection");
  }
}

/// @brief default hash-based sharding strategy
ShardingStrategyHash::ShardingStrategyHash(ShardingInfo* sharding)
    : ShardingStrategyHashBase(sharding) {
  // whether or not the collection uses the default shard attributes (["_key"])
  // this setting is initialized to false, and we may change it now
  TRI_ASSERT(!_usesDefaultShardKeys);
  auto shardKeys = _sharding->shardKeys();
  if (shardKeys.size() == 1) {
    _usesDefaultShardKeys =
        (shardKeys[0] == StaticStrings::KeyString ||
         (shardKeys[0][0] == ':' &&
          shardKeys[0].compare(1, shardKeys[0].size() - 1,
                               StaticStrings::KeyString) == 0) ||
         (shardKeys[0].back() == ':' &&
          shardKeys[0].compare(0, shardKeys[0].size() - 1,
                               StaticStrings::KeyString) == 0));
  }

  if (_sharding->collection()->isSmart() &&
      _sharding->collection()->type() == TRI_COL_TYPE_EDGE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("sharding strategy ") + NAME + " cannot be used for smart edge collection");
  }
}
