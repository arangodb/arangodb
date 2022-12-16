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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "search/filter.hpp"
#include "search/search_range.hpp"
#include "utils/type_limits.hpp"

#include "Basics/debugging.h"
#include "IResearch/Geo.h"
#include "Geo/ShapeContainer.h"

namespace arangodb {
namespace iresearch {

class GeoFilter;
class GeoDistanceFilter;

enum class GeoFilterType : uint32_t {
  // Check if a given shape intersects indexed data
  INTERSECTS = 0,

  // Check if a given shape fully contains indexed data
  CONTAINS,

  // Check if a given shape is fully contained within indexed data
  IS_CONTAINED
};

// Options for geo filter
struct GeoFilterOptions {
 public:
  using filter_type = GeoFilter;

  bool operator==(const GeoFilterOptions& rhs) const noexcept {
    return type == rhs.type && shape.equals(&rhs.shape);
  }

  size_t hash() const noexcept {
    using GeoFilterUnderlyingType = std::underlying_type_t<GeoFilterType>;
    size_t hash = std::hash<GeoFilterUnderlyingType>()(
        static_cast<GeoFilterUnderlyingType>(type));

    auto* region = shape.region();
    TRI_ASSERT(region);

    std::vector<S2CellId> cells;
    region->GetCellUnionBound(&cells);

    for (auto cell : cells) {
      hash = irs::hash_combine(hash, S2CellIdHash()(cell));
    }
    return hash;
  }

  geo::ShapeContainer shape;
  std::string prefix;
  S2RegionTermIndexer::Options options;
  GeoFilterType type{GeoFilterType::INTERSECTS};
};

// User-side geo filter
class GeoFilter final : public irs::filter_base<GeoFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoFilter";
  }

  using filter::prepare;

  prepared::ptr prepare(irs::index_reader const& rdr, irs::Order const& ord,
                        irs::score_t boost,
                        irs::attribute_provider const* /*ctx*/) const override;
};  // GeoFilter

// Options for term filter
class GeoDistanceFilterOptions {
 public:
  using filter_type = GeoDistanceFilter;

  bool operator==(const GeoDistanceFilterOptions& rhs) const noexcept {
    return origin == rhs.origin && range == rhs.range;
  }

  size_t hash() const noexcept {
    return irs::hash_combine(range.hash(), S2PointHash()(origin));
  }

  S2Point origin;
  irs::search_range<double_t> range;
  std::string prefix;
  S2RegionTermIndexer::Options options;
};

// User-side geo distance filter
class GeoDistanceFilter final
    : public irs::filter_base<GeoDistanceFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoFilter";
  }

  using filter::prepare;

  prepared::ptr prepare(const irs::index_reader& rdr, const irs::Order& ord,
                        irs::score_t boost,
                        const irs::attribute_provider* /*ctx*/) const override;
};

}  // namespace iresearch
}  // namespace arangodb
