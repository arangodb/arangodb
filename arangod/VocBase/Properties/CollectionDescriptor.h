////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/Properties/ClusteringConstantProperties.h"
#include "VocBase/Properties/ClusteringMutableProperties.h"
#include "VocBase/Properties/CollectionConstantProperties.h"
#include "VocBase/Properties/CollectionMutableProperties.h"
#include "VocBase/Properties/CollectionInternalProperties.h"
#include "VocBase/Properties/CollectionStorageProperties.h"
#include "VocBase/Properties/CollectionCreateOptions.h"
#include "VocBase/voc-types.h"

namespace arangodb {
template<typename T>
class ResultT;
struct DatabaseConfiguration;

struct CollectionDescriptor {
  CollectionConstantProperties constant{};
  CollectionInternalProperties internal{};
  ClusteringConstantProperties clusteringConstant{};
  ClusteringMutableProperties clusteringMutable{};
  CollectionMutableProperties mutableProps{};
  CollectionStorageProperties storage{};
  bool operator==(CollectionDescriptor const&) const = default;

  static CollectionDescriptor fromVelocyPack(velocypack::Slice info);

  static ResultT<CollectionDescriptor> fromCreateAPIBody(
      velocypack::Slice input, DatabaseConfiguration const& config,
      CollectionCreateOptions& options,
      bool activateBackwardsCompatibility = true);

  static ResultT<CollectionDescriptor> fromCreateAPIV8(
      velocypack::Slice properties, std::string const& name,
      TRI_col_type_e type, DatabaseConfiguration const& config);

  static ResultT<CollectionDescriptor> fromRestoreAPIBody(
      velocypack::Slice input, DatabaseConfiguration const& config);

  struct Invariants {
    [[nodiscard]] static auto isSmartConfiguration(
        CollectionDescriptor const& d) -> inspection::Status;
  };
};

[[nodiscard]] velocypack::Builder collectionCreateResponse(
    CollectionDescriptor const& d);

template<class Inspector>
auto inspect(Inspector& f, CollectionDescriptor& d) {
  auto result = f.object(d).fields(
      f.embedFields(d.constant), f.embedFields(d.internal),
      f.embedFields(d.clusteringConstant), f.embedFields(d.clusteringMutable),
      f.embedFields(d.mutableProps), f.embedFields(d.storage),
      // parsed into CollectionCreateOptions, not a collection property
      f.ignoreField("avoidServers"));

  if constexpr (isInternalContext<Inspector>) {
    return inspection::Status{std::move(result)};
  } else {
    return result.invariant(
        CollectionDescriptor::Invariants::isSmartConfiguration);
  }
}

}  // namespace arangodb