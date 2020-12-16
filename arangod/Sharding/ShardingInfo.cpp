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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Sharding/ShardingFeature.h"
#include "Sharding/ShardingStrategyDefault.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

ShardingInfo::ShardingInfo(arangodb::velocypack::Slice info, LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(basics::VelocyPackHelper::getNumericValue<size_t>(info, StaticStrings::NumberOfShards,
                                                                         1)),
      _replicationFactor(1),
      _writeConcern(1),
      _distributeShardsLike(basics::VelocyPackHelper::getStringValue(info, StaticStrings::DistributeShardsLike,
                                                                     "")),
      _shardIds(new ShardMap()) {
  bool const isSmart =
      basics::VelocyPackHelper::getBooleanValue(info, StaticStrings::IsSmart, false);

  if (isSmart && _collection->type() == TRI_COL_TYPE_EDGE) {
    // smart edge collection
    _numberOfShards = 0;
  }

  VPackSlice shardKeysSlice = info.get(StaticStrings::ShardKeys);
  if (ServerState::instance()->isCoordinator()) {
    if (_numberOfShards == 0 && !isSmart) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid number of shards");
    }
    // intentionally no call to validateNumberOfShards here,
    // because this constructor is called from the constructor of
    // LogicalCollection, and we want LogicalCollection to be created
    // with any configured number of shards in case the maximum allowed
    // number of shards is set or decreased in a cluster with already
    // existing collections that would violate the setting.
    // so we validate the number of shards against the maximum only
    // when a collection is created by a user, and on a restore
  }

  VPackSlice distributeShardsLike = info.get(StaticStrings::DistributeShardsLike);
  if (!distributeShardsLike.isNone() && 
      !distributeShardsLike.isString() &&
      !distributeShardsLike.isNull()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "invalid non-string value for 'distributeShardsLike'");
  }

  VPackSlice v = info.get(StaticStrings::NumberOfShards);
  if (!v.isNone() && !v.isNumber() && !v.isNull()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid number of shards");
  }

  auto avoidServersSlice = info.get("avoidServers");
  if (avoidServersSlice.isArray()) {
    for (VPackSlice i : VPackArrayIterator(avoidServersSlice)) {
      if (i.isString()) {
        _avoidServers.push_back(i.copyString());
      } else {
        LOG_TOPIC("e5bc6", ERR, arangodb::Logger::FIXME)
            << "avoidServers must be a vector of strings, we got "
            << avoidServersSlice.toJson() << ". discarding!";
        _avoidServers.clear();
        break;
      }
    }
  }

  bool isASatellite = false; 
  auto replicationFactorSlice = info.get(StaticStrings::ReplicationFactor);
  if (!replicationFactorSlice.isNone()) {
    bool isError = true;
    if (replicationFactorSlice.isNumber()) {
      _replicationFactor = replicationFactorSlice.getNumber<size_t>();
      // mop: only allow SatelliteCollections to be created explicitly
      if (_replicationFactor > 0) {
        isError = false;
#ifdef USE_ENTERPRISE
      } else if (_replicationFactor == 0) {
        isError = false;
#endif
      }
    }
#ifdef USE_ENTERPRISE
    else if (replicationFactorSlice.isString() &&
             replicationFactorSlice.copyString() == StaticStrings::Satellite) {
      _replicationFactor = 0;
      _writeConcern = 0;
      _numberOfShards = 1;
      _avoidServers.clear();
      isError = false;
      isASatellite = true;
    }

    if (isSmart && isASatellite) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "'isSmart' and replicationFactor 'satellite' cannot be combined");
    }
#endif
    if (isError) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid replicationFactor");
    }
  }

  if (!isASatellite) {
    auto writeConcernSlice = info.get(StaticStrings::WriteConcern);
    if (writeConcernSlice.isNone()) { // minReplicationFactor is deprecated in 3.6
      writeConcernSlice = info.get(StaticStrings::MinReplicationFactor);
    }
    if (!writeConcernSlice.isNone()) {
      if (writeConcernSlice.isNumber()) {
        _writeConcern = writeConcernSlice.getNumber<size_t>();
        if (!isSatellite() && _writeConcern > _replicationFactor) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "writeConcern cannot be larger than replicationFactor (" +
                  basics::StringUtils::itoa(_writeConcern) + " > " +
                  basics::StringUtils::itoa(_replicationFactor) + ")");
        }
        if (!isSatellite() && _writeConcern == 0) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                         "writeConcern cannot be 0");
        }
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "writeConcern needs to be an integer number");
      }
    }
  }
  
  // replicationFactor == 0 -> SatelliteCollection
  if (shardKeysSlice.isNone() || _replicationFactor == 0) {
    // Use default.
    _shardKeys.emplace_back(StaticStrings::KeyString);
  } else {
    if (shardKeysSlice.isArray()) {
      for (auto const& sk : VPackArrayIterator(shardKeysSlice)) {
        if (sk.isString()) {
          velocypack::StringRef key = sk.stringRef();
          // remove : char at the beginning or end (for enterprise)
          velocypack::StringRef stripped;
          if (!key.empty()) {
            if (key.front() == ':') {
              stripped = key.substr(1);
            } else if (key.back() == ':') {
              stripped = key.substr(0, key.size() - 1);
            } else {
              stripped = key;
            }
          }
          // system attributes are not allowed (except _key, _from and _to)
          if (stripped == StaticStrings::IdString || 
              stripped == StaticStrings::RevString) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, 
                                           "_id or _rev cannot be used as shard keys");
          }

          if (!stripped.empty()) {
            _shardKeys.emplace_back(key.toString());
          }
        }
      }
      if (_shardKeys.empty()) { 
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
        "invalid number of shard keys for collection");
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
        _shardIds->try_emplace(shard, servers);
      }
    }
  }

  // set the sharding strategy
  if (!ServerState::instance()->isRunningInCluster()) {
    // shortcut, so we do not need to set up the whole application server for
    // testing
    _shardingStrategy = std::make_unique<ShardingStrategyNone>();
  } else {
    auto& server = _collection->vocbase().server();
    _shardingStrategy = server.getFeature<ShardingFeature>().fromVelocyPack(info, this);
  }
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::ShardingInfo(ShardingInfo const& other, LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(other.numberOfShards()),
      _replicationFactor(other.replicationFactor()),
      _writeConcern(other.writeConcern()),
      _distributeShardsLike(other.distributeShardsLike()),
      _avoidServers(other.avoidServers()),
      _shardKeys(other.shardKeys()),
      _shardIds(new ShardMap()),
      _shardingStrategy() {
  TRI_ASSERT(_collection != nullptr);

  // set the sharding strategy
  auto& server = _collection->vocbase().server();
  _shardingStrategy =
      server.getFeature<ShardingFeature>().create(other._shardingStrategy->name(), this);
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::~ShardingInfo() = default;

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

void ShardingInfo::toVelocyPack(VPackBuilder& result, bool translateCids) const {
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
    result.add(StaticStrings::ReplicationFactor, VPackValue(StaticStrings::Satellite));
  } else {
    result.add(StaticStrings::ReplicationFactor, VPackValue(_replicationFactor));
  }

  // minReplicationFactor deprecated in 3.6
  result.add(StaticStrings::WriteConcern, VPackValue(_writeConcern));
  result.add(StaticStrings::MinReplicationFactor, VPackValue(_writeConcern));

  if (!_distributeShardsLike.empty() && ServerState::instance()->isCoordinator()) {
    if (translateCids) {
      CollectionNameResolver resolver(_collection->vocbase());

      result.add(StaticStrings::DistributeShardsLike,
                 VPackValue(resolver.getCollectionNameCluster(static_cast<TRI_voc_cid_t>(
                     basics::StringUtils::uint64(distributeShardsLike())))));
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "cannot distribute shards like "
        "a collection with a different number of shard key attributes");
  }

  if (!usesSameShardingStrategy(other)) {
    // other collection has a different sharding strategy
    // adjust our sharding so it uses the same strategy as the other collection
    auto& server = _collection->vocbase().server();
    auto& shr = server.getFeature<ShardingFeature>();
    _shardingStrategy = shr.create(other->shardingStrategyName(), this);
  }

  _distributeShardsLike = cid;

  if (_collection->isSmart() && _collection->type() == TRI_COL_TYPE_EDGE) {
    return;
  }

  _replicationFactor = other->replicationFactor();
  _writeConcern = other->writeConcern();
  _numberOfShards = other->numberOfShards();
}

std::vector<std::string> const& ShardingInfo::avoidServers() const {
  return _avoidServers;
}

void ShardingInfo::avoidServers(std::vector<std::string> const& avoidServers) {
  _avoidServers = avoidServers;
}

size_t ShardingInfo::replicationFactor() const {
  TRI_ASSERT(isSatellite() || _writeConcern <= _replicationFactor);
  return _replicationFactor;
}

void ShardingInfo::replicationFactor(size_t replicationFactor) {
  if (!isSatellite() && replicationFactor < _writeConcern) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "replicationFactor cannot be smaller than writeConcern (" +
            basics::StringUtils::itoa(_replicationFactor) + " < " +
            basics::StringUtils::itoa(_writeConcern) + ")");
  }
  _replicationFactor = replicationFactor;
}

size_t ShardingInfo::writeConcern() const {
  TRI_ASSERT(isSatellite() || _writeConcern <= _replicationFactor);
  return _writeConcern;
}

void ShardingInfo::writeConcern(size_t writeConcern) {
  if (!isSatellite() && writeConcern > _replicationFactor) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "writeConcern cannot be larger than replicationFactor (" +
            basics::StringUtils::itoa(_writeConcern) + " > " +
            basics::StringUtils::itoa(_replicationFactor) + ")");
  }
  _writeConcern = writeConcern;
}

void ShardingInfo::setWriteConcernAndReplicationFactor(size_t writeConcern, size_t replicationFactor) {
  if (writeConcern > replicationFactor) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "writeConcern cannot be larger than replicationFactor (" +
            basics::StringUtils::itoa(writeConcern) + " > " +
            basics::StringUtils::itoa(replicationFactor) + ")");
  }
  _writeConcern = writeConcern;
  _replicationFactor = replicationFactor;
}

bool ShardingInfo::isSatellite() const { return _replicationFactor == 0; }

size_t ShardingInfo::numberOfShards() const { return _numberOfShards; }

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

std::shared_ptr<ShardMap> ShardingInfo::shardIds() const { return _shardIds; }

std::shared_ptr<std::vector<ShardID>> ShardingInfo::shardListAsShardID() const {
  auto vector = std::make_shared<std::vector<ShardID>>();
  for (auto const& mapElement : *_shardIds) {
    vector->emplace_back(mapElement.first);
  }
  sortShardNamesNumerically(*vector);
  return vector;
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
    result->try_emplace(it.first, it.second);
  }
  return result;
}

void ShardingInfo::setShardMap(std::shared_ptr<ShardMap> const& map) {
  _shardIds = map;
}

int ShardingInfo::getResponsibleShard(arangodb::velocypack::Slice slice, bool docComplete,
                                      ShardID& shardID, bool& usesDefaultShardKeys,
                                      VPackStringRef const& key) {
  return _shardingStrategy->getResponsibleShard(slice, docComplete, shardID,
                                                usesDefaultShardKeys, key);
}

Result ShardingInfo::validateShardsAndReplicationFactor(arangodb::velocypack::Slice slice,
                                                        application_features::ApplicationServer const& server,
                                                        bool enforceReplicationFactor) {
  if (slice.isObject()) {
    auto& cl = server.getFeature<ClusterFeature>();

    auto numberOfShardsSlice = slice.get(StaticStrings::NumberOfShards);
    if (numberOfShardsSlice.isNumber()) {
      uint32_t const maxNumberOfShards = cl.maxNumberOfShards();
      uint32_t numberOfShards = numberOfShardsSlice.getNumber<uint32_t>();
      if (maxNumberOfShards > 0 &&
          numberOfShards > maxNumberOfShards) {
        return Result(TRI_ERROR_CLUSTER_TOO_MANY_SHARDS, 
                      std::string("too many shards. maximum number of shards is ") + std::to_string(maxNumberOfShards));
      }

      TRI_ASSERT((cl.forceOneShard() && numberOfShards <= 1) || !cl.forceOneShard()); 
    }
          
    auto writeConcernSlice = slice.get(StaticStrings::WriteConcern);
    auto minReplicationFactorSlice = slice.get(StaticStrings::MinReplicationFactor);
          
    if (writeConcernSlice.isNumber() && minReplicationFactorSlice.isNumber()) {
      // both attributes set. now check if they have different values
      if (basics::VelocyPackHelper::compare(writeConcernSlice, minReplicationFactorSlice, false) != 0) {
        return Result(TRI_ERROR_BAD_PARAMETER, "got ambiguous values for writeConcern and minReplicationFactor");
      }
    }

    if (enforceReplicationFactor) {
      auto enforceSlice = slice.get("enforceReplicationFactor");
      if (!enforceSlice.isBool() || enforceSlice.getBool()) {
        auto replicationFactorSlice = slice.get(StaticStrings::ReplicationFactor);
        if (replicationFactorSlice.isNumber()) {
          int64_t replicationFactorProbe = replicationFactorSlice.getNumber<int64_t>();
          if (replicationFactorProbe == 0) {
            // TODO: Which configuration for satellites are valid regarding minRepl and writeConcern
            // valid for creating a SatelliteCollection
            return Result();
          }
          if (replicationFactorProbe < 0) {
            return Result(TRI_ERROR_BAD_PARAMETER, "invalid value for replicationFactor");
          }

          uint32_t const minReplicationFactor = cl.minReplicationFactor();
          uint32_t const maxReplicationFactor = cl.maxReplicationFactor();
          uint32_t replicationFactor = replicationFactorSlice.getNumber<uint32_t>();

          // make sure the replicationFactor value is between the configured min and max values
          if (replicationFactor > maxReplicationFactor &&
              maxReplicationFactor > 0) {
            return Result(TRI_ERROR_BAD_PARAMETER,
                          std::string("replicationFactor must not be higher than maximum allowed replicationFactor (") + std::to_string(maxReplicationFactor) + ")");
          } else if (replicationFactor < minReplicationFactor &&
              minReplicationFactor > 0) {
            return Result(TRI_ERROR_BAD_PARAMETER,
                          std::string("replicationFactor must not be lower than minimum allowed replicationFactor (") + std::to_string(minReplicationFactor) + ")");
          }
        
          // make sure we have enough servers available for the replication factor
          if (ServerState::instance()->isCoordinator() &&
              replicationFactor > cl.clusterInfo().getCurrentDBServers().size()) { 
            return Result(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
          }
        }

        if (!replicationFactorSlice.isString()) {
          // beware: "satellite" replicationFactor
          if (writeConcernSlice.isNone()) {
            writeConcernSlice = minReplicationFactorSlice;
          }

          if (writeConcernSlice.isNumber()) {
            int64_t writeConcern = writeConcernSlice.getNumber<int64_t>();
            if (writeConcern <= 0) {
              return Result(TRI_ERROR_BAD_PARAMETER, "invalid value for writeConcern");
            }
            if (ServerState::instance()->isCoordinator() &&
                static_cast<size_t>(writeConcern) > cl.clusterInfo().getCurrentDBServers().size()) { 
              return Result(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS);
            }
          }
        }
      }
    }
  }

  return Result();
}

void ShardingInfo::sortShardNamesNumerically(std::vector<ShardID>& list) {
  // We need to sort numerically, so s99 is before s100:
  std::sort(list.begin(), list.end(), [](ShardID const& lhs, ShardID const& rhs) {
    TRI_ASSERT(lhs.size() > 1 && lhs[0] == 's');
    uint64_t l = basics::StringUtils::uint64(lhs.c_str() + 1, lhs.size() - 1);
    TRI_ASSERT(rhs.size() > 1 && rhs[0] == 's');
    uint64_t r = basics::StringUtils::uint64(rhs.c_str() + 1, rhs.size() - 1);
    return l < r;
  });
}
