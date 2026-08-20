////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "CollectionValidation.h"

#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Utilities/NameValidator.h"
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/Properties/DatabaseConfiguration.h"

#include <string>
#include <variant>
#include <vector>

using namespace arangodb;

namespace {

Result validateOrSetDefaultShardingStrategy(CollectionDescriptor& d) {
  if (d.constant.isSmart) {
#ifdef USE_ENTERPRISE
    return validateOrSetDefaultShardingStrategyEE(d);
#endif
  }
  auto& strategy = d.clusteringConstant.shardingStrategy;
  if (strategy.has_value()) {
    if (strategy.value() == "hash" || strategy.value() == "community-compat" ||
        strategy.value() == "enterprise-compat") {
      return {TRI_ERROR_NO_ERROR};
    }
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid sharding strategy " + strategy.value()};
  }
  strategy = "hash";
  return {TRI_ERROR_NO_ERROR};
}

void setDefaultShardKeys(CollectionDescriptor& d) {
  TRI_ASSERT(!d.clusteringConstant.shardKeys.has_value());
  if (d.constant.isSmart) {
#ifdef USE_ENTERPRISE
    setDefaultShardKeysEE(d);
    return;
#endif
  }
  d.clusteringConstant.shardKeys =
      std::vector<std::string>{StaticStrings::KeyString};
}

Result validateShardKeys(CollectionDescriptor const& d) {
  // Has to be set by now
  TRI_ASSERT(d.clusteringConstant.shardKeys.has_value());

  auto const& keys = d.clusteringConstant.shardKeys.value();

  if (keys.empty() || keys.size() > 8) {
    return {TRI_ERROR_BAD_PARAMETER,
            "invalid number of shard keys for collection"};
  }
  for (auto const& sk : keys) {
    auto key = std::string_view{sk};
    // remove : char at the beginning or end (for enterprise)
    std::string_view stripped;
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
      return {TRI_ERROR_BAD_PARAMETER,
              "_id or _rev cannot be used as shard keys"};
    }
  }
  return TRI_ERROR_NO_ERROR;
}

Result validateOrSetShardingStrategy(
    CollectionDescriptor& d, CollectionDescriptor const& leadingCollection) {
  auto const& leaderStrategy = leadingCollection.clusteringConstant.shardingStrategy;
  TRI_ASSERT(leaderStrategy.has_value());
  if (d.constant.isSmart) {
#ifdef USE_ENTERPRISE
    return validateOrSetShardingStrategyEE(d, leadingCollection);
#else
    return {TRI_ERROR_ONLY_ENTERPRISE,
            "Smart collections are only available in Enterprise version."};
#endif
  }
  auto& strategy = d.clusteringConstant.shardingStrategy;
  if (strategy.has_value()) {
    if (strategy != leaderStrategy) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot have a different sharding strategy (" + strategy.value() +
                  "), than the leading collection (" + leaderStrategy.value() +
                  ")"};
    }
  } else {
    strategy = leaderStrategy;
  }
  return TRI_ERROR_NO_ERROR;
}

Result validateSmartJoin(CollectionDescriptor& d) {
  if (d.constant.smartJoinAttribute.has_value()) {
#ifdef USE_ENTERPRISE
    return validateSmartJoinEE(d);
#else
    return {TRI_ERROR_ONLY_ENTERPRISE,
            "SmartJoin collections are only available in Enterprise version."};
#endif
  }
  return TRI_ERROR_NO_ERROR;
}

}  // namespace

Result arangodb::applyDefaultsAndValidate(CollectionDescriptor& d,
                                          DatabaseConfiguration const& config) {
  //  Check name is allowed
  if (auto res = CollectionNameValidator::validateName(
          d.constant.isSystem, config.allowExtendedNames, d.mutableProps.name);
      res.fail()) {
    return res;
  }

  auto res =
      d.internal.applyDefaultsAndValidateDatabaseConfiguration(config);
  if (res.fail()) {
    return res;
  }

  // Unfortunately handling of distributeShardsLike requires more information
  // than just ClusterProperties. Hence we have to handle it on this higher
  // level. DistributeShardsLike has the strongest binding, so it comes first.

  auto& dsl = d.clusteringConstant.distributeShardsLike;
  if (config.oneShardDBConfiguration.has_value() && !dsl.has_value() &&
      d.mutableProps.name !=
          config.oneShardDBConfiguration.value().defaultDistributeShardsLike) {
    dsl = config.oneShardDBConfiguration.value().defaultDistributeShardsLike;
  }

  if (!d.clusteringConstant.shardKeys.has_value()) {
    setDefaultShardKeys(d);
  }

  if (dsl.has_value()) {
    auto groupInfo = config.getCollectionGroupSharding(dsl.value());
    if (!groupInfo.ok()) {
      return groupInfo.result();
    }
    auto const& leader = groupInfo.get();
    if (leader.clusteringConstant.distributeShardsLike.has_value() ||
        leader.clusteringConstant.distributeShardsLikeCid.has_value()) {
      // We are creating a chain of distributeShardsLike, this is not allowed.
      TRI_ASSERT(
          leader.clusteringConstant.distributeShardsLikeCid.has_value());
      auto leadersLeader = config.getCollectionGroupSharding(
          leader.clusteringConstant.distributeShardsLikeCid.value());
      // We cannot see a follower to a non-existent leader.
      TRI_ASSERT(leadersLeader.ok());

      return {TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE,
              "Cannot distribute shards like '" + dsl.value() +
                  "' it is already distributed like '" +
                  leadersLeader->mutableProps.name + "'."};
    }

    // We cannot have a cid set yet, this can only be set if we read from
    // agency which is not yet implemented using this path.
    TRI_ASSERT(!d.clusteringConstant.distributeShardsLikeCid.has_value());
    d.clusteringConstant.distributeShardsLikeCid =
        std::to_string(leader.internal.id.id());

    TRI_ASSERT(leader.clusteringConstant.numberOfShards.has_value());
    if (d.clusteringConstant.numberOfShards.has_value()) {
      if (d.clusteringConstant.numberOfShards !=
          leader.clusteringConstant.numberOfShards) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different numberOfShards (" +
                    std::to_string(
                        d.clusteringConstant.numberOfShards.value()) +
                    "), than the leading collection (" +
                    std::to_string(
                        leader.clusteringConstant.numberOfShards.value()) +
                    ")"};
      }
    } else {
      d.clusteringConstant.numberOfShards =
          leader.clusteringConstant.numberOfShards;
    }

    TRI_ASSERT(leader.clusteringMutable.writeConcern.has_value());
    if (d.clusteringMutable.writeConcern.has_value()) {
      if (config.replicationVersion == replication::Version::TWO) {
        // Replication 2 requires the writeConcern to be equal within a group.
        if (d.clusteringMutable.writeConcern !=
            leader.clusteringMutable.writeConcern) {
          return {TRI_ERROR_BAD_PARAMETER,
                  "Cannot have a different writeConcern (" +
                      std::to_string(
                          d.clusteringMutable.writeConcern.value()) +
                      "), than the leading collection (" +
                      std::to_string(
                          leader.clusteringMutable.writeConcern.value()) +
                      ")"};
        }
      }
    } else {
      d.clusteringMutable.writeConcern = leader.clusteringMutable.writeConcern;
    }

    TRI_ASSERT(leader.clusteringMutable.replicationFactor.has_value());
    if (d.clusteringMutable.replicationFactor.has_value()) {
      if (d.clusteringMutable.replicationFactor !=
          leader.clusteringMutable.replicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different replicationFactor (" +
                    std::to_string(
                        d.clusteringMutable.replicationFactor.value()) +
                    "), than the leading collection (" +
                    std::to_string(
                        leader.clusteringMutable.replicationFactor.value()) +
                    ")"};
      }
    } else {
      d.clusteringMutable.replicationFactor =
          leader.clusteringMutable.replicationFactor;
    }

    res = validateOrSetShardingStrategy(d, leader);
    if (!res.ok()) {
      return res;
    }

    TRI_ASSERT(d.clusteringConstant.shardKeys.has_value());
    // Every existing collection has shardKeys.
    TRI_ASSERT(leader.clusteringConstant.shardKeys.has_value());
    if (d.clusteringConstant.shardKeys.value().size() !=
        leader.clusteringConstant.shardKeys.value().size()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot have a different number of shardKeys (" +
                  std::to_string(
                      d.clusteringConstant.shardKeys.value().size()) +
                  "), than the leading collection (" +
                  std::to_string(
                      leader.clusteringConstant.shardKeys.value().size()) +
                  ")."};
    }
  } else {
    if (auto strategyResult = validateOrSetDefaultShardingStrategy(d);
        strategyResult.fail()) {
      return strategyResult;
    }
  }

  res = validateShardKeys(d);
  if (res.fail()) {
    return res;
  }

  res = validateSmartJoin(d);
  if (res.fail()) {
    return res;
  }

  // was ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration
  if (!dsl.has_value()) {
    // DistributeShardsLike has been handled above
    d.clusteringMutable.applyDatabaseDefaults(config);
    d.clusteringConstant.applyDatabaseDefaults(config);
  }
  TRI_ASSERT(d.clusteringMutable.replicationFactor.has_value());
  TRI_ASSERT(d.clusteringMutable.writeConcern.has_value());
  TRI_ASSERT(d.clusteringConstant.numberOfShards.has_value());

  res = d.clusteringMutable.validateDatabaseConfiguration(config);
  if (!res.ok()) {
    return res;
  }
  res = d.clusteringConstant.validateDatabaseConfiguration(config);
  if (!res.ok()) {
    return res;
  }
  if (d.clusteringMutable.isSatellite() &&
      d.clusteringConstant.numberOfShards.value() != 1) {
    return {TRI_ERROR_BAD_PARAMETER,
            "A satellite collection can only have a single shard"};
  }

  if (d.clusteringMutable.isSatellite()) {
    // We are a satellite, we cannot be smart at the same time
    if (d.constant.isSmart) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmart' and replicationFactor 'satellite' cannot be combined"};
    }
    if (d.internal.isSmartChild) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmartChild' and replicationFactor 'satellite' cannot be "
              "combined"};
    }
    // Has to be set by now.
    TRI_ASSERT(d.clusteringConstant.shardKeys.has_value());
    auto const& keys = d.clusteringConstant.shardKeys.value();
    if (keys.size() != 1 || keys.at(0) != StaticStrings::KeyString) {
      return {TRI_ERROR_BAD_PARAMETER, "'satellite' cannot use shardKeys"};
    }
  }

  if (std::holds_alternative<AutoIncrementGeneratorProperties>(
          d.constant.keyOptions)) {
    // AutoIncrement is not allowed with any other than 1 shard.
    if (d.clusteringConstant.numberOfShards.value() != 1) {
      return {TRI_ERROR_CLUSTER_UNSUPPORTED,
              "the specified key generator is not "
              "supported for collections with more than one shard"};
    }
  }

#ifdef USE_ENTERPRISE
  res = validateOrSetSmartEdgeValidators(d);
  if (res.fail()) {
    return res;
  }
#endif

  return {TRI_ERROR_NO_ERROR};
}
