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

#include "Indexes/IndexDefinition.h"

namespace arangodb {

namespace application_features {
class ApplicationServer;
}  // namespace application_features

namespace iresearch {

class IResearchInvertedIndexDefinition : public IndexDefinition {
 public:
  explicit IResearchInvertedIndexDefinition(
      application_features::ApplicationServer& server);

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const final;

  /// @brief normalize an Index definition prior to instantiation/persistence
  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const final;

  bool attributeOrderMatters() const final { return false; }

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const final {
    return factory.createInverted(collection, definition, id,
                                  isClusterConstructor);
  }

 private:
  // needed by IResearchInvertedIndexMeta
  application_features::ApplicationServer& _server;
};

}  // namespace iresearch
}  // namespace arangodb
