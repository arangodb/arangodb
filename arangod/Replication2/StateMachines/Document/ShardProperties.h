////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Cluster/ClusterTypes.h"

#include <unordered_map>

namespace arangodb::replication2::replicated_state::document {
inline constexpr auto kStringCollectionID = std::string_view{"collectionId"};
inline constexpr auto kStringProperties = std::string_view{"properties"};

struct ShardProperties {
  CollectionID collectionId;
  std::shared_ptr<VPackBuilder> properties;

  template<class Inspector>
  inline friend auto inspect(Inspector& f, ShardProperties& s) {
    return f.object(s).fields(f.field(kStringCollectionID, s.collectionId),
                              f.field(kStringProperties, s.properties));
  }
};

using ShardMap = std::unordered_map<ShardID, ShardProperties>;
}  // namespace arangodb::replication2::replicated_state::document
