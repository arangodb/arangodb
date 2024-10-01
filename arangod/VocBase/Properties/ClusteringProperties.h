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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/Properties/ClusteringMutableProperties.h"
#include "VocBase/Properties/ClusteringConstantProperties.h"

namespace arangodb {
class Result;

struct UserInputCollectionProperties;

struct ClusteringProperties : public ClusteringMutableProperties,
                              public ClusteringConstantProperties {
  bool operator==(ClusteringProperties const& other) const = default;

  [[nodiscard]] arangodb::Result applyDefaultsAndValidateDatabaseConfiguration(
      DatabaseConfiguration const& config);
};

template<class Inspector>
auto inspect(Inspector& f, ClusteringProperties& props) {
  return f.object(props).fields(
      f.template embedFields<ClusteringMutableProperties>(props),
      f.template embedFields<ClusteringConstantProperties>(props));
}
}  // namespace arangodb
