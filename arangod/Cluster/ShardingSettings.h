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

#ifndef ARANGOD_CLUSTER_SHARDING_SETTINGS_H
#define ARANGOD_CLUSTER_SHARDING_SETTINGS_H 1

#include "VocBase/LogicalCollection.h"

namespace arangodb {

class ShardingSettings {
  public:
    ShardingSettings(LogicalCollection const* col) :
      _databaseName(col->vocbase()->name()),
      _numberOfShards(col->numberOfShards()),
      _replicationFactor(col->replicationFactor()),
      _distributeShardsLike(col->distributeShardsLike()),
      _createIndependentOnShardsLikeError(false),
      _avoidServers(),
      _softReplicationFactor(col->isSystem()) { // system collections HAVE to be created regardless of numServers
    }

    ShardingSettings() : 
      _databaseName(""),
      _numberOfShards(0),
      _replicationFactor(0),
      _distributeShardsLike(""),
      _createIndependentOnShardsLikeError(false),
      _avoidServers(),
      _softReplicationFactor(false) {
    }
    ShardingSettings(ShardingSettings const& other) { *this = other; }

    ShardingSettings& operator=(ShardingSettings const& other) {
      _databaseName = other._databaseName;
      _numberOfShards = other._numberOfShards;
      _replicationFactor = other._replicationFactor;
      _distributeShardsLike = other._distributeShardsLike;
      _createIndependentOnShardsLikeError = other._createIndependentOnShardsLikeError;
      _avoidServers = other._avoidServers;
      _softReplicationFactor = other._softReplicationFactor;
      return *this;
    }

    ShardingSettings(ShardingSettings&& other) :
      _databaseName(std::move(other._databaseName)),
      _numberOfShards(std::move(other._numberOfShards)),
      _replicationFactor(std::move(other._replicationFactor)),
      _distributeShardsLike(std::move(other._distributeShardsLike)),
      _createIndependentOnShardsLikeError(std::move(other._createIndependentOnShardsLikeError)), 
      _avoidServers(std::move(other._avoidServers)),
      _softReplicationFactor(std::move(other._softReplicationFactor)) {
    }

    ShardingSettings& operator=(ShardingSettings&& other) {
      _databaseName = std::move(other._databaseName);
      _numberOfShards = std::move(other._numberOfShards);
      _replicationFactor = std::move(other._replicationFactor);
      _distributeShardsLike = std::move(other._distributeShardsLike);
      _createIndependentOnShardsLikeError = std::move(other._createIndependentOnShardsLikeError);
      _avoidServers = std::move(other._avoidServers);
      _softReplicationFactor = std::move(other._softReplicationFactor);
      return *this;
    }

    ShardingSettings(std::string const& databaseName, uint64_t const& numberOfShards,
      uint64_t const& replicationFactor, std::string const& distributeShardsLike,
      bool createIndependentOnShardsLikeError, std::vector<std::string> avoidServers,
      bool softReplicationFactor) :
      _databaseName(databaseName),
      _numberOfShards(numberOfShards),
      _replicationFactor(replicationFactor),
      _distributeShardsLike(distributeShardsLike),
      _createIndependentOnShardsLikeError(createIndependentOnShardsLikeError),
      _avoidServers(avoidServers),
      _softReplicationFactor(softReplicationFactor) {
    }

  public:
    std::string databaseName() { return _databaseName; }
    uint64_t numberOfShards() { return _numberOfShards; }
    uint64_t replicationFactor() { return _replicationFactor; }
    std::string distributeShardsLike() { return _distributeShardsLike; }
    bool createIndependentOnShardsLikeError() { return _createIndependentOnShardsLikeError; }
    std::vector<std::string> avoidServers() { return _avoidServers; }
    bool softReplicationFactor() { return _softReplicationFactor; }

    void databaseName(std::string const& databaseName) { _databaseName = databaseName; }
    void numberOfShards(uint64_t numberOfShards) { _numberOfShards = numberOfShards; }
    void replicationFactor(uint64_t replicationFactor) { _replicationFactor = replicationFactor; }
    void distributeShardsLike(std::string distributeShardsLike) { _distributeShardsLike = distributeShardsLike; }
    // if distributeShardsLike doesn't work just create them in a best fit variant
    void createIndependentOnShardsLikeError(bool createIndependentOnShardsLikeError) {
      _createIndependentOnShardsLikeError = createIndependentOnShardsLikeError;
    }
    void avoidServers(std::vector<std::string> const& avoidServers) { _avoidServers = avoidServers; }
    // disable hard replication factor check and create collection with whatever we have
    void softReplicationFactor(bool softReplicationFactor) { _softReplicationFactor = softReplicationFactor; }

  private:
    std::string _databaseName;
    uint64_t _numberOfShards;
    uint64_t _replicationFactor;
    std::string _distributeShardsLike;
    bool _createIndependentOnShardsLikeError;
    std::vector<std::string> _avoidServers;
    bool _softReplicationFactor;
};

}

#endif