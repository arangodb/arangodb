////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GEO_SHAPE_CONTAINER_H
#define ARANGOD_GEO_SHAPE_CONTAINER_H 1

#include <string>
#include <memory>

#include <s2/s2cell_id.h>

#include <velocypack/Slice.h>

#include "Basics/Result.h"
#include "Geo/Shapes.h"

class S2Region;
class S2RegionCoverer;
class S2LatLng;
class S2LatLngRect;
class S2Cap;
class S2Polyline;
class S2Polygon;

namespace arangodb {
namespace geo {

/// Thin wrapper around S2Region objects combined with
/// a type and helper methods to do intersect and contains
/// checks between all supported region types
class ShapeContainer final {
  ShapeContainer(ShapeContainer const&) = delete;

 public:
  enum class Type {
    EMPTY = 0,
    S2_POINT,
    S2_POLYLINE,
    S2_LATLNGRECT,
    S2_POLYGON,
    S2_MULTIPOINT,
    S2_MULTIPOLYLINE
  };

  ShapeContainer() noexcept : _data(nullptr), _type(Type::EMPTY) {}
  ShapeContainer(ShapeContainer&& other) noexcept;
  /*ShapeContainer(std::unique_ptr<S2Region>&& ptr, Type tt)
      : _data(ptr.release()), _type(tt) {}
  ShapeContainer(S2Region* ptr, Type tt) : _data(ptr), _type(tt) {}*/
  ~ShapeContainer();

 public:
  /// Parses a coordinate pair
  Result parseCoordinates(velocypack::Slice const& json, bool geoJson);

  void reset(std::unique_ptr<S2Region> ptr, Type tt) noexcept;
  void reset(S2Region* ptr, Type tt) noexcept;
  void resetCoordinates(double lat, double lon);

  Type type() const { return _type; }
  inline bool empty() const { return _type == Type::EMPTY; }

  bool isAreaType() const noexcept {
    return _type == Type::S2_POLYGON || _type == Type::S2_LATLNGRECT;
  }

  /// @brief centroid of this shape
  S2Point centroid() const noexcept;

  /// @brief generate a cell covering
  std::vector<S2CellId> covering(S2RegionCoverer*) const noexcept;

  /// @brief distance from center in meters
  double distanceFrom(S2Point const&) const noexcept;

  /// @brief may intersect the cell
  bool mayIntersect(S2CellId) const noexcept;

  /// @brief adjust query parameters (specifically max distance)
  void updateBounds(QueryParams& qp) const noexcept;

  /// contains this region the coordinate
  bool contains(S2Point const&) const;
  bool contains(S2Polyline const*) const;
  bool contains(S2LatLngRect const*) const;
  bool contains(S2Polygon const*) const;
  bool contains(ShapeContainer const*) const;

  /// intersects this region the coordinate
  bool intersects(Coordinate const*) const;
  bool intersects(S2Point const& p) const {
    return contains(p);  // same thing
  }
  bool intersects(S2Polyline const*) const;
  bool intersects(S2LatLngRect const*) const;
  bool intersects(S2Polygon const*) const;
  bool intersects(ShapeContainer const*) const;

  /// equals this region the coordinate
  bool equals(Coordinate const*) const;
  bool equals(Coordinate const&, Coordinate const&) const;
  bool equals(double lat1, double lon1) const;
  bool equals(S2Polyline const*) const;
  bool equals(S2Polyline const*, S2Polyline const*) const;
  bool equals(S2LatLngRect const*) const;
  bool equals(S2Polygon const*) const;
  bool equals(ShapeContainer const*) const;

  S2Region const* region() const;

 private:
  S2Region* _data;
  Type _type;
};
}  // namespace geo
}  // namespace arangodb

#endif
