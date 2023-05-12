////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <vector>
#include <set>
#include <unordered_set>

#include "Pregel/GraphStore/PregelShard.h"
#include "Pregel/GraphStore/LoadableVertexShard.h"
#include "Pregel/DatabaseTypes.h"

#include "CrashHandler/CrashHandler.h"
#include "Assertions/ProdAssert.h"
#include "Containers/Enumerate.h"

namespace arangodb::pregel {

struct GraphSerdeConfig {
  std::vector<LoadableVertexShard> loadableVertexShards;

  [[nodiscard]] auto collectionName(PregelShard pregelShard) const
      -> std::string const& {
    ADB_PROD_ASSERT(pregelShard.value < loadableVertexShards.size());
    return loadableVertexShards.at(pregelShard.value).collectionName;
  }

  [[nodiscard]] auto shardID(PregelShard pregelShard) const -> ShardID {
    ADB_PROD_ASSERT(pregelShard.value < loadableVertexShards.size());
    return loadableVertexShards.at(pregelShard.value).vertexShard;
  }
  [[nodiscard]] auto pregelShard(ShardID responsibleShard) const
      -> PregelShard {
    for (auto const& lvs : loadableVertexShards) {
      if (lvs.vertexShard == responsibleShard) {
        return lvs.pregelShard;
      }
    }

    // TODO: ADB_PROD_ASSERT(false) << fmt::format("could not find PregelShard
    // for {}", responsibleShard);
    return InvalidPregelShard;
  }

  // Actual set of pregel shard id's located here
  [[nodiscard]] auto localPregelShardIDs(ServerID server) const
      -> std::set<PregelShard> {
    auto result = std::set<PregelShard>{};

    for (auto&& loadableVertexShard : loadableVertexShards) {
      if (loadableVertexShard.responsibleServer == server) {
        result.insert(loadableVertexShard.pregelShard);
      }
    }
    return result;
  }
  // Actual set of pregel shard id's located here
  [[nodiscard]] auto loadableVertexShardsForServer(ServerID server) const
      -> std::vector<LoadableVertexShard> {
    auto result = std::vector<LoadableVertexShard>{};

    for (auto&& loadableVertexShard : loadableVertexShards) {
      if (loadableVertexShard.responsibleServer == server) {
        result.push_back(loadableVertexShard);
      }
    }
    return result;
  }
  //
  [[nodiscard]] auto localShardIDs(ServerID server) const -> std::set<ShardID> {
    auto result = std::set<ShardID>{};

    for (auto&& loadableVertexShard : loadableVertexShards) {
      if (loadableVertexShard.responsibleServer == server) {
        result.insert(loadableVertexShard.vertexShard);
      }
    }
    return result;
  }

  [[nodiscard]] auto responsibleServerSet() const -> std::set<ServerID> {
    auto result = std::set<ServerID>{};
    for (auto&& loadableVertexShard : loadableVertexShards) {
      result.insert(loadableVertexShard.responsibleServer);
    }
    return result;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, GraphSerdeConfig& x) {
  return f.object(x).fields(
      f.field("loadableVertexShards", x.loadableVertexShards));
}

}  // namespace arangodb::pregel
