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

#ifndef ARANGOD_SHARDING_SHARDING_FEATURE_H
#define ARANGOD_SHARDING_SHARDING_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Common.h"
#include "Sharding/ShardingStrategy.h"

#include <velocypack/Slice.h>

namespace arangodb {

class ShardingInfo;

class ShardingFeature : public application_features::ApplicationFeature {
 public:
  explicit ShardingFeature(application_features::ApplicationServer& server);

  void prepare() override final;
  void start() override final;

  void registerFactory(std::string const& name, ShardingStrategy::FactoryFunction const&);

  std::unique_ptr<ShardingStrategy> fromVelocyPack(arangodb::velocypack::Slice slice,
                                                   ShardingInfo* sharding);

  std::unique_ptr<ShardingStrategy> create(std::string const& name, ShardingInfo* sharding);

  /// @brief returns the name of the default sharding strategy for new
  /// collections
  std::string getDefaultShardingStrategyForNewCollection(VPackSlice const& properties) const;

 private:
  /// @brief returns the name of the default sharding strategy for existing
  /// collections without a sharding strategy assigned
  std::string getDefaultShardingStrategy(ShardingInfo const* sharding) const;

  std::unordered_map<std::string, ShardingStrategy::FactoryFunction> _factories;
};

}  // namespace arangodb

#endif