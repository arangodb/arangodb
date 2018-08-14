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

#ifndef ARANGOD_SHARDING_SHARDING_STRATEGY_DEFAULT_H
#define ARANGOD_SHARDING_SHARDING_STRATEGY_DEFAULT_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Sharding/ShardingStrategy.h"

#include <velocypack/Slice.h>

namespace arangodb {
class ShardingInfo;

/// @brief a sharding implementation that will always fail when asking
/// for a shard. this can be used on the DB server or on the single server
class ShardingStrategyNone final : public ShardingStrategy {
 public:
  ShardingStrategyNone();

  std::string const& name() const override { return NAME; }

  static std::string const NAME;

  /// @brief does not really matter here
  bool usesDefaultShardKeys() override { return true; }

  int getResponsibleShard(arangodb::velocypack::Slice,
                          bool docComplete, ShardID& shardID,
                          bool& usesDefaultShardKeys,
                          std::string const& key = "") override;
};

/// @brief base class for hash-based sharding
class ShardingStrategyHashBase : public ShardingStrategy {
 public:
  explicit ShardingStrategyHashBase(ShardingInfo* sharding);

  virtual int getResponsibleShard(arangodb::velocypack::Slice,
                                  bool docComplete, ShardID& shardID,
                                  bool& usesDefaultShardKeys,
                                  std::string const& key = "") override;

  /// @brief does not really matter here
  bool usesDefaultShardKeys() override { return _usesDefaultShardKeys; }

  virtual uint64_t hashByAttributes(
    arangodb::velocypack::Slice slice, std::vector<std::string> const& attributes,
    bool docComplete, int& error, std::string const& key);

 private:
  void determineShards();

 protected:
  ShardingInfo* _sharding;
  std::vector<ShardID> _shards;
  bool _usesDefaultShardKeys;
  std::atomic<bool> _shardsSet;
  Mutex _shardsSetMutex;
};

/// @brief old version of the sharding used in the community edition
/// this is DEPRECATED and should not be used for new collections
class ShardingStrategyCommunityCompat final : public ShardingStrategyHashBase {
 public:
  explicit ShardingStrategyCommunityCompat(ShardingInfo* sharding);

  std::string const& name() const override { return NAME; }

  static std::string const NAME;
};

/// @brief old version of the sharding used in the community edition
/// this is DEPRECATED and should not be used for new collections
class ShardingStrategyHash final : public ShardingStrategyHashBase {
 public:
  explicit ShardingStrategyHash(ShardingInfo* sharding);

  std::string const& name() const override { return NAME; }

  static std::string const NAME;
};

}

#endif
