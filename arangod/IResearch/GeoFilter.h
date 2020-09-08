////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_GEO_FILTER
#define ARANGODB_IRESEARCH__IRESEARCH_GEO_FILTER 1

#include <s2/s2region_term_indexer.h>

#include "search/filter.hpp"
#include "utils/type_limits.hpp"

#include "Basics/debugging.h"
#include "Geo/ShapeContainer.h"

namespace arangodb {
namespace iresearch {

class GeoFilter;

enum class GeoFilterType : uint32_t {
  NEAR = 0,
  INTERSECTS,
  CONTAINS,
  IS_CONTAINED
};

////////////////////////////////////////////////////////////////////////////////
/// @struct by_term_options
/// @brief options for term filter
////////////////////////////////////////////////////////////////////////////////
class GeoFilterOptions {
 public:
  using filter_type = GeoFilter;

  bool operator==(const GeoFilterOptions& rhs) const noexcept {
    return type == rhs.type && shape.equals(&rhs.shape);
  }

  size_t hash() const noexcept {
    size_t hash = std::hash<decltype(type)>()(type);

    auto* region = shape.region();
    TRI_ASSERT(region);

    std::vector<S2CellId> cells;
    region->GetCellUnionBound(&cells);

    for (auto cell: cells) {
      hash = irs::hash_combine(hash, S2CellIdHash()(cell));
    }
    return hash;
  }

  geo::ShapeContainer shape;
  std::string storedField;
  std::string prefix;
  S2RegionTermIndexer::Options options;
  GeoFilterType type{GeoFilterType::NEAR};

}; // GeoFilterOptions

//////////////////////////////////////////////////////////////////////////////
/// @class by_geo_distance
/// @brief user-side geo distance filter
//////////////////////////////////////////////////////////////////////////////
class GeoFilter final
  : public irs::filter_base<GeoFilterOptions>{
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "arangodb::iresearch::GeoFilter";
  }

  DECLARE_FACTORY();

  using filter::prepare;

  virtual prepared::ptr prepare(
    const irs::index_reader& rdr,
    const irs::order::prepared& ord,
    irs::boost_t boost,
    const irs::attribute_provider* /*ctx*/) const;
}; // by_geo_terms

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_GEO_FILTER
