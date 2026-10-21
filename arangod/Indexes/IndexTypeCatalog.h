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

#include "Basics/Result.h"
#include "Indexes/EdgeIndexDefinition.h"
#include "Indexes/FulltextIndexDefinition.h"
#include "Indexes/Geo1IndexDefinition.h"
#include "Indexes/Geo2IndexDefinition.h"
#include "Indexes/GeoIndexDefinition.h"
#include "Indexes/IndexType.h"
#include "Indexes/MdiIndexDefinition.h"
#include "Indexes/MdiPrefixedIndexDefinition.h"
#include "Indexes/PrimaryIndexDefinition.h"
#include "Indexes/SecondaryIndexDefinition.h"
#include "Indexes/TtlIndexDefinition.h"
#include "Indexes/VectorIndexDefinition.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {

namespace application_features {
class ApplicationServer;
}  // namespace application_features

struct Database;
struct IVectorIndexProvider;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// lets _byName be looked up by string_view without allocating a std::string
struct TransparentStringHash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept {
    return std::hash<std::string_view>{}(sv);
  }
};

// Index type names and equal/normalize rules, shared by both engines.
class IndexTypeCatalog final {
 public:
  IndexTypeCatalog(application_features::ApplicationServer& server,
                   IVectorIndexProvider const& vectorIndexProvider);

  [[nodiscard]] IndexType resolve(std::string_view name) const noexcept;

  // the definition registered under this name, or nullptr if unknown; used
  // by IndexFactory::prepareIndexFromSlice to dispatch create() without a
  // second, independent name->IndexType mapping
  [[nodiscard]] IndexDefinition const* definitionFor(
      std::string_view name) const noexcept;

  [[nodiscard]] Result normalizeType(std::string_view name,
                                     velocypack::Builder& normalized,
                                     velocypack::Slice definition,
                                     bool isCreation,
                                     Database const& vocbase) const;

  [[nodiscard]] bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
                           std::string const& dbname) const;

  [[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>>
  aliases(uint32_t apiVersion) const;

  // definition must outlive the catalog
  [[nodiscard]] Result add(std::string_view name,
                           IndexDefinition const& definition);

 private:
  EdgeIndexDefinition _edge;
  FulltextIndexDefinition _fulltext;
  GeoIndexDefinition _geo;
  Geo1IndexDefinition _geo1;
  Geo2IndexDefinition _geo2;
  SecondaryIndexDefinition _hash;
  SecondaryIndexDefinition _persistent;
  SecondaryIndexDefinition _skiplist;
  PrimaryIndexDefinition _primary;
  TtlIndexDefinition _ttl;
  MdiIndexDefinition _zkd;
  MdiIndexDefinition _mdi;
  MdiPrefixedIndexDefinition _mdiPrefixed;
  VectorIndexDefinition _vector;

  std::unordered_map<std::string, IndexDefinition const*, TransparentStringHash,
                     std::equal_to<>>
      _byName;
};

}  // namespace arangodb
