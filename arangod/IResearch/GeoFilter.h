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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "search/filter.hpp"
#include "search/search_range.hpp"
#include "utils/type_limits.hpp"

#include "Assertions/Assert.h"
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

  bool operator==(GeoFilterOptions const& rhs) const noexcept {
    return type == rhs.type && shape.equals(rhs.shape);
  }

  GeoFilterType type{GeoFilterType::INTERSECTS};
  geo::ShapeContainer shape;
};

class GeoFilter final : public irs::FilterWithField<GeoFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoFilter";
  }

  prepared::ptr prepare(irs::PrepareContext const& ctx) const final;
};

class GeoDistanceFilter;

struct GeoDistanceFilterOptions : GeoFilterOptionsBase {
  using filter_type = GeoDistanceFilter;

  bool operator==(GeoDistanceFilterOptions const& rhs) const noexcept {
    return origin == rhs.origin && range == rhs.range;
  }

  S2Point origin;
  irs::search_range<double> range;
};

class GeoDistanceFilter final
    : public irs::FilterWithField<GeoDistanceFilterOptions> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "arangodb::iresearch::GeoDistanceFilter";
  }

  prepared::ptr prepare(irs::PrepareContext const& ctx) const final;
};

}  // namespace arangodb::iresearch
