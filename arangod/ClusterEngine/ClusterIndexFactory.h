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

#include "Indexes/IndexFactory.h"

namespace arangodb {

class ClusterEngine;

class ClusterIndexFactory final : public IndexFactory {
 public:
  ClusterIndexFactory(application_features::ApplicationServer&,
                      ClusterEngine& engine, IndexTypeCatalog const& catalog);
  ~ClusterIndexFactory() = default;

  void fillSystemIndexes(
      LogicalCollection& col,
      std::vector<std::shared_ptr<Index>>& systemIndexes) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(
      LogicalCollection& col, velocypack::Slice indexesSlice,
      std::vector<std::shared_ptr<Index>>& indexes) const override;

  IndexTypeFactory const& factoryFor(IndexType type) const noexcept override;

 private:
  ClusterEngine& _engine;

  // one factory per type, owned by IndexFactory::_owned
  IndexTypeFactory const* const _edge;
  IndexTypeFactory const* const _fulltext;
  IndexTypeFactory const* const _geo;
  IndexTypeFactory const* const _geo1;
  IndexTypeFactory const* const _geo2;
  IndexTypeFactory const* const _hash;
  IndexTypeFactory const* const _persistent;
  IndexTypeFactory const* const _primary;
  IndexTypeFactory const* const _skiplist;
  IndexTypeFactory const* const _ttl;
  IndexTypeFactory const* const _zkd;
  IndexTypeFactory const* const _mdi;
  IndexTypeFactory const* const _mdiPrefixed;
  IndexTypeFactory const* const _inverted;
  IndexTypeFactory const* const _vector;
};

}  // namespace arangodb
