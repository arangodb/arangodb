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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Sharding/ShardingStrategy.h"

#include <velocypack/Slice.h>

#include <span>
#include <mutex>

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
  bool usesDefaultShardKeys() const noexcept override { return true; }

  ResultT<ShardID> getResponsibleShard(arangodb::velocypack::Slice slice,
                                       bool docComplete,
                                       bool& usesDefaultShardKeys,
                                       std::string_view key) override;
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
  bool usesDefaultShardKeys() const noexcept override { return true; }

  /// @brief will always throw an exception telling the user the selected
  /// sharding is only available in the Enterprise Edition
  ResultT<ShardID> getResponsibleShard(arangodb::velocypack::Slice slice,
                                       bool docComplete,
                                       bool& usesDefaultShardKeys,
                                       std::string_view key) override;

 private:
  /// @brief name of the sharding strategy we are replacing
  std::string const _name;
};

/// @brief base class for hash-based sharding
class ShardingStrategyHashBase : public ShardingStrategy {
 public:
  explicit ShardingStrategyHashBase(ShardingInfo* sharding);

  virtual ResultT<ShardID> getResponsibleShard(
      arangodb::velocypack::Slice slice, bool docComplete,
      bool& usesDefaultShardKeys, std::string_view key) override;

  /// @brief does not really matter here
  bool usesDefaultShardKeys() const noexcept override {
    return _usesDefaultShardKeys;
  }

  virtual uint64_t hashByAttributes(arangodb::velocypack::Slice slice,
                                    std::span<std::string const> attributes,
                                    bool docComplete, ErrorCode& error,
                                    std::string_view key);

 protected:
  std::span<ShardID const> determineShards();

  ShardingInfo* _sharding;
  bool _usesDefaultShardKeys;

 private:
  std::atomic_bool _shardsSet;
  std::mutex _shardsSetMutex;
  std::vector<ShardID> _shards;
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
                            std::span<std::string const> attributes,
                            bool docComplete, ErrorCode& error,
                            std::string_view key) override final;
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

  bool isCompatible(ShardingStrategy const* other) const override;
};

}  // namespace arangodb
