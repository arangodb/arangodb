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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <vector>

#include "Basics/Result.h"

#include <s2/s2point.h>
#include <s2/s2cell_id.h>

class S2Region;
class S2Polyline;
class S2LatLngRect;
class S2Polygon;
class S2RegionCoverer;

namespace arangodb::geo {

class Ellipsoid;
struct QueryParams;

/// Thin wrapper around S2Region objects combined with
/// a type and helper methods to do intersect and contains
/// checks between all supported region types
class ShapeContainer final {
 public:
  enum class Type : uint8_t {
    EMPTY = 0,
    S2_POINT,
    S2_POLYLINE,
    S2_LAT_LNG_RECT,  // only used in legacy code but kept for backwards
                      // compatibility of the enum numerical values
    S2_POLYGON,
    S2_POINTS,
    S2_POLYLINES,
  };

  bool empty() const noexcept { return _type == Type::EMPTY; }

  bool isAreaType() const noexcept {
    return _type == Type::S2_POLYGON || _type == Type::S2_LAT_LNG_RECT;
  }

  void updateBounds(QueryParams& qp) const noexcept;

  S2Point centroid() const noexcept;

  bool contains(S2Point const& other) const;
  bool contains(ShapeContainer const& other) const;

  bool intersects(ShapeContainer const& other) const;

  bool equals(ShapeContainer const& other) const;

  double distanceFromCentroid(S2Point const& other) const noexcept;

  double distanceFromCentroid(S2Point const& other,
                              Ellipsoid const& e) const noexcept;

  double area(Ellipsoid const& e) const;

  std::vector<S2CellId> covering(S2RegionCoverer& coverer) const;

  S2Region const* region() const noexcept { return _data.get(); }
  Type type() const noexcept { return _type; }

  void reset(std::unique_ptr<S2Region> region, Type type) noexcept;
  void reset(S2Point point);

  /// TODO(MBkkt) Needs to implement
  /// The main idea is store binary representation of S2Region in the indexes
  /// void S2Region::Encode(Encoder* const encoder) const;
  /// bool S2Region::Decode(Decoder* const decoder);
  ///
  /// So we don't need to do all validation
  ///  (it's implemented for GeoJson too but with more runtime conditions)
  /// but we also don't need to do transformation from user GeoJson to S2Region
  ///  (it's not possible with GeoJson)
  ///
  /// It also have cons, we cannot use stored data for materialization
  /// Maybe we can if user specify something like normalize output
  /// But I'm not sure will it better than read from collection or not

 private:
  std::unique_ptr<S2Region> _data;
  Type _type{Type::EMPTY};
};

}  // namespace arangodb::geo
