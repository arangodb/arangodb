////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Indexes/IndexFactory.h"

namespace arangodb {

class ClusterIndexFactory final : public IndexFactory {
 public:
  static void linkIndexFactories(
      application_features::ApplicationServer& server, IndexFactory& factory);
  explicit ClusterIndexFactory(application_features::ApplicationServer&);
  ~ClusterIndexFactory() = default;

  Result enhanceIndexDefinition(           // normalize definition
      velocypack::Slice const definition,  // source definition
      velocypack::Builder& normalized,     // normalized definition (out-param)
      bool isCreation,                     // definition for index creation
      TRI_vocbase_t const& vocbase         // index vocbase
  ) const override;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
  /// "hash") used to display storage engine capabilities
  std::unordered_map<std::string, std::string> indexAliases() const override;

  void fillSystemIndexes(
      LogicalCollection& col,
      std::vector<std::shared_ptr<Index>>& systemIndexes) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(
      LogicalCollection& col, velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<Index>>& indexes) const override;
};

}  // namespace arangodb
