////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SHARDING_STRATEGY_H
#define ARANGOD_CLUSTER_SHARDING_STRATEGY_H 1

#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"

namespace arangodb {
class LogicalCollection;

namespace velocypack {
class Slice;
}

class ShardingStrategy {
  
  ShardingStrategy(ShardingStrategy const&) = delete;
  ShardingStrategy& operator=(ShardingStrategy const&) = delete;

 public:
  typedef std::function<std::unique_ptr<ShardingStrategy>(LogicalCollection*)> FactoryFunction;

  ShardingStrategy() = default; 
  virtual ~ShardingStrategy() = default; 

  virtual char const* name() const = 0;

  virtual int getResponsibleShard(arangodb::velocypack::Slice,
                                  bool docComplete, ShardID& shardID,
                                  bool& usesDefaultShardKeys,
                                  std::string const& key = "") = 0;

#if 0
  /// @brief whether or not the collection uses the default shard keys
  virtual bool usesDefaultShardKeys() = 0;
#endif
  /// @brief whether or not the shard keys passed are the default
  /// shard keys
  static bool usesDefaultShardKeys(std::vector<std::string> const& shardKeys);
};

}

#endif
