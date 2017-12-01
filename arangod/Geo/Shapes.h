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

class S2Region;
class S2LatLngRect;
class S2Cap;
class S2Polyline;
class S2Polygon;

namespace arangodb {
namespace geo {

/// coordinate point on the sphere
struct Coordinate {
  Coordinate(double lat, double lon) : latitude(lat), longitude(lon) {}
  double latitude;   // in degrees
  double longitude;  // in degrees

  std::string toString() const {
    return "(lat: " + std::to_string(latitude) + ", lon: " +
           std::to_string(longitude) + ")";
  }
};

/// Thin wrapper around S2Region combined with
/// a type and helper methods for all special cases
class ShapeContainer final {
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
  ShapeContainer(std::unique_ptr<S2Region>&& ptr, Type tt)
      : _data(ptr.release()), _type(tt) {}
  ShapeContainer(S2Region* ptr, Type tt) : _data(ptr), _type(tt) {}
  ~ShapeContainer();
  
  /// Parses a coordinate pair
  Result parseCoordinates(velocypack::Slice const& json, bool geoJson);
  Result parseGeoJson(velocypack::Slice const& json);

 public:
  void reset(std::unique_ptr<S2Region>&& ptr, Type tt);
  void reset(S2Region* ptr, Type tt);
  
  Type type() const { return _type; }
  bool isAreaType() const {
    return _type == Type::S2_POLYGON || _type == Type::S2_CAP || _type == Type::S2_LATLNGRECT;
  }
  
  bool isAreaEmpty() const;

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
