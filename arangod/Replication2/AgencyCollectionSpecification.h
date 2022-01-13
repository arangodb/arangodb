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

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <Basics/Identifier.h>
#include "Cluster/ClusterTypes.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2::agency {

struct CollectionGroupId : basics::Identifier {
  using Identifier::Identifier;
};

struct CollectionGroup {
  CollectionGroupId id;

  struct Collection {
    explicit Collection(VPackSlice slice);
    void toVelocyPack(VPackBuilder& builder) const;
  };
  std::unordered_map<CollectionID, Collection> collections;

  struct ShardSheaf {
    LogId replicatedLog;

    explicit ShardSheaf(VPackSlice slice);
    void toVelocyPack(VPackBuilder& builder) const;
  };
  std::vector<ShardSheaf> shardSheaves;

  struct Attributes {
    std::size_t writeConcern;
    bool waitForSync;

    explicit Attributes(VPackSlice slice);
    void toVelocyPack(VPackBuilder& builder) const;
  };
  Attributes attributes;

  explicit CollectionGroup(VPackSlice slice);
  void toVelocyPack(VPackBuilder& builder) const;
};

}  // namespace arangodb::replication2::agency

DECLARE_HASH_FOR_IDENTIFIER(arangodb::replication2::agency::CollectionGroupId)
DECLARE_EQUAL_FOR_IDENTIFIER(arangodb::replication2::agency::CollectionGroupId)
