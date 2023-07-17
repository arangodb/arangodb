////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "UserInputCollectionProperties.h"

#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Utilities/NameValidator.h"
#include "VocBase/Properties/DatabaseConfiguration.h"


#include "Logger/LogMacros.h"

using namespace arangodb;

[[nodiscard]] auto
UserInputCollectionProperties::Invariants::isSmartConfiguration(
    UserInputCollectionProperties const& props) -> inspection::Status {
  if (props.smartGraphAttribute.has_value()) {
    if (props.getType() != TRI_COL_TYPE_DOCUMENT) {
      return {"Only document collections can have a smartGraphAttribute."};
    }
    if (!props.isSmart) {
      return {
          "A smart vertex collection needs to be "
          "marked with \"isSmart: true\"."};
    }
    if (props.shardKeys.has_value() && (props.shardKeys->size() != 1 ||
        props.shardKeys->at(0) != StaticStrings::PrefixOfKeyString)) {
      return {
          R"(A smart vertex collection needs to have "shardKeys": ["_key:"].)"};
    }
  } else if (props.isSmart) {
    if (props.shardKeys.has_value()) {
      // Check if SmartSharding is set correctly, but only if we have one.
      // Otherwise our default sharding will set correct values.
      if (props.shardKeys->size() != 1) {
        return {R"(A smart collection needs to have a single shardKey)"};
      } else {
        TRI_ASSERT(props.shardKeys->size() == 1);
        if (props.getType() == TRI_COL_TYPE_EDGE) {
          if (props.shardKeys->at(0) != StaticStrings::PrefixOfKeyString &&
              props.shardKeys->at(0) != StaticStrings::PostfixOfKeyString &&
              props.shardKeys->at(0) != StaticStrings::KeyString) {
            // For Smart Edges Post and Prefix are allowed (for connecting satellites)
            // Also just _key is allowed, as the shardKey for this collection is not really used.
            // We use the shadows ShardKeys, which are _key based.
            return {R"(A smart edge collection needs to have "shardKeys": ["_key:"], [":_key"] or ["_key"].)"};
          }
        } else {
          if (props.shardKeys->at(0) != StaticStrings::PrefixOfKeyString) {
            return {R"(A smart collection needs to have "shardKeys": ["_key:"].)"};
          }
        }
      }
    }
  }

  return inspection::Status::Success{};
}

[[nodiscard]] Result
UserInputCollectionProperties::applyDefaultsAndValidateDatabaseConfiguration(
    DatabaseConfiguration const& config) {
  //  Check name is allowed
  if (auto res = CollectionNameValidator::validateName(
          isSystem, config.allowExtendedNames, name);
      res.fail()) {
    return res;
  }

  auto res = CollectionInternalProperties::
      applyDefaultsAndValidateDatabaseConfiguration(config);
  if (res.fail()) {
    return res;
  }

  // Unfortunately handling of distributeShardsLike requires more information
  // than just ClusterProperties.
  // Hence we have to handle it on this higher level, or hand in more context
  // into ClusteringProperties. Doing it here was the quicker to implement way,
  // so we went with it first DistributeShardsLike has the strongest binding. We
  // have to handle this first

  if (!config.defaultDistributeShardsLike.empty() &&
      !distributeShardsLike.has_value() &&
      name != config.defaultDistributeShardsLike) {
    distributeShardsLike = config.defaultDistributeShardsLike;
  }

  if (!shardKeys.has_value()) {
    setDefaultShardKeys();
  }

  if (distributeShardsLike.has_value()) {
    auto groupInfo =
        config.getCollectionGroupSharding(distributeShardsLike.value());
    if (!groupInfo.ok()) {
      return groupInfo.result();
    }
    if (groupInfo->distributeShardsLike.has_value() ||
        groupInfo->distributeShardsLikeCid.has_value()) {
      // We are creating a chain of distributeShardsLike, this is not allowed.
      // TODO: For Collection groups, we may want to allow this, as the target
      // distributeShardsLike will be the Collection group, and we have to read
      // this. This version is for original distributeShardsLike behaviour.

      // At the time this was implemented the internal structure was always
      // using CID.
      TRI_ASSERT(groupInfo->distributeShardsLikeCid.has_value());
      // This is a bit of an overkill just for the message, but we have the
      // operations in our hands right now. LongTermPlan: At this point in time
      // we should see a collection by name, not by cid, and not have two
      // values, this way we can save the lookup here.
      auto leadersLeader = config.getCollectionGroupSharding(
          groupInfo->distributeShardsLikeCid.value());
      // We cannot see a follower to a non-existent leader.
      TRI_ASSERT(leadersLeader.ok());

      return {TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE,
              "Cannot distribute shards like '" + distributeShardsLike.value() +
                  "' it is already distributed like '" + leadersLeader->name +
                  "'."};
    }
    // Copy the relevant attributes

    // We cannot have a cid set yet, this can only be set if we read from
    // agency which is not yet implemented using this path.
    TRI_ASSERT(!distributeShardsLikeCid.has_value());
    // Copy the CID value
    distributeShardsLikeCid = std::to_string(groupInfo->id.id());
    TRI_ASSERT(groupInfo->numberOfShards.has_value());
    if (numberOfShards.has_value()) {
      if (numberOfShards != groupInfo->numberOfShards) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different numberOfShards (" +
                    std::to_string(numberOfShards.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->numberOfShards.value()) + ")"};
      }
    } else {
      numberOfShards = groupInfo->numberOfShards;
    }
    TRI_ASSERT(groupInfo->writeConcern.has_value());
    if (writeConcern.has_value()) {
      if (writeConcern != groupInfo->writeConcern) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different writeConcern (" +
                    std::to_string(writeConcern.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->writeConcern.value()) + ")"};
      }
    } else {
      writeConcern = groupInfo->writeConcern;
    }
    TRI_ASSERT(groupInfo->replicationFactor.has_value());
    if (replicationFactor.has_value()) {
      if (replicationFactor != groupInfo->replicationFactor) {
        return {TRI_ERROR_BAD_PARAMETER,
                "Cannot have a different replicationFactor (" +
                    std::to_string(replicationFactor.value()) +
                    "), than the leading collection (" +
                    std::to_string(groupInfo->replicationFactor.value()) + ")"};
      }
    } else {
      replicationFactor = groupInfo->replicationFactor;
    }

    res = validateOrSetShardingStrategy(groupInfo.get());
    if (!res.ok()) {
      return res;
    }

    TRI_ASSERT(shardKeys.has_value());
    // Every existing collection has shardKeys.
    TRI_ASSERT(groupInfo->shardKeys.has_value());
    if (shardKeys.value().size() != groupInfo->shardKeys.value().size()) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot have a different number of shardKeys (" +
                  std::to_string(shardKeys.value().size()) +
                  "), than the leading collection (" +
                  std::to_string(groupInfo->shardKeys.value().size()) + ")."};
    }
  } else {
    auto strategyResult = validateOrSetDefaultShardingStrategy();
    if (strategyResult.fail()) {
      return strategyResult;
    }
  }

  res = validateShardKeys();
  if (res.fail()) {
    return res;
  }

  res = validateSmartJoin();
  if (res.fail()) {
    return res;
  }

  res = ClusteringProperties::applyDefaultsAndValidateDatabaseConfiguration(
      config);
  if (res.fail()) {
    return res;
  }

  if (isSatellite()) {
    // We are a satellite, we cannot be smart at the same time
    if (isSmart) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmart' and replicationFactor 'satellite' cannot be combined"};
    }
    if (isSmartChild) {
      return {TRI_ERROR_BAD_PARAMETER,
              "'isSmartChild' and replicationFactor 'satellite' cannot be "
              "combined"};
    }
    // Has to be set by now.
    TRI_ASSERT(shardKeys.has_value());
    if (shardKeys.value().size() != 1 ||
        shardKeys.value().at(0) != StaticStrings::KeyString) {
      return {TRI_ERROR_BAD_PARAMETER, "'satellite' cannot use shardKeys"};
    }
  }
  if (std::holds_alternative<AutoIncrementGeneratorProperties>(keyOptions)) {
    // AutoIncrement is not allowed with any other than 1 shard.
    if (numberOfShards.value() != 1) {
      return {TRI_ERROR_CLUSTER_UNSUPPORTED,
              "the specified key generator is not "
              "supported for collections with more than one shard"};
    }
  }

#ifdef USE_ENTERPRISE
  res = validateOrSetSmartEdgeValidators();
  if (res.fail()) {
    return res;
  }
#endif

  return {TRI_ERROR_NO_ERROR};
}

Result UserInputCollectionProperties::validateOrSetDefaultShardingStrategy() {
  if (isSmart) {
#ifdef USE_ENTERPRISE
    return validateOrSetDefaultShardingStrategyEE();
#endif
  }
  if (shardingStrategy.has_value()) {
    if (shardingStrategy.value() == "hash" ||
        shardingStrategy.value() == "community-compat" ||
        shardingStrategy.value() == "enterprise-compat") {
      return {TRI_ERROR_NO_ERROR};
    }
    return {TRI_ERROR_BAD_PARAMETER,
            "Invalid sharding strategy " + shardingStrategy.value()};
  }
  shardingStrategy = "hash";
  return {TRI_ERROR_NO_ERROR};
}

void UserInputCollectionProperties::setDefaultShardKeys() {
  TRI_ASSERT(!shardKeys.has_value());
  if (isSmart) {
#ifdef USE_ENTERPRISE
    setDefaultShardKeysEE();
    return;
#endif
  }
  shardKeys = std::vector<std::string>{StaticStrings::KeyString};
}

Result UserInputCollectionProperties::validateShardKeys() {
  // Has to be set by now
  TRI_ASSERT(shardKeys.has_value());

  // TODO: Copy Pasted Code to validate ShardKeys, maybe move!
  auto const& keys = shardKeys.value();

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

Result UserInputCollectionProperties::validateOrSetShardingStrategy(
    UserInputCollectionProperties const& leadingCollection) {
  TRI_ASSERT(leadingCollection.shardingStrategy.has_value());
  if (isSmart) {
#ifdef USE_ENTERPRISE
    return validateOrSetShardingStrategyEE(leadingCollection);
#else
    return {TRI_ERROR_ONLY_ENTERPRISE,
            "Smart collections are only available in Enterprise version."};
#endif
  }
  if (shardingStrategy.has_value()) {
    if (shardingStrategy != leadingCollection.shardingStrategy) {
      return {TRI_ERROR_BAD_PARAMETER,
              "Cannot have a different sharding strategy (" +
                  shardingStrategy.value() +
                  "), than the leading collection (" +
                  leadingCollection.shardingStrategy.value() + ")"};
    }
  } else {
    shardingStrategy = leadingCollection.shardingStrategy;
  }
  return TRI_ERROR_NO_ERROR;
}

Result UserInputCollectionProperties::validateSmartJoin() {
  if (smartJoinAttribute.has_value()) {
#ifdef USE_ENTERPRISE
    return validateSmartJoinEE();
#else
    return {TRI_ERROR_ONLY_ENTERPRISE,
            "SmartJoin collections are only available in Enterprise version."};
#endif
  }
  return TRI_ERROR_NO_ERROR;
}
