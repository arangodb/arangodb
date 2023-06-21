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

#pragma once

#include "Cluster/ClusterTypes.h"
#include "Cluster/Utils/ResponsibleServerList.h"

#include <unordered_set>
#include <vector>

namespace arangodb {

class Result;

struct PlanShardToServerMapping;

struct IShardDistributionFactory {
  virtual ~IShardDistributionFactory() = default;

  /**
   * @brief Plan shard -> [servers] mapping.
   * This has te be called once before we send the request to the agency.
   * It can be called again to select other servers in case the operation needs
   * to be retried.
   */
  [[nodiscard]] virtual auto planShardsOnServers(
      std::vector<ServerID> availableServers,
      std::unordered_set<ServerID>& serversPlanned) -> Result = 0;

  /**
   * @brief Get the List of servers for the given ShardIndex
   * @param index
   */
  [[nodiscard]] auto getServersForShardIndex(size_t index) const
      -> ResponsibleServerList;

 protected:
  std::vector<ResponsibleServerList> _shardToServerMapping{};
};
}  // namespace arangodb
