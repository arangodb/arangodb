////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "Sharding/ShardingFeature.h"
#include "Sharding/ShardingStrategyDefault.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;

namespace {

Result writeConcernError(std::size_t replicationFactor,
                         std::size_t writeConcern) {
  std::string msg;
  if (replicationFactor < writeConcern) {
    msg =
        absl::StrCat("replicationFactor cannot be smaller than writeConcern (",
                     replicationFactor, " < ", writeConcern, ")");
  } else if (replicationFactor > writeConcern) {
    msg = absl::StrCat("replicationFactor cannot be higher than writeConcern (",
                       replicationFactor, " > ", writeConcern, ")");
  } else {
    TRI_ASSERT(false);
  }
  return {TRI_ERROR_BAD_PARAMETER, std::move(msg)};
}

}  // namespace

ShardingInfo::ShardingInfo(arangodb::velocypack::Slice info,
                           LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(basics::VelocyPackHelper::getNumericValue<size_t>(
          info, StaticStrings::NumberOfShards, 1)),
      _replicationFactor(1),
      _writeConcern(1),
      _distributeShardsLike(basics::VelocyPackHelper::getStringValue(
          info, StaticStrings::DistributeShardsLike, "")),
      _shardIds(std::make_shared<ShardMap>()) {
  bool const isSmart = basics::VelocyPackHelper::getBooleanValue(
      info, StaticStrings::IsSmart, false);

  if (isSmart && _collection->type() == TRI_COL_TYPE_EDGE &&
      ServerState::instance()->isRunningInCluster()) {
    // A smart edge collection in a single server environment does get proper
    // numberOfShards value. A smart edge collection in a cluster needs to set
    // numberOfShards to zero by definition.
    _numberOfShards = 0;
  }

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

  VPackSlice distributeShardsLike =
      info.get(StaticStrings::DistributeShardsLike);
  if (!distributeShardsLike.isNone() && !distributeShardsLike.isString() &&
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

  size_t replicationFactor = _replicationFactor;
  Result res = extractReplicationFactor(info, isSmart, replicationFactor);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  _replicationFactor = replicationFactor;
  if (_replicationFactor == 0) {
    // satellite collection
    makeSatellite();
  } else {
    auto writeConcernSlice = info.get(StaticStrings::WriteConcern);
    if (writeConcernSlice
            .isNone()) {  // minReplicationFactor is deprecated in 3.6
      writeConcernSlice = info.get(StaticStrings::MinReplicationFactor);
    }
    if (!writeConcernSlice.isNone()) {
      if (writeConcernSlice.isNumber()) {
        _writeConcern = writeConcernSlice.getNumber<size_t>();
        if (!isSatellite() && _writeConcern > _replicationFactor &&
            ServerState::instance()->isCoordinator()) {
          res = writeConcernError(
              _replicationFactor.load(std::memory_order_relaxed),
              _writeConcern.load(std::memory_order_relaxed));
          if (res.fail()) {
            THROW_ARANGO_EXCEPTION(res);
          }
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

  res = extractShardKeys(info, _replicationFactor, _shardKeys);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto shardsSlice = info.get("shards");
  if (shardsSlice.isObject()) {
    for (auto const& shardSlice : VPackObjectIterator(shardsSlice)) {
      if (shardSlice.key.isString() && shardSlice.value.isArray()) {
        // NOTE: Can throw if shard is not a valid shard name
        ShardID shard{shardSlice.key.stringView()};

        std::vector<ServerID> servers;
        for (auto const& serverSlice : VPackArrayIterator(shardSlice.value)) {
          servers.push_back(serverSlice.copyString());
        }
        TRI_ASSERT(_shardIds != nullptr);
        _shardIds->try_emplace(shard, servers);
      }
    }
  }

  // set the sharding strategy

  auto& server = _collection->vocbase().server();
#ifdef ARANGODB_USE_GOOGLE_TESTS
  auto const& engineSelection =
      server.getFeature<arangodb::EngineSelectorFeature>();
  if (!ServerState::instance()->isRunningInCluster() &&
      engineSelection.engineName() == "Mock") {
    // shortcut, so we do not need to set up the whole application
    // server for testing
    _shardingStrategy = std::make_unique<ShardingStrategyNone>();
    return;
  }
#endif
  _shardingStrategy =
      server.getFeature<ShardingFeature>().fromVelocyPack(info, this);
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::ShardingInfo(ShardingInfo const& other,
                           LogicalCollection* collection)
    : _collection(collection),
      _numberOfShards(other.numberOfShards()),
      _replicationFactor(other.replicationFactor()),
      _writeConcern(other.writeConcern()),
      _distributeShardsLike(other.distributeShardsLike()),
      _shardKeys(other.shardKeys()),
      _shardIds(std::make_shared<ShardMap>()),
      _shardingStrategy() {
  TRI_ASSERT(_collection != nullptr);

  // set the sharding strategy
  auto& server = _collection->vocbase().server();
  _shardingStrategy = server.getFeature<ShardingFeature>().create(
      other._shardingStrategy->name(), this);
  TRI_ASSERT(_shardingStrategy != nullptr);
}

ShardingInfo::~ShardingInfo() = default;

Result ShardingInfo::extractReplicationFactor(velocypack::Slice info,
                                              bool isSmart,
                                              size_t& replicationFactor) {
  bool isASatellite = false;
  auto replicationFactorSlice = info.get(StaticStrings::ReplicationFactor);
  if (!replicationFactorSlice.isNone()) {
    bool isError = true;
    if (replicationFactorSlice.isNumber()) {
      replicationFactor = replicationFactorSlice.getNumber<size_t>();
      // mop: only allow SatelliteCollections to be created explicitly
      if (replicationFactor > 0) {
        isError = false;
#ifdef USE_ENTERPRISE
      } else if (replicationFactor == 0) {
        isError = false;
        isASatellite = true;
#endif
      }
    }
#ifdef USE_ENTERPRISE
    else if (replicationFactorSlice.isString() &&
             replicationFactorSlice.stringView() == StaticStrings::Satellite) {
      isError = false;
      isASatellite = true;
      replicationFactor = 0;
    }

    if (isSmart && isASatellite) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmart' and replicationFactor 'satellite' cannot be combined"};
    }
#endif
    if (isError) {
      return {TRI_ERROR_BAD_PARAMETER, "invalid replicationFactor"};
    }
  }

  TRI_ASSERT(!isASatellite || replicationFactor == 0);

  return {};
}

Result ShardingInfo::extractShardKeys(velocypack::Slice info,
                                      size_t replicationFactor,
                                      std::vector<std::string>& shardKeys) {
  TRI_ASSERT(shardKeys.empty());

  // replicationFactor == 0 -> SatelliteCollection
  VPackSlice shardKeysSlice = info.get(StaticStrings::ShardKeys);
  if (shardKeysSlice.isNone() || replicationFactor == 0) {
    // Use default.
    shardKeys.emplace_back(StaticStrings::KeyString);
  } else {
    if (shardKeysSlice.isArray()) {
      for (VPackSlice sk : VPackArrayIterator(shardKeysSlice)) {
        if (sk.isString()) {
          std::string_view key = sk.stringView();
          // remove : char at the beginning or end (for enterprise)
          std::string_view stripped;
          if (!key.empty()) {
            if (key.starts_with(':')) {
              stripped = key.substr(1);
            } else if (key.ends_with(':')) {
              stripped = key.substr(0, key.size() - 1);
            } else {
              stripped = key;
            }
          }
          // system attributes are not allowed (except _key, _from and _to)
          if (stripped == StaticStrings::IdString ||
              stripped == StaticStrings::RevString) {
            return {TRI_ERROR_BAD_PARAMETER,
                    "_id or _rev cannot be used as shard keys"};
          }

          if (!stripped.empty()) {
            shardKeys.emplace_back(std::string(key));
          }
        }
      }
      if (shardKeys.empty()) {
        // Compatibility. Old configs might store empty shard-keys locally.
        // This is translated to ["_key"]. In cluster-case this always was
        // forbidden.
        // TODO: now we need to allow this, as we use cluster features for
        // single servers in case of async failover
        shardKeys.emplace_back(StaticStrings::KeyString);
      }
    }
  }

  if (shardKeys.empty() || shardKeys.size() > 8) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid number of shard keys for collection"};
  }

  TRI_ASSERT(!shardKeys.empty());
  return {};
}

std::string ShardingInfo::shardingStrategyName() const {
  return _shardingStrategy->name();
}

LogicalCollection* ShardingInfo::collection() const noexcept {
  TRI_ASSERT(_collection != nullptr);
  return _collection;
}

void ShardingInfo::toVelocyPack(VPackBuilder& result,
                                bool ignoreCollectionGroupAttributes,
                                bool translateCids,
                                bool includeShardsEntry) const {
  result.add(StaticStrings::NumberOfShards, VPackValue(_numberOfShards));

  if (includeShardsEntry) {
    result.add(VPackValue("shards"));
    result.openObject();
    TRI_ASSERT(_shardIds != nullptr);
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
  }

  if (!ignoreCollectionGroupAttributes) {
    // For replication Two this class is not responsible for the following
    // attributes.
    if (isSatellite()) {
      result.add(StaticStrings::ReplicationFactor,
                 VPackValue(StaticStrings::Satellite));
    } else {
      result.add(StaticStrings::ReplicationFactor,
                 VPackValue(_replicationFactor));
    }
    // minReplicationFactor deprecated in 3.6
    result.add(StaticStrings::WriteConcern, VPackValue(_writeConcern));
    result.add(StaticStrings::MinReplicationFactor, VPackValue(_writeConcern));
  }

  if (!_distributeShardsLike.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      // We either want to expose _distributeShardsLike if we're either on a
      // Coordinator
      if (translateCids) {
        CollectionNameResolver resolver(_collection->vocbase());

        result.add(StaticStrings::DistributeShardsLike,
                   VPackValue(resolver.getCollectionNameCluster(DataSourceId{
                       basics::StringUtils::uint64(distributeShardsLike())})));
      } else {
        result.add(StaticStrings::DistributeShardsLike,
                   VPackValue(distributeShardsLike()));
      }
    } else if (ServerState::instance()->isSingleServer()) {
      // Or we have found a Smart or Satellite collection on a single server
      // instance.
      result.add(StaticStrings::DistributeShardsLike,
                 VPackValue(distributeShardsLike()));
    }
  }

  result.add(VPackValue(StaticStrings::ShardKeys));
  result.openArray();

  for (auto const& key : _shardKeys) {
    result.add(VPackValue(key));
  }

  result.close();  // shardKeys

  _shardingStrategy->toVelocyPack(result);
}

std::string const& ShardingInfo::distributeShardsLike() const noexcept {
  return _distributeShardsLike;
}

size_t ShardingInfo::replicationFactor() const noexcept {
  TRI_ASSERT(isSatellite() || !ServerState::instance()->isCoordinator() ||
             _writeConcern <= _replicationFactor);
  return _replicationFactor;
}

void ShardingInfo::replicationFactor(size_t replicationFactor) {
  if (ServerState::instance()->isCoordinator()) {
    auto wc = _writeConcern.load(std::memory_order_relaxed);
    if (!isSatellite() && replicationFactor < wc) {
      auto res = writeConcernError(replicationFactor, wc);
      TRI_ASSERT(res.fail());
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  _replicationFactor = replicationFactor;
}

size_t ShardingInfo::writeConcern() const noexcept {
  TRI_ASSERT(isSatellite() || !ServerState::instance()->isCoordinator() ||
             _writeConcern <= _replicationFactor);
  return _writeConcern;
}

void ShardingInfo::writeConcern(size_t writeConcern) {
  if (ServerState::instance()->isCoordinator()) {
    auto rf = _replicationFactor.load(std::memory_order_relaxed);
    if (!isSatellite() && writeConcern > rf) {
      auto res = writeConcernError(rf, writeConcern);
      TRI_ASSERT(res.fail());
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  _writeConcern = writeConcern;
}

void ShardingInfo::setWriteConcernAndReplicationFactor(
    size_t writeConcern, size_t replicationFactor) {
  if (ServerState::instance()->isCoordinator() &&
      writeConcern > replicationFactor) {
    auto res = writeConcernError(replicationFactor, writeConcern);
    TRI_ASSERT(res.fail());
    THROW_ARANGO_EXCEPTION(res);
  }
  _writeConcern = writeConcern;
  _replicationFactor = replicationFactor;
}

bool ShardingInfo::isSatellite() const noexcept {
  return _replicationFactor == 0;
}

void ShardingInfo::makeSatellite() {
  _replicationFactor = 0;
  _writeConcern = 0;
  _numberOfShards = 1;
}

size_t ShardingInfo::numberOfShards() const noexcept { return _numberOfShards; }

void ShardingInfo::numberOfShards(size_t numberOfShards) {
  // the only allowed value is "0", because the only allowed
  // caller of this method is VirtualClusterSmartEdgeCollection, which
  // sets the number of shards to 0
  TRI_ASSERT(numberOfShards == 0);
  _numberOfShards = numberOfShards;
}

bool ShardingInfo::usesDefaultShardKeys() const noexcept {
  return _shardingStrategy->usesDefaultShardKeys();
}

std::vector<std::string> const& ShardingInfo::shardKeys() const noexcept {
  TRI_ASSERT(!_shardKeys.empty());
  return _shardKeys;
}

std::shared_ptr<ShardMap> ShardingInfo::shardIds() const {
  TRI_ASSERT(_shardIds != nullptr);
  return _shardIds;
}

std::set<ShardID> ShardingInfo::shardListAsShardID() const {
  std::set<ShardID> result;
  TRI_ASSERT(_shardIds != nullptr);
  for (auto const& mapElement : *_shardIds) {
    result.emplace(mapElement.first);
  }
  return result;
}

// return a filtered list of the collection's shards
std::shared_ptr<ShardMap> ShardingInfo::shardIds(
    std::unordered_set<std::string> const& includedShards) const {
  if (includedShards.empty()) {
    return _shardIds;
  }

  TRI_ASSERT(_shardIds != nullptr);
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
  TRI_ASSERT(map != nullptr);
  _shardIds = map;
  _numberOfShards = map->size();
}

ResultT<ShardID> ShardingInfo::getResponsibleShard(
    arangodb::velocypack::Slice slice, bool docComplete,
    bool& usesDefaultShardKeys, std::string_view key) {
  return _shardingStrategy->getResponsibleShard(slice, docComplete,
                                                usesDefaultShardKeys, key);
}

Result ShardingInfo::validateShardsAndReplicationFactor(
    arangodb::velocypack::Slice slice, ArangodServer const& server,
    bool enforceReplicationFactor) {
  if (slice.isObject()) {
    auto& cl = server.getFeature<ClusterFeature>();

    auto numberOfShardsSlice = slice.get(StaticStrings::NumberOfShards);
    if (numberOfShardsSlice.isNumber()) {
      uint32_t const maxNumberOfShards = cl.maxNumberOfShards();
      uint32_t numberOfShards = numberOfShardsSlice.getNumber<uint32_t>();
      if (maxNumberOfShards > 0 && numberOfShards > maxNumberOfShards) {
        return {TRI_ERROR_CLUSTER_TOO_MANY_SHARDS,
                absl::StrCat("too many shards. maximum number of shards is ",
                             maxNumberOfShards)};
      }

      TRI_ASSERT((cl.forceOneShard() && numberOfShards <= 1) ||
                 !cl.forceOneShard());
    }

    auto writeConcernSlice = slice.get(StaticStrings::WriteConcern);
    auto minReplicationFactorSlice =
        slice.get(StaticStrings::MinReplicationFactor);

    if (writeConcernSlice.isNumber() && minReplicationFactorSlice.isNumber()) {
      // both attributes set. now check if they have different values
      if (basics::VelocyPackHelper::compare(
              writeConcernSlice, minReplicationFactorSlice, false) != 0) {
        return {
            TRI_ERROR_BAD_PARAMETER,
            "got ambiguous values for writeConcern and minReplicationFactor"};
      }
    }

    if (enforceReplicationFactor) {
      auto enforceSlice = slice.get("enforceReplicationFactor");
      if (!enforceSlice.isBool() || enforceSlice.getBool()) {
        auto replicationFactorSlice =
            slice.get(StaticStrings::ReplicationFactor);
        if (replicationFactorSlice.isNumber()) {
          int64_t replicationFactorProbe =
              replicationFactorSlice.getNumber<int64_t>();
          if (replicationFactorProbe == 0) {
            // TODO: Which configuration for satellites are valid regarding
            // minRepl and writeConcern valid for creating a SatelliteCollection
            return {};
          }
          if (replicationFactorProbe < 0) {
            return {TRI_ERROR_BAD_PARAMETER,
                    "invalid value for replicationFactor"};
          }

          uint32_t const minReplicationFactor = cl.minReplicationFactor();
          uint32_t const maxReplicationFactor = cl.maxReplicationFactor();
          uint32_t replicationFactor =
              replicationFactorSlice.getNumber<uint32_t>();

          // make sure the replicationFactor value is between the configured min
          // and max values
          if (replicationFactor > maxReplicationFactor &&
              maxReplicationFactor > 0) {
            return {TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("replicationFactor must not be higher than "
                                 "maximum allowed replicationFactor (",
                                 maxReplicationFactor, ")")};
          } else if (replicationFactor < minReplicationFactor &&
                     minReplicationFactor > 0) {
            return {TRI_ERROR_BAD_PARAMETER,
                    absl::StrCat("replicationFactor must not be lower than "
                                 "minimum allowed replicationFactor (",
                                 minReplicationFactor, ")")};
          }

          // make sure we have enough servers available for the replication
          // factor
          if (ServerState::instance()->isCoordinator() &&
              replicationFactor >
                  cl.clusterInfo().getCurrentDBServers().size()) {
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
              return {TRI_ERROR_BAD_PARAMETER,
                      "invalid value for writeConcern"};
            }
            if (ServerState::instance()->isCoordinator() &&
                static_cast<size_t>(writeConcern) >
                    cl.clusterInfo().getCurrentDBServers().size()) {
              return {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS};
            }

            if (replicationFactorSlice.isNumber() &&
                writeConcern > replicationFactorSlice.getNumber<int64_t>()) {
              auto res = writeConcernError(
                  replicationFactorSlice.getNumber<int64_t>(), writeConcern);
              TRI_ASSERT(res.fail());
              return res;
            }
          }
        }
      }
    }
  }

  return {};
}

template<typename T>
void ShardingInfo::sortShardNamesNumerically(T& list) {
  // We need to sort numerically, so s99 is before s100:
  std::sort(
      list.begin(), list.end(),
      [](typename T::value_type const& lhs, typename T::value_type const& rhs) {
        TRI_ASSERT(lhs.size() > 1 && lhs[0] == 's');
        uint64_t l =
            basics::StringUtils::uint64(lhs.data() + 1, lhs.size() - 1);
        TRI_ASSERT(rhs.size() > 1 && rhs[0] == 's');
        uint64_t r =
            basics::StringUtils::uint64(rhs.data() + 1, rhs.size() - 1);
        return l < r;
      });
}

template void ShardingInfo::sortShardNamesNumerically<std::vector<ServerID>>(
    std::vector<ServerID>& list);

template void ShardingInfo::sortShardNamesNumerically<
    containers::SmallVector<std::string_view, 8>>(
    containers::SmallVector<std::string_view, 8>& list);
