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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Result.h"
#include "Indexes/IndexDefinitions.h"
#include "Indexes/IndexType.h"
#include "IResearch/IResearchRocksDBInvertedIndex.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {

struct Database;
struct IVectorIndexProvider;

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

// Index type names and equal/normalize rules, shared by both engines.
class IndexTypeCatalog final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "IndexTypeCatalog";
  }

  IndexTypeCatalog(application_features::ApplicationServer& server,
                   IVectorIndexProvider const& vectorIndexProvider);

  IndexType resolve(std::string_view name) const noexcept;

  Result normalizeType(std::string_view name, velocypack::Builder& normalized,
                       velocypack::Slice definition, bool isCreation,
                       Database const& vocbase) const;

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const& dbname) const;

  std::vector<std::pair<std::string_view, std::string_view>> aliases(
      uint32_t apiVersion) const;

  // definition must outlive the catalog
  Result add(std::string_view name, IndexDefinition const& definition);

  EdgeIndexDefinition const& edge() const noexcept { return _edge; }
  FulltextIndexDefinition const& fulltext() const noexcept { return _fulltext; }
  GeoIndexDefinition const& geo() const noexcept { return _geo; }
  Geo1IndexDefinition const& geo1() const noexcept { return _geo1; }
  Geo2IndexDefinition const& geo2() const noexcept { return _geo2; }
  SecondaryIndexDefinition const& hash() const noexcept { return _hash; }
  SecondaryIndexDefinition const& persistent() const noexcept {
    return _persistent;
  }
  SecondaryIndexDefinition const& skiplist() const noexcept {
    return _skiplist;
  }
  PrimaryIndexDefinition const& primary() const noexcept { return _primary; }
  TtlIndexDefinition const& ttl() const noexcept { return _ttl; }
  MdiIndexDefinition const& zkd() const noexcept { return _zkd; }
  MdiIndexDefinition const& mdi() const noexcept { return _mdi; }
  MdiPrefixedIndexDefinition const& mdiPrefixed() const noexcept {
    return _mdiPrefixed;
  }
  VectorIndexDefinition const& vector() const noexcept { return _vector; }
  iresearch::IResearchInvertedIndexDefinition const& inverted() const noexcept {
    return _inverted;
  }

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
  iresearch::IResearchInvertedIndexDefinition _inverted;

  std::unordered_map<std::string, IndexDefinition const*> _byName;
};

}  // namespace arangodb
