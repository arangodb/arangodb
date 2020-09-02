////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/hashes.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Sharding/ShardingInfo.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {

enum class Part : uint8_t { ALL, FRONT, BACK };

void preventUseOnSmartEdgeCollection(LogicalCollection const* collection,
                                     std::string const& strategyName) {
  if (collection->isSmart() && collection->type() == TRI_COL_TYPE_EDGE) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        std::string("sharding strategy ") + strategyName +
            " cannot be used for smart edge collections");
  }
}

inline void parseAttributeAndPart(std::string const& attr, arangodb::velocypack::StringRef& realAttr, Part& part) {
  if (!attr.empty() && attr.back() == ':') {
    realAttr = arangodb::velocypack::StringRef(attr.data(), attr.size() - 1);
    part = Part::FRONT;
  } else if (!attr.empty() && attr.front() == ':') {
    realAttr = arangodb::velocypack::StringRef(attr.data() + 1, attr.size() - 1);
    part = Part::BACK;
  } else {
    realAttr = arangodb::velocypack::StringRef(attr.data(), attr.size());
    part = Part::ALL;
  }
}

template <bool returnNullSlice>
VPackSlice buildTemporarySlice(VPackSlice const sub, Part const& part,
                               VPackBuilder& temporaryBuilder, bool splitSlash) {
  if (sub.isString()) {
    arangodb::velocypack::StringRef key(sub);
    if (splitSlash) {
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        // We have an _id. Split it.
        key = key.substr(pos + 1);
      } else {
        splitSlash = false;
      }
    }
    switch (part) {
      case Part::ALL: {
        if (!splitSlash) {
          return sub;
        }
        // by adding the key to the builder, we may invalidate the original key...
        // however, this is safe here as the original key is not used after we have
        // added to the builder
        return VPackSlice(temporaryBuilder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String)));
      }
      case Part::FRONT: {
        size_t pos = key.find(':');
        if (pos != std::string::npos) {
          key = key.substr(0, pos);
          // by adding the key to the builder, we may invalidate the original key...
          // however, this is safe here as the original key is not used after we have
          // added to the builder
          return VPackSlice(temporaryBuilder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String)));
        }
        // fall-through to returning null or original slice
        break;
      }
      case Part::BACK: {
        size_t pos = key.rfind(':');
        if (pos != std::string::npos) {
          key = key.substr(pos + 1);
          // by adding the key to the builder, we may invalidate the original key...
          // however, this is safe here as the original key is not used after we have
          // added to the builder
          return VPackSlice(temporaryBuilder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String)));
        }
        // fall-through to returning null or original slice
        break;
      }
    }
  }

  if (returnNullSlice) {
    return VPackSlice::nullSlice();
  }
  return sub;
}

template <bool returnNullSlice>
uint64_t hashByAttributesImpl(VPackSlice slice, std::vector<std::string> const& attributes,
                              bool docComplete, int& error, VPackStringRef const& key) {
  uint64_t hashval = TRI_FnvHashBlockInitial();
  error = TRI_ERROR_NO_ERROR;
  slice = slice.resolveExternal();
  
  VPackBuffer<uint8_t> buffer;
  VPackBuilder temporaryBuilder(buffer);

  if (slice.isObject()) {
    for (auto const& attr : attributes) {
      
      arangodb::velocypack::StringRef realAttr;
      ::Part part;
      ::parseAttributeAndPart(attr, realAttr, part);
      VPackSlice sub = slice.get(realAttr).resolveExternal();
      if (sub.isNone()) {
        // shard key attribute not present in document
        if (realAttr == StaticStrings::KeyString && !key.empty()) {
          temporaryBuilder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
          sub = temporaryBuilder.slice();
        } else {
          if (!docComplete) {
            error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
          }
          // Null is equal to None/not present
          sub = VPackSlice::nullSlice();
        }
      }
      // buildTemporarySlice may append data to the builder, which may invalidate
      // the original "sub" value. however, "sub" is reassigned immediately with
      // a new value, so it does not matter in reality
      sub = ::buildTemporarySlice<returnNullSlice>(sub, part, temporaryBuilder,
                                                   /*splitSlash*/false);
      hashval = sub.normalizedHash(hashval);
      temporaryBuilder.clear();
    }
    
    return hashval;
    
  } else if (slice.isString()) {
    
    // optimization for `_key` and `_id` with default sharding
    if (attributes.size() == 1) {
      arangodb::velocypack::StringRef realAttr;
      ::Part part;
      ::parseAttributeAndPart(attributes[0], realAttr, part);
      if (realAttr == StaticStrings::KeyString) {
        TRI_ASSERT(key.empty());
        
        // We always need the _key part. Everything else should be ignored
        // beforehand.
        VPackSlice sub =
        ::buildTemporarySlice<returnNullSlice>(slice, part, temporaryBuilder,
                                               /*splitSlash*/true);
        return sub.normalizedHash(hashval);
      }
    }
    
    if (!docComplete) { // ok for use in update, replace and remove operation
      error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
      return hashval;
    }
  }
  
  // we can only get here if a developer calls this wrongly.
  // allowed cases are either and object or (as an optimization)
  // `_key` or `_id` string values and default sharding
  
  TRI_ASSERT(false);
  error = TRI_ERROR_BAD_PARAMETER;
  
  return hashval;
}

}  // namespace

std::string const ShardingStrategyNone::NAME("none");
std::string const ShardingStrategyCommunityCompat::NAME("community-compat");
std::string const ShardingStrategyEnterpriseCompat::NAME("enterprise-compat");
std::string const ShardingStrategyHash::NAME("hash");

/// @brief a sharding class used for single server and the DB servers
/// calling getResponsibleShard on this class will always throw an exception
ShardingStrategyNone::ShardingStrategyNone() : ShardingStrategy() {
  if (ServerState::instance()->isCoordinator()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, std::string("sharding strategy ") + NAME +
                                     " cannot be used for sharded collections");
  }
}

/// calling getResponsibleShard on this class will always throw an exception
int ShardingStrategyNone::getResponsibleShard(arangodb::velocypack::Slice slice,
                                              bool docComplete, ShardID& shardID,
                                              bool& usesDefaultShardKeys,
                                              VPackStringRef const& key) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "unexpected invocation of ShardingStrategyNone");
}

/// @brief a sharding class used to indicate that the selected sharding strategy
/// is only available in the Enterprise Edition of ArangoDB
/// calling getResponsibleShard on this class will always throw an exception
/// with an appropriate error message
ShardingStrategyOnlyInEnterprise::ShardingStrategyOnlyInEnterprise(std::string const& name)
    : ShardingStrategy(), _name(name) {}

/// @brief will always throw an exception telling the user the selected sharding
/// is only available in the Enterprise Edition
int ShardingStrategyOnlyInEnterprise::getResponsibleShard(arangodb::velocypack::Slice slice,
                                                          bool docComplete, ShardID& shardID,
                                                          bool& usesDefaultShardKeys,
                                                          VPackStringRef const& key) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ONLY_ENTERPRISE,
      std::string("sharding strategy '") + _name +
          "' is only available in the Enterprise Edition of ArangoDB");
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid shard keys");
  }
  for (auto const& it : shardKeys) {
    if (it.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid shard keys");
    }
  }
}

int ShardingStrategyHashBase::getResponsibleShard(arangodb::velocypack::Slice slice,
                                                  bool docComplete, ShardID& shardID,
                                                  bool& usesDefaultShardKeys,
                                                  VPackStringRef const& key) {
  static constexpr char const* magicPhrase =
      "Foxx you have stolen the goose, give she back again!";
  static constexpr size_t magicLength = 52;

  determineShards();
  TRI_ASSERT(!_shards.empty());

  TRI_ASSERT(!_sharding->shardKeys().empty());

  int res = TRI_ERROR_NO_ERROR;
  usesDefaultShardKeys = _usesDefaultShardKeys;
  // calls virtual "hashByAttributes" function

  uint64_t hashval = hashByAttributes(slice, _sharding->shardKeys(), docComplete, res, key);
  // To improve our hash function result:
  hashval = TRI_FnvHashBlock(hashval, magicPhrase, magicLength);
  shardID = _shards[hashval % _shards.size()];
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
  auto& ci =
      _sharding->collection()->vocbase().server().getFeature<ClusterFeature>().clusterInfo();
  auto shards = ci.getShardList(std::to_string(_sharding->collection()->id().id()));

  _shards = *shards;

  if (_shards.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid shard count");
  }

  TRI_ASSERT(!_shards.empty());
  _shardsSet = true;
}

uint64_t ShardingStrategyHashBase::hashByAttributes(VPackSlice slice,
                                                    std::vector<std::string> const& attributes,
                                                    bool docComplete, int& error,
                                                    VPackStringRef const& key) {
  return ::hashByAttributesImpl<false>(slice, attributes, docComplete, error, key);
}

/// @brief old version of the sharding used in the Community Edition
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

  ::preventUseOnSmartEdgeCollection(_sharding->collection(), NAME);
}

/// @brief old version of the sharding used in the Enterprise Edition
/// this is DEPRECATED and should not be used for new collections
ShardingStrategyEnterpriseBase::ShardingStrategyEnterpriseBase(ShardingInfo* sharding)
    : ShardingStrategyHashBase(sharding) {
  // whether or not the collection uses the default shard attributes (["_key"])
  // this setting is initialized to false, and we may change it now
  TRI_ASSERT(!_usesDefaultShardKeys);
  auto shardKeys = _sharding->shardKeys();
  TRI_ASSERT(!shardKeys.empty());

  if (shardKeys.size() == 1) {
    _usesDefaultShardKeys =
        (shardKeys[0] == StaticStrings::KeyString ||
         (shardKeys[0][0] == ':' &&
          shardKeys[0].compare(1, shardKeys[0].size() - 1, StaticStrings::KeyString) == 0) ||
         (shardKeys[0].back() == ':' &&
          shardKeys[0].compare(0, shardKeys[0].size() - 1, StaticStrings::KeyString) == 0));
  }
}

/// @brief this implementation of "hashByAttributes" is slightly different
/// than the implementation in the Community Edition
/// we leave the differences in place, because making any changes here
/// will affect the data distribution, which we want to avoid
uint64_t ShardingStrategyEnterpriseBase::hashByAttributes(
    VPackSlice slice, std::vector<std::string> const& attributes,
    bool docComplete, int& error, VPackStringRef const& key) {
  return ::hashByAttributesImpl<true>(slice, attributes, docComplete, error, key);
}

/// @brief old version of the sharding used in the Enterprise Edition
/// this is DEPRECATED and should not be used for new collections
ShardingStrategyEnterpriseCompat::ShardingStrategyEnterpriseCompat(ShardingInfo* sharding)
    : ShardingStrategyEnterpriseBase(sharding) {
  ::preventUseOnSmartEdgeCollection(_sharding->collection(), NAME);
}

/// @brief default hash-based sharding strategy
/// used for new collections from 3.4 onwards
ShardingStrategyHash::ShardingStrategyHash(ShardingInfo* sharding)
    : ShardingStrategyHashBase(sharding) {
  // whether or not the collection uses the default shard attributes (["_key"])
  // this setting is initialized to false, and we may change it now
  TRI_ASSERT(!_usesDefaultShardKeys);
  auto shardKeys = _sharding->shardKeys();
  TRI_ASSERT(!shardKeys.empty());

  if (shardKeys.size() == 1) {
    _usesDefaultShardKeys =
        (shardKeys[0] == StaticStrings::KeyString ||
         (shardKeys[0][0] == ':' &&
          shardKeys[0].compare(1, shardKeys[0].size() - 1, StaticStrings::KeyString) == 0) ||
         (shardKeys[0].back() == ':' &&
          shardKeys[0].compare(0, shardKeys[0].size() - 1, StaticStrings::KeyString) == 0));
  }

  ::preventUseOnSmartEdgeCollection(_sharding->collection(), NAME);
}
