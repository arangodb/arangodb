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

#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchLinkHelper.h"
#include "Indexes/IndexDefinition.h"

namespace arangodb::iresearch {

// the arangosearch link rules, shared by the catalog and both link factories
struct IResearchLinkDefinition final : public IndexDefinition {
  explicit IResearchLinkDefinition(
      application_features::ApplicationServer& server)
      : IndexDefinition(IndexType::IResearchLink), _server(server) {}

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const override {
    return IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
  }

  Result normalize(velocypack::Builder& normalized,
                   velocypack::Slice definition, bool isCreation,
                   Database const& vocbase) const override {
    // no attribute set in a definition -> old version
    constexpr LinkVersion defaultVersion = LinkVersion::MIN;
    return IResearchLinkHelper::normalize(normalized, definition, isCreation,
                                          vocbase, defaultVersion);
  }

  std::shared_ptr<Index> create(IIndexFactory const& factory,
                                LogicalCollection& collection,
                                velocypack::Slice definition, IndexId id,
                                bool isClusterConstructor) const override {
    return factory.createIResearchLink(collection, definition, id,
                                       isClusterConstructor);
  }

 private:
  application_features::ApplicationServer& _server;
};

}  // namespace arangodb::iresearch
