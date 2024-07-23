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

#include "Sharding/ShardingStrategy.h"
#include "RestServer/arangod.h"

#include <velocypack/Slice.h>

namespace arangodb {

class ShardingInfo;

class ShardingFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Sharding"; }

  explicit ShardingFeature(Server& server);

  void prepare() override final;
  void start() override final;

  void registerFactory(std::string const& name,
                       ShardingStrategy::FactoryFunction const&);

  std::unique_ptr<ShardingStrategy> fromVelocyPack(
      arangodb::velocypack::Slice slice, ShardingInfo* sharding);

  std::unique_ptr<ShardingStrategy> create(std::string const& name,
                                           ShardingInfo* sharding);

  /// @brief returns the name of the default sharding strategy for new
  /// collections
  std::string getDefaultShardingStrategyForNewCollection(
      VPackSlice const& properties) const;

 private:
  /// @brief returns the name of the default sharding strategy for existing
  /// collections without a sharding strategy assigned
  std::string getDefaultShardingStrategy(ShardingInfo const* sharding) const;

  std::unordered_map<std::string, ShardingStrategy::FactoryFunction> _factories;
};

}  // namespace arangodb
