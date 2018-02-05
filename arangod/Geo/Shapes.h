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

#ifndef ARANGOD_GEO_SHAPES_H
#define ARANGOD_GEO_SHAPES_H 1

#include "Basics/Result.h"

#include <s2/s2cell_id.h>
#include <velocypack/Slice.h>
#include <string>

class S2Region;
class S2RegionCoverer;
class S2LatLng;
class S2LatLngRect;
class S2Cap;
class S2Polyline;
class S2Polygon;

namespace arangodb {
namespace geo {
struct QueryParams;

/// coordinate point on the sphere in DEGREES
struct Coordinate {
 public:
  Coordinate(double lat, double lon) noexcept : latitude(lat), longitude(lon) {}
  Coordinate(Coordinate&& c) noexcept : latitude(c.latitude), longitude(c.longitude) {}
  Coordinate(Coordinate const& c) noexcept
      : latitude(c.latitude), longitude(c.longitude) {}

  static Coordinate fromLatLng(S2LatLng const&) noexcept;
  static inline Coordinate Invalid() noexcept { return Coordinate(91, 181); }

 public:
  Coordinate& operator=(Coordinate const& other) noexcept {
    latitude = other.latitude;
    longitude = other.longitude;
    return *this;
  }

  bool operator==(Coordinate const& other) const {
    return latitude == other.latitude && longitude == other.longitude;
  }

  bool operator!=(Coordinate const& other) const {
    return latitude != other.latitude || longitude != other.longitude;
  }

  bool isValid() const {
    return std::abs(latitude) <= 90.0 && std::abs(longitude) <= 180.0;
  }

  std::string toString() const {
    return "(lat: " + std::to_string(latitude) + ", lon: " +
           std::to_string(longitude) + ")";
  }

 public:
  double latitude;   // in degrees
  double longitude;  // in degrees
};

/// Circle on a sphere
/*struct Cap {
  Cap(geo::Coordinate cc, double r) : center(cc), radius(r) {}
public:
  Coordinate center;
  double radius; // in rad
};*/

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
    S2_POLYGON,
    S2_MULTIPOINT,
    S2_MULTIPOLYLINE
  };

  ShapeContainer() : _data(nullptr), _type(Type::EMPTY) {}
  ShapeContainer(ShapeContainer&& other) noexcept;
  /*ShapeContainer(std::unique_ptr<S2Region>&& ptr, Type tt)
      : _data(ptr.release()), _type(tt) {}
  ShapeContainer(S2Region* ptr, Type tt) : _data(ptr), _type(tt) {}*/
  ~ShapeContainer();

 public:
  /// Parses a coordinate pair
  Result parseCoordinates(velocypack::Slice const& json, bool geoJson);

  void reset(std::unique_ptr<S2Region>&& ptr, Type tt) noexcept;
  void reset(S2Region* ptr, Type tt) noexcept;
  void resetCoordinates(double lat, double lon);

  Type type() const { return _type; }
  inline bool empty() const { return _type == Type::EMPTY; }

  bool isAreaType() const noexcept {
    return _type == Type::S2_POLYGON;
  }

  /// @brief centroid of this shape
  geo::Coordinate centroid() const noexcept;

  /// @brief generate a cell covering
  std::vector<S2CellId> covering(S2RegionCoverer*) const noexcept;

  /// @brief distance from center in meters
  double distanceFrom(geo::Coordinate const&) const noexcept;

  /// @brief may intersect the cell
  bool mayIntersect(S2CellId) const noexcept;

  /// @brief update query parameters
  void updateBounds(QueryParams& qp) const noexcept;

  /// contains this region the coordinate
  bool contains(Coordinate const*) const;
  bool contains(S2Polyline const*) const;
  bool contains(S2Polygon const*) const;
  bool contains(ShapeContainer const*) const;

  bool intersects(Coordinate const*) const;
  bool intersects(S2Polyline const*) const;
  bool intersects(S2Polygon const*) const;
  bool intersects(ShapeContainer const*) const;

 private:
  S2Region* _data;
  Type _type;
};
}  // namespace geo
}  // namespace arangodb

#endif
