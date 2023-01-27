////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <unordered_map>

#include "Basics/Identifier.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/PlanShardToServerMappping.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2::agency {

struct CollectionGroupId : basics::Identifier {
  using Identifier::Identifier;
};

/***
 * SECTION Collection Groups
 */
struct CollectionGroup {
  CollectionGroupId id;

  struct Collection {};
  std::unordered_map<CollectionID, Collection> collections;

  struct Attributes {

    struct MutableAttributes {
      std::size_t writeConcern;
      std::size_t replicationFactor;
      bool waitForSync;
    };

    MutableAttributes mutableAttributes;

    struct ImmutableAttributes {
      std::size_t numberOfShards;
    };

    ImmutableAttributes immutableAttributes;

  };
  Attributes attributes;
};

struct CollectionGroupTargetSpecification : public CollectionGroup {};

struct CollectionGroupPlanSpecification : public CollectionGroup {
  struct ShardSheaf {
    LogId replicatedLog;
  };
  std::vector<ShardSheaf> shardSheaves;
};

/***
 * SECTION Collections
 */

struct Collection {
  // TODO: Fill Attributes.
  CollectionGroupId groupId;

  struct MutableProperties {

  };

  MutableProperties mutableProperties;

  struct ImmutableProperties {

  };

  ImmutableProperties immutableProperties;
};

struct CollectionTargetSpecification : public Collection {};

struct CollectionPlanSpecification : public Collection {
  std::vector<ShardID> shardList;

  // Note this is still here for compatibility, and temporary reasons.
  // We think we can get away with just above shardList and CollectionGroups
  // as soon as everything is in place.
  PlanShardToServerMapping deprecatedShardMap;
};

}  // namespace arangodb::replication2::agency

DECLARE_HASH_FOR_IDENTIFIER(arangodb::replication2::agency::CollectionGroupId)
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::replication2::agency::CollectionGroupId)
