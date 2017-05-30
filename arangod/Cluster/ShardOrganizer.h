////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SHARD_ORGANIZER_H
#define ARANGOD_CLUSTER_SHARD_ORGANIZER_H 1

#include "Basics/Result.h"
#include "Cluster/ShardingSettings.h"

namespace arangodb {

class ClusterInfo;
class ShardingResult;

class ShardOrganizer {
  public:
    typedef std::unordered_map<std::string, std::vector<std::string>> ShardMap;
    typedef std::shared_ptr<ShardMap> ShardMapPtr;
  
  public:
    ShardOrganizer() = delete;
    explicit ShardOrganizer(ClusterInfo * ci) : _ci(ci) {}

  private:
    ClusterInfo* _ci;

  public:
    ShardingResult createShardMap(ShardingSettings /*const&*/); // grr

  private:
    ShardingResult createShardMap(std::string const& databaseName, std::string const& distributeShardsLike);
    ShardingResult createShardMap(uint64_t const& numberOfShards, uint64_t replicationFactor,
      std::vector<std::string> dbServers, std::vector<std::string> const& avoidServers, bool softReplicationFactor);
    ShardMap distributeShards(uint64_t numberOfShards, uint64_t replicationFactor, std::vector<std::string> const& dbServers);
};

class ShardingResult: public Result {
  public:
    ShardingResult(ShardingSettings const& settings) :
      Result(),
      resultSettings(settings) {
    }
    explicit ShardingResult() : Result() {}
    explicit ShardingResult(int errorCode) : Result(errorCode) {}
    
  public:
    ShardOrganizer::ShardMapPtr resultShards;
    ShardingSettings resultSettings;
};

}

#endif