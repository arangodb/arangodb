////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_INDEX_FACTORY_H
#define ARANGOD_CLUSTER_CLUSTER_INDEX_FACTORY_H 1

#include "Indexes/IndexFactory.h"

namespace arangodb {

class ClusterIndexFactory final : public IndexFactory {
 public:
  ClusterIndexFactory();
  ~ClusterIndexFactory() = default;
  
  Result enhanceIndexDefinition(
    velocypack::Slice const definition,
    velocypack::Builder& normalized,
    bool isCreation,
    bool isCoordinator
  ) const override;

  void fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes
  ) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(
    LogicalCollection& col,
    arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
  ) const override;
};

}

#endif
