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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Indexes/IndexFactory.h"

namespace arangodb {

class ClusterIndexFactory final : public IndexFactory {
 public:
  static void linkIndexFactories(ArangodServer& server, IndexFactory& factory,
                                 ClusterEngine& engine);
  explicit ClusterIndexFactory(ArangodServer&, ClusterEngine& engine);
  ~ClusterIndexFactory() = default;

  // normalize definition
  Result enhanceIndexDefinition(velocypack::Slice const definition,
                                velocypack::Builder& normalized,
                                bool isCreation,
                                TRI_vocbase_t const& vocbase) const override;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
  /// "hash") used to display storage engine capabilities
  std::vector<std::pair<std::string_view, std::string_view>> indexAliases()
      const override;

  void fillSystemIndexes(
      LogicalCollection& col,
      std::vector<std::shared_ptr<Index>>& systemIndexes) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(
      LogicalCollection& col, velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<Index>>& indexes) const override;

 private:
  ClusterEngine& _engine;
};

}  // namespace arangodb
