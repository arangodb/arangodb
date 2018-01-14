////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Slice.h>
#include <string>

#include <geometry/s2cellid.h>

class S2Region;
class S2LatLng;
class S2LatLngRect;
class S2Cap;
class S2Polyline;
class S2Polygon;

namespace arangodb {
namespace geo {

/// coordinate point on the sphere in DEGREES
struct Coordinate {
 public:
  Coordinate(double lat, double lon) : latitude(lat), longitude(lon) {}
  Coordinate(Coordinate&& c) : latitude(c.latitude), longitude(c.longitude) {}
  Coordinate(Coordinate const& c)
      : latitude(c.latitude), longitude(c.longitude) {}

  static Coordinate fromLatLng(S2LatLng const&);
  static inline Coordinate Invalid() { return Coordinate(-1, -1); }

 public:
  Coordinate& operator=(Coordinate const& other) {
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

  std::string toString() const {
    return "(lat: " + std::to_string(latitude) + ", lon: " +
           std::to_string(longitude) + ")";
  }

 public:
  double latitude;   // in degrees
  double longitude;  // in degrees
};

/// Thin wrapper around S2Region combined with
/// a type and helper methods for all special cases
class ShapeContainer final {
  ShapeContainer(ShapeContainer const&) = delete;

 public:
  enum class Type {
    UNDEFINED = 0,
    S2_POINT,
    S2_LATLNGRECT,
    S2_CAP,
    S2_POLYLINE,
    S2_POLYGON,
    S2_MULTIPOINT,
    S2_MULTIPOLYLINE
  };

  ShapeContainer() : _data(nullptr), _type(Type::UNDEFINED) {}
  ShapeContainer(ShapeContainer&& other);
  /*ShapeContainer(std::unique_ptr<S2Region>&& ptr, Type tt)
      : _data(ptr.release()), _type(tt) {}
  ShapeContainer(S2Region* ptr, Type tt) : _data(ptr), _type(tt) {}*/
  ~ShapeContainer();

 public:
  /// Parses a coordinate pair
  Result parseCoordinates(velocypack::Slice const& json, bool geoJson);

  void reset(std::unique_ptr<S2Region>&& ptr, Type tt);
  void reset(S2Region* ptr, Type tt);

  Type type() const { return _type; }
  bool isAreaType() const {
    return _type == Type::S2_POLYGON || _type == Type::S2_CAP ||
           _type == Type::S2_LATLNGRECT;
  }

  /// @brief is an empty shape (can be expensive)
  bool isAreaEmpty() const;
  /// @brief centroid of this shape
  geo::Coordinate centroid() const;
  /// @brief distance from center in meters
  double distanceFromCentroid(geo::Coordinate const&);
  bool mayIntersect(S2CellId) const;
  double capBoundRadius() const;

  bool contains(Coordinate const*) const;
  // bool contains(S2LatLngRect const&) const;
  // bool contains(S2Cap const&) const;
  bool contains(S2Polyline const*) const;
  bool contains(S2Polygon const*) const;
  bool contains(ShapeContainer const*) const;

  bool intersects(Coordinate const*) const;
  // bool intersects(S2LatLngRect const&) const;
  // bool intersects(S2Cap const&) const;
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
