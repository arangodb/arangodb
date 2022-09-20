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

#include <vector>

namespace arangodb {

struct PlanShardToServerMapping;

struct IShardDistributionFactory {
  virtual ~IShardDistributionFactory() = default;

  /**
   * @brief Reshuffle shard -> [servers] mapping.
   * This should be used to re-calculate on new healthy servers
   * it is supposed to be used whenever we could not create shards
   * due to server errors.
   */
  virtual auto shuffle() -> void = 0;

  /**
   * @brief Get the List of server for the given ShardIndex
   * @param index
   */
  auto getServerForShardIndex(size_t index) const -> std::vector<ServerID>;

 protected:

  std::vector<std::vector<ServerID>> _shardToServerMapping{};
};
}
