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

#include "Cluster/Utils/IShardDistributionFactory.h"

namespace arangodb {

struct EvenDistribution : public IShardDistributionFactory {
  EvenDistribution(uint64_t numberOfShards, uint64_t replicationFactor,
                   std::vector<ServerID> avoidServers,
                   bool enforceReplicationFactor);

  Result planShardsOnServers(
      std::vector<ServerID> availableServers,
      std::unordered_set<ServerID>& serversPlanned) override;

 private:
  uint64_t _numberOfShards;
  uint64_t _replicationFactor;
  std::vector<ServerID> _avoidServers;
  bool _enforceReplicationFactor;
};

}  // namespace arangodb
