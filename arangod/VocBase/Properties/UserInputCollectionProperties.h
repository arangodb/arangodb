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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/Properties/CollectionConstantProperties.h"
#include "VocBase/Properties/CollectionMutableProperties.h"
#include "VocBase/Properties/CollectionInternalProperties.h"
#include "VocBase/Properties/ClusteringConstantProperties.h"
#include "VocBase/Properties/ClusteringMutableProperties.h"

namespace arangodb {
struct DatabaseConfiguration;

struct UserInputCollectionProperties : public CollectionConstantProperties,
                                       public CollectionMutableProperties,
                                       public CollectionInternalProperties,
                                       public ClusteringMutableProperties,
                                       public ClusteringConstantProperties {
  bool operator==(UserInputCollectionProperties const& other) const = default;
};

template<class Inspector>
auto inspect(Inspector& f, UserInputCollectionProperties& body) {
  return f.object(body)
      .fields(f.template embedFields<CollectionConstantProperties>(body),
              f.template embedFields<CollectionMutableProperties>(body),
              f.template embedFields<CollectionInternalProperties>(body),
              f.template embedFields<ClusteringMutableProperties>(body),
              f.template embedFields<ClusteringConstantProperties>(body));
}

}  // namespace arangodb
