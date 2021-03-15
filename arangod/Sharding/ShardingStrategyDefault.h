////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

  ErrorCode getResponsibleShard(arangodb::velocypack::Slice slice, bool docComplete,
                                ShardID& shardID, bool& usesDefaultShardKeys,
                                arangodb::velocypack::StringRef const& key) override;
};

/// @brief a sharding class used to indicate that the selected sharding strategy
/// is only available in the Enterprise Edition of ArangoDB
/// calling getResponsibleShard on this class will always throw an exception
/// with an appropriate error message
class ShardingStrategyOnlyInEnterprise final : public ShardingStrategy {
 public:
  explicit ShardingStrategyOnlyInEnterprise(std::string const& name);

  std::string const& name() const override { return _name; }

  /// @brief does not really matter here
  bool usesDefaultShardKeys() override { return true; }

  /// @brief will always throw an exception telling the user the selected
  /// sharding is only available in the Enterprise Edition
  ErrorCode getResponsibleShard(arangodb::velocypack::Slice slice, bool docComplete,
                                ShardID& shardID, bool& usesDefaultShardKeys,
                                arangodb::velocypack::StringRef const& key) override;

 private:
  /// @brief name of the sharding strategy we are replacing
  std::string const _name;
};

/// @brief base class for hash-based sharding
class ShardingStrategyHashBase : public ShardingStrategy {
 public:
  explicit ShardingStrategyHashBase(ShardingInfo* sharding);

  virtual ErrorCode getResponsibleShard(arangodb::velocypack::Slice slice, bool docComplete,
                                        ShardID& shardID, bool& usesDefaultShardKeys,
                                        arangodb::velocypack::StringRef const& key) override;

  /// @brief does not really matter here
  bool usesDefaultShardKeys() override { return _usesDefaultShardKeys; }

  virtual uint64_t hashByAttributes(arangodb::velocypack::Slice slice,
                                    std::vector<std::string> const& attributes,
                                    bool docComplete, ErrorCode& error,
                                    arangodb::velocypack::StringRef const& key);

 private:
  void determineShards();

 protected:
  ShardingInfo* _sharding;
  std::vector<ShardID> _shards;
  bool _usesDefaultShardKeys;
  std::atomic<bool> _shardsSet;
  Mutex _shardsSetMutex;
};

/// @brief old version of the sharding used in the Community Edition
/// this is DEPRECATED and should not be used for new collections
class ShardingStrategyCommunityCompat final : public ShardingStrategyHashBase {
 public:
  explicit ShardingStrategyCommunityCompat(ShardingInfo* sharding);

  std::string const& name() const override { return NAME; }

  static std::string const NAME;
};

/// @brief old version of the sharding used in the Enterprise Edition
/// this is DEPRECATED and should not be used for new collections
class ShardingStrategyEnterpriseBase : public ShardingStrategyHashBase {
 public:
  explicit ShardingStrategyEnterpriseBase(ShardingInfo* sharding);

 protected:
  /// @brief this implementation of "hashByAttributes" is slightly different
  /// than the implementation in the Community Edition
  /// we leave the differences in place, because making any changes here
  /// will affect the data distribution, which we want to avoid
  uint64_t hashByAttributes(arangodb::velocypack::Slice slice,
                            std::vector<std::string> const& attributes,
                            bool docComplete, ErrorCode& error,
                            arangodb::velocypack::StringRef const& key) override final;
};

/// @brief old version of the sharding used in the Enterprise Edition
/// this is DEPRECATED and should not be used for new collections
class ShardingStrategyEnterpriseCompat : public ShardingStrategyEnterpriseBase {
 public:
  explicit ShardingStrategyEnterpriseCompat(ShardingInfo* sharding);

  std::string const& name() const override { return NAME; }

  static std::string const NAME;
};

/// @brief default hash-based sharding strategy
/// used for new collections from 3.4 onwards
class ShardingStrategyHash final : public ShardingStrategyHashBase {
 public:
  explicit ShardingStrategyHash(ShardingInfo* sharding);

  std::string const& name() const override { return NAME; }

  static std::string const NAME;
};

}  // namespace arangodb

#endif
