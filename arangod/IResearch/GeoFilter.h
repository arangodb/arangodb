////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb::iresearch {

enum class StoredType : uint8_t {
  // VPack but with legacy parsing to S2LatLngRect and Polygon Normalization
  VPackLegacy = 0,
  // Valid GeoJson as VPack or coordinates array of two S2LatLng
  VPack,
  // Valid ShapeContainer serialized as S2Region
  S2Region,
  // Same as S2Region, but contains only S2Point
  S2Point,
  // Store centroid
  S2Centroid,
};

struct GeoFilterOptionsBase {
  std::string prefix;
  S2RegionTermIndexer::Options options;
  StoredType stored{StoredType::VPack};
  // Default value should be S2Point for bad written test
  geo::coding::Options coding{geo::coding::Options::kInvalid};
};

enum class GeoFilterType : uint8_t {
  // Check if a given shape intersects indexed data
  INTERSECTS = 0,
  // Check if a given shape fully contains indexed data
  CONTAINS,
  // Check if a given shape is fully contained within indexed data
  IS_CONTAINED,
};

class GeoFilter;

struct GeoFilterOptions : GeoFilterOptionsBase {
  using filter_type = GeoFilter;

  bool operator==(const GeoFilterOptions& rhs) const noexcept {
    return type == rhs.type && shape.equals(rhs.shape);
  }

  template<typename H>
  friend H AbslHashValue(H h, GeoFilterOptions const& self) noexcept {
    auto* region = self.shape.region();
    TRI_ASSERT(region != nullptr);
    std::vector<S2CellId> cells;
    region->GetCellUnionBound(&cells);
    for (auto cell : cells) {
      h = H::combine(std::move(h), cell);
    }
    return H::combine(std::move(h), cells.size(), self.type);
  }

  // TODO(MBkkt) remove it
  size_t hash() const noexcept { return absl::Hash<GeoFilterOptions>{}(*this); }

  GeoFilterType type{GeoFilterType::INTERSECTS};
  geo::ShapeContainer shape;
};

class GeoFilter final : public irs::filter_base<GeoFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoFilter";
  }

  using filter::prepare;

  prepared::ptr prepare(irs::IndexReader const& rdr, irs::Scorers const& ord,
                        irs::score_t boost,
                        irs::attribute_provider const* /*ctx*/) const override;
};

class GeoDistanceFilter;

struct GeoDistanceFilterOptions : GeoFilterOptionsBase {
  using filter_type = GeoDistanceFilter;

  bool operator==(GeoDistanceFilterOptions const& rhs) const noexcept {
    return origin == rhs.origin && range == rhs.range;
  }

  template<typename H>
  friend H AbslHashValue(H h, GeoDistanceFilterOptions const& self) noexcept {
    // TODO(MBkkt) remove ".hash()"
    return H::combine(std::move(h), self.range.hash(), self.origin);
  }

  // TODO(MBkkt) remove it
  size_t hash() const noexcept {
    return absl::Hash<GeoDistanceFilterOptions>{}(*this);
  }

  S2Point origin;
  irs::search_range<double> range;
};

class GeoDistanceFilter final
    : public irs::filter_base<GeoDistanceFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoDistanceFilter";
  }

  using filter::prepare;

  prepared::ptr prepare(irs::IndexReader const& rdr, irs::Scorers const& ord,
                        irs::score_t boost,
                        irs::attribute_provider const* /*ctx*/) const final;
};

}  // namespace arangodb::iresearch
