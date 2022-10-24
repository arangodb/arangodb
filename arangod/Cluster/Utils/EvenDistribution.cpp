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

#include "EvenDistribution.h"

#include "Basics/debugging.h"
#include "Logger/LogMacros.h"

#include <random>

using namespace arangodb;

EvenDistribution::EvenDistribution(uint64_t numberOfShards,
                                   uint64_t replicationFactor,
                                   std::vector<ServerID> avoidServers,
                                   bool enforceReplicationFactor)
    : _numberOfShards(numberOfShards),
      _replicationFactor(replicationFactor),
      _avoidServers{std::move(avoidServers)},
      _enforceReplicationFactor{enforceReplicationFactor} {}

Result EvenDistribution::planShardsOnServers(
    std::vector<ServerID> availableServers,
    std::unordered_set<ServerID>& serversPlanned) {
  // Caller needs to ensure we have something to place shards on
  TRI_ASSERT(!availableServers.empty());
  if (_enforceReplicationFactor &&
      availableServers.size() < _replicationFactor) {
    return {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS};
  }
  if (!availableServers.empty()) {
    auto serversBefore = availableServers.size();
    // Erase all servers that are not allowed
    availableServers.erase(
        std::remove_if(availableServers.begin(), availableServers.end(),
                       [&](const std::string& x) {
                         return std::find(_avoidServers.begin(),
                                          _avoidServers.end(),
                                          x) != _avoidServers.end();
                       }),
        availableServers.end());

    if (_enforceReplicationFactor &&
        availableServers.size() < _replicationFactor) {
      // Check if enough servers are left
      LOG_TOPIC("03682", DEBUG, Logger::CLUSTER)
          << "Do not have enough DBServers for requested "
             "replicationFactor,"
          << " (after considering avoid list),"
          << " nrDBServers: " << serversBefore
          << " replicationFactor: " << _replicationFactor
          << " avoid list size: " << _avoidServers.size();
      return {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS};
    }
  }

  // Shuffle what we have left.
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(availableServers.begin(), availableServers.end(), g);
  _shardToServerMapping.clear();

  // In case we have not enough servers available AND do not enforce replication
  // factor.
  auto serversToPick = std::min(_replicationFactor, availableServers.size());

  size_t leaderIndex = 0;
  size_t followerIndex = 0;
  for (uint64_t i = 0; i < _numberOfShards; ++i) {
    // determine responsible server(s)
    std::vector<ServerID> serverIds;
    for (uint64_t j = 0; j < serversToPick; ++j) {
      if (j >= availableServers.size()) {
        break;
      }
      std::string candidate;
      // mop: leader
      if (serverIds.empty()) {
        candidate = availableServers[leaderIndex++];
        if (leaderIndex >= availableServers.size()) {
          leaderIndex = 0;
        }
      } else {
        do {
          candidate = availableServers[followerIndex++];
          if (followerIndex >= availableServers.size()) {
            followerIndex = 0;
          }
        } while (candidate == serverIds[0]);  // mop: ignore leader
      }
      serverIds.push_back(candidate);
      // remember that we use this server
      serversPlanned.emplace(candidate);
    }

    _shardToServerMapping.emplace_back(
        ResponsibleServerList{std::move(serverIds)});
  }
  return {TRI_ERROR_NO_ERROR};
}
