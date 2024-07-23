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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "EvenDistribution.h"

#include "Basics/debugging.h"
#include "Inspection/Format.h"
#include "Logger/LogMacros.h"

#include <numeric>
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

auto EvenDistribution::checkDistributionPossible(
    std::vector<ServerID>& availableServers) -> Result {
  if (availableServers.empty()) {
    return {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS,
            "Do not have a single server to make responsible for shards"};
  }

  if (_enforceReplicationFactor &&
      availableServers.size() < _replicationFactor) {
    return {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS};
  }

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
  return {};
}

auto EvenDistribution::planShardsOnServers(
    std::vector<ServerID> availableServers,
    std::unordered_set<ServerID>& serversPlanned) -> Result {
  // Caller needs to ensure we have something to place shards on
  auto res = checkDistributionPossible(availableServers);
  if (res.fail()) {
    return res;
  }

  // Shuffle the servers, such that we don't always start with the same one
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(availableServers.begin(), availableServers.end(), g);

  TRI_IF_FAILURE("allShardsOnSameServer") {
    // Only one server shall remain available
    if (!availableServers.empty()) {
      std::sort(availableServers.begin(), availableServers.end());
      availableServers.resize(1);
    }
  }

  _shardToServerMapping.clear();

  // Example: Servers: A B C D E F G H I (9)
  // Replication Factor 3, k = 9 / gcd(3, 9) = 3
  // A B C
  // D E F
  // G H I  <- now we do an additional shift
  // B C D
  // E F G
  // H I A  <- shift
  // C D E
  // F G H
  // I A B

  // In case we have not enough servers available AND do not enforce replication
  // factor.
  size_t serversToPick = std::min(
      _replicationFactor, static_cast<uint64_t>(availableServers.size()));

  TRI_ASSERT(availableServers.size() > 0);
  TRI_ASSERT(_replicationFactor > 0);
  size_t k = availableServers.size() /
             std::gcd(serversToPick, availableServers.size());
  size_t offset = 0;
  for (uint64_t i = 0; i < _numberOfShards; ++i) {
    TRI_ASSERT(k != 0);
    if (i % k == 0) {
      offset += 1;
    }
    // determine responsible server(s)
    std::vector<std::string> serverIds;
    for (uint64_t j = 0; j < serversToPick; ++j) {
      auto const& candidate =
          availableServers[offset % availableServers.size()];
      offset += 1;
      serverIds.push_back(candidate);
      // remember that we use this server
      serversPlanned.emplace(candidate);
    }

    _shardToServerMapping.emplace_back(
        ResponsibleServerList{std::move(serverIds)});
  }

  return {TRI_ERROR_NO_ERROR};
}
