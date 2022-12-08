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

#pragma once

#include "VocBase/Properties/ClusteringProperties.h"
#include "VocBase/Properties/CollectionConstantProperties.h"
#include "VocBase/Properties/CollectionMutableProperties.h"
#include "VocBase/Properties/CollectionInternalProperties.h"

namespace arangodb {
struct DatabaseConfiguration;

struct UserInputCollectionProperties : public CollectionConstantProperties,
                                       public CollectionMutableProperties,
                                       public CollectionInternalProperties,
                                       public ClusteringProperties {
  bool operator==(UserInputCollectionProperties const& other) const = default;

  [[nodiscard]] arangodb::Result applyDefaultsAndValidateDatabaseConfiguration(
      DatabaseConfiguration const& config);

 private:
  Result validateOrSetDefaultShardingStrategy();

#ifdef USE_ENTERPRISE
  Result validateOrSetDefaultShardingStrategyEE();
#endif

  void setDefaultShardKeys();

#ifdef USE_ENTERPRISE
  void setDefaultShardKeysEE();
#endif

  [[nodiscard]] arangodb::Result validateShardKeys();

#ifdef USE_ENTERPRISE
  [[nodiscard]] arangodb::Result validateShardKeysEE();
#endif

  [[nodiscard]] arangodb::Result validateOrSetShardingStrategy(
      UserInputCollectionProperties const& leadingCollection);

#ifdef USE_ENTERPRISE
  [[nodiscard]] arangodb::Result validateOrSetShardingStrategyEE(
      UserInputCollectionProperties const& leadingCollection);
#endif

  [[nodiscard]] arangodb::Result validateSmartJoin();

#ifdef USE_ENTERPRISE
  [[nodiscard]] arangodb::Result validateSmartJoinEE();
#endif
};

template<class Inspector>
auto inspect(Inspector& f, UserInputCollectionProperties& body) {
  return f.object(body).fields(
      f.template embedFields<CollectionConstantProperties>(body),
      f.template embedFields<CollectionMutableProperties>(body),
      f.template embedFields<CollectionInternalProperties>(body),
      f.template embedFields<ClusteringProperties>(body));
}

}  // namespace arangodb
