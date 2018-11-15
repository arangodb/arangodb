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

#include "ShardingInfo.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Sharding/ShardingFeature.h"
#include "Sharding/ShardingStrategy.h"
#include "Sharding/ShardingStrategyDefault.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

ShardingInfo::ShardingInfo(arangodb::velocypack::Slice info, LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(basics::VelocyPackHelper::readNumericValue<size_t>(info, StaticStrings::NumberOfShards, 1)),
      _replicationFactor(1),
      _distributeShardsLike(basics::VelocyPackHelper::getStringValue(info, StaticStrings::DistributeShardsLike, "")),
      _avoidServers(),
      _shardKeys(),
      _shardIds(new ShardMap()),
      _shardingStrategy() {

  bool const isSmart = basics::VelocyPackHelper::readBooleanValue(info, StaticStrings::IsSmart, false);

  if (isSmart && _collection->type() == TRI_COL_TYPE_EDGE) {
    // smart edge collection
    _numberOfShards = 0;
  }

  VPackSlice shardKeysSlice = info.get(StaticStrings::ShardKeys);
  if (ServerState::instance()->isCoordinator()) {
    if ((_numberOfShards == 0 && !isSmart) || _numberOfShards > 1000) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid number of shards");
    }
  }
      
  VPackSlice v = info.get(StaticStrings::NumberOfShards);
  if (!v.isNone() && !v.isNumber() && !v.isNull()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid number of shards");
  }

  if (info.hasKey("avoidServers")) {
    auto avoidServersSlice = info.get("avoidServers");
    if (avoidServersSlice.isArray()) {
      for (const auto& i : VPackArrayIterator(avoidServersSlice)) {
        if (i.isString()) {
          _avoidServers.push_back(i.copyString());
        } else {
          LOG_TOPIC(ERR, arangodb::Logger::FIXME)
              << "avoidServers must be a vector of strings we got "
              << avoidServersSlice.toJson() << ". discarding!";
          _avoidServers.clear();
          break;
        }
      }
    }
  }

  auto replicationFactorSlice = info.get(StaticStrings::ReplicationFactor);
  if (!replicationFactorSlice.isNone()) {
    bool isError = true;
    if (replicationFactorSlice.isNumber()) {
      _replicationFactor = replicationFactorSlice.getNumber<size_t>();
      // mop: only allow satellite collections to be created explicitly
      if (_replicationFactor > 0 && _replicationFactor <= 10) {
        isError = false;
#ifdef USE_ENTERPRISE
      } else if (_replicationFactor == 0) {
        isError = false;
#endif
      }
    }
#ifdef USE_ENTERPRISE
    else if (replicationFactorSlice.isString() &&
             replicationFactorSlice.copyString() == "satellite") {
      _replicationFactor = 0;
      _numberOfShards = 1;
      _distributeShardsLike = "";
      _avoidServers.clear();
      isError = false;
    }
#endif
    if (isError) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid replicationFactor");
    }
  }

  // replicationFactor == 0 -> satellite collection
  if (shardKeysSlice.isNone() || _replicationFactor == 0) {
    // Use default.
    _shardKeys.emplace_back(StaticStrings::KeyString);
  } else {
    if (shardKeysSlice.isArray()) {
      for (auto const& sk : VPackArrayIterator(shardKeysSlice)) {
        if (sk.isString()) {
          std::string key = sk.copyString();
          // remove : char at the beginning or end (for enterprise)
          std::string stripped;
          if (!key.empty()) {
            if (key.front() == ':') {
              stripped = key.substr(1);
            } else if (key.back() == ':') {
              stripped = key.substr(0, key.size() - 1);
            } else {
              stripped = key;
            }
          }
          // system attributes are not allowed (except _key)
          if (!stripped.empty() && stripped != StaticStrings::IdString &&
              stripped != StaticStrings::RevString) {
            _shardKeys.emplace_back(key);
          }
        }
      }
      if (_shardKeys.empty()) { // && !isCluster) {
        // Compatibility. Old configs might store empty shard-keys locally.
        // This is translated to ["_key"]. In cluster-case this always was
        // forbidden.
        // TODO: now we need to allow this, as we use cluster features for
        // single servers in case of async failover
        _shardKeys.emplace_back(StaticStrings::KeyString);
      }
    }
  }

  if (_shardKeys.empty() || _shardKeys.size() > 8) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid number of shard keys for collection")
    );
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
  
  // set the sharding strategy
  if (!ServerState::instance()->isRunningInCluster()) {
    // shortcut, so we do not need to set up the whole application server for testing
    _shardingStrategy = std::make_unique<ShardingStrategyNone>();
  } else {
    _shardingStrategy = application_features::ApplicationServer::getFeature<ShardingFeature>("Sharding")->fromVelocyPack(info, this);
  }
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::ShardingInfo(ShardingInfo const& other, LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(other.numberOfShards()),
      _replicationFactor(other.replicationFactor()),
      _distributeShardsLike(other.distributeShardsLike()),
      _avoidServers(other.avoidServers()),
      _shardKeys(other.shardKeys()),
      _shardIds(new ShardMap()),
      _shardingStrategy() {

  TRI_ASSERT(_collection != nullptr);

  // set the sharding strategy
  _shardingStrategy = application_features::ApplicationServer::getFeature<ShardingFeature>("Sharding")->create(other._shardingStrategy->name(), this);
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::~ShardingInfo() {}

bool ShardingInfo::usesSameShardingStrategy(ShardingInfo const* other) const {
  return _shardingStrategy->isCompatible(other->_shardingStrategy.get());
}

std::string ShardingInfo::shardingStrategyName() const {
  return _shardingStrategy->name();
}

LogicalCollection* ShardingInfo::collection() const {
  TRI_ASSERT(_collection != nullptr);
  return _collection;
}

void ShardingInfo::toVelocyPack(VPackBuilder& result, bool translateCids) {
  result.add(StaticStrings::NumberOfShards, VPackValue(_numberOfShards));

  result.add(VPackValue("shards"));
  result.openObject();
  auto tmpShards = _shardIds;

  for (auto const& shards : *tmpShards) {
    result.add(VPackValue(shards.first));
    result.openArray();

    for (auto const& servers : shards.second) {
      result.add(VPackValue(servers));
    }

    result.close();  // server array
  }

  result.close();  // shards

  if (isSatellite()) {
    result.add(StaticStrings::ReplicationFactor, VPackValue("satellite"));
  } else {
    result.add(StaticStrings::ReplicationFactor, VPackValue(_replicationFactor));
  }

  if (!_distributeShardsLike.empty() &&
      ServerState::instance()->isCoordinator()) {

    if (translateCids) {
      CollectionNameResolver resolver(_collection->vocbase());

      result.add(StaticStrings::DistributeShardsLike,
                 VPackValue(resolver.getCollectionNameCluster(
                     static_cast<TRI_voc_cid_t>(basics::StringUtils::uint64(
                         distributeShardsLike())))));
    } else {
      result.add(StaticStrings::DistributeShardsLike, VPackValue(distributeShardsLike()));
    }
  }

  result.add(VPackValue(StaticStrings::ShardKeys));
  result.openArray();

  for (auto const& key : _shardKeys) {
    result.add(VPackValue(key));
  }

  result.close();  // shardKeys

  if (!_avoidServers.empty()) {
    result.add(VPackValue("avoidServers"));
    result.openArray();
    for (auto const& server : _avoidServers) {
      result.add(VPackValue(server));
    }
    result.close();
  }

  _shardingStrategy->toVelocyPack(result);
}

std::string ShardingInfo::distributeShardsLike() const {
  return _distributeShardsLike;
}

void ShardingInfo::distributeShardsLike(std::string const& cid, ShardingInfo const* other) {
  if (_shardKeys.size() != other->shardKeys().size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "cannot distribute shards like "
                                   "a collection with a different number of shard key attributes");
  }

  if (!usesSameShardingStrategy(other)) {
    // other collection has a different sharding strategy
    // adjust our sharding so it uses the same strategy as the other collection
    auto shr = application_features::ApplicationServer::getFeature<ShardingFeature>("Sharding");
    _shardingStrategy = shr->create(other->shardingStrategyName(), this);
  }

  _distributeShardsLike = cid;

  if (_collection->isSmart() && _collection->type() == TRI_COL_TYPE_EDGE) {
    return;
  }

  _replicationFactor = other->replicationFactor();
  _numberOfShards = other->numberOfShards();
}

std::vector<std::string> const& ShardingInfo::avoidServers() const {
  return _avoidServers;
}

void ShardingInfo::avoidServers(std::vector<std::string> const& avoidServers) {
  _avoidServers = avoidServers;
}

size_t ShardingInfo::replicationFactor() const {
  return _replicationFactor;
}

void ShardingInfo::replicationFactor(size_t replicationFactor) {
  _replicationFactor = replicationFactor;
}

bool ShardingInfo::isSatellite() const {
  return _replicationFactor == 0;
}

size_t ShardingInfo::numberOfShards() const {
  return _numberOfShards;
}

void ShardingInfo::numberOfShards(size_t numberOfShards) {
  // the only allowed value is "0", because the only allowed
  // caller of this method is VirtualSmartEdgeCollection, which
  // sets the number of shards to 0
  TRI_ASSERT(numberOfShards == 0);
  _numberOfShards = numberOfShards;
}

bool ShardingInfo::usesDefaultShardKeys() const {
  return _shardingStrategy->usesDefaultShardKeys();
}

std::vector<std::string> const& ShardingInfo::shardKeys() const {
  TRI_ASSERT(!_shardKeys.empty());
  return _shardKeys;
}

std::shared_ptr<ShardMap> ShardingInfo::shardIds() const {
  return _shardIds;
}

// return a filtered list of the collection's shards
std::shared_ptr<ShardMap> ShardingInfo::shardIds(std::unordered_set<std::string> const& includedShards) const {
  if (includedShards.empty()) {
    return _shardIds;
  }

  std::shared_ptr<ShardMap> copy = _shardIds;
  auto result = std::make_shared<ShardMap>();

  for (auto const& it : *copy) {
    if (includedShards.find(it.first) == includedShards.end()) {
      // a shard we are not interested in
      continue;
    }
    result->emplace(it.first, it.second);
  }
  return result;
}

void ShardingInfo::setShardMap(std::shared_ptr<ShardMap> const& map) {
  _shardIds = map;
}

int ShardingInfo::getResponsibleShard(arangodb::velocypack::Slice slice,
                                      bool docComplete, ShardID& shardID,
                                      bool& usesDefaultShardKeys,
                                      std::string const& key) {
  return _shardingStrategy->getResponsibleShard(slice, docComplete, shardID, usesDefaultShardKeys, key);
}
