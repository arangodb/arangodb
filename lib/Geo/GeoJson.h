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
/// @author Heiko Kernbach
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <s2/s2point.h>

#include <velocypack/Slice.h>

#include "Basics/Result.h"
#include "Geo/Coding.h"

class S2LatLng;
class S2Loop;
class S2Polyline;
class S2Polygon;
class Encoder;

namespace arangodb::geo {

class S2MultiPointRegion;
class S2MultiPolylineRegion;

class ShapeContainer;

/// Simple GeoJson parser should be more or less forgiving
/// and complies with most of https://tools.ietf.org/html/rfc7946
namespace json {
namespace fields {

inline constexpr std::string_view kType = "type";
inline constexpr std::string_view kCoordinates = "coordinates";

}  // namespace fields

enum class Type : uint8_t {
  UNKNOWN = 0,
  POINT,
  LINESTRING,
  POLYGON,
  MULTI_POINT,
  MULTI_LINESTRING,
  MULTI_POLYGON,
  // TODO(MBkkt) we don't support GeometryCollection so it's a having it in Type
  //  a very little slowdown parsing, but provide better error.
  //
  //  Also I think we can support it, because for all operations with geometry
  //  we use S2ShapeIndex and it support construction from collection of
  //  S2Region so I think it's nice feature.
  GEOMETRY_COLLECTION,
};

/// About without validation, it should be used when we parse data from our
/// indexes. Because this checks very expensive and we don't want to make it in
/// every query. Instead of this we store in our indexes only valid data.
/// @note
/// In other words I make assumption if Validation is false all parsing
/// is successful.
/// @note
/// In Maintainer mode under x86 linux Validation is false,
/// use same code as Validation is true. But make TRI_ASSERT that result is ok.
/// @note
/// For all functions without Validation parameter it's implicitly equal true.
/// @note
/// For all functions without legacy parameter it's implicitly equal false.
/// @note
/// For all functions without geoJson parameter it's implicitly equal true.

/// @brief Expects an GeoJson type:
///
/// {
///   "type": "..."
/// }
/// type is case-insensitive
Type type(velocypack::Slice vpack) noexcept;

/// @brief Expects an GeoJson Point:
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.2
/// {
///   "type": "Point",
///   "coordinates": [lon, lat]
Result parsePoint(velocypack::Slice vpack, S2LatLng& region);

/// @brief Expects an GeoJson MultiPoint:
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.3
/// {
///   "type": "MultiPoint",
///   "coordinates": [
///     [lon0, lat0], [lon1, lat1], ...
Result parseMultiPoint(velocypack::Slice vpack, S2MultiPointRegion& region);

/// @brief Expects an GeoJson LineString:
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.4
/// {
///   "type": "LineString",
///   "coordinates": [
///     [lon0, lat0], [lon1, lat1], ...
Result parseLinestring(velocypack::Slice vpack, S2Polyline& region);

/// @brief Expects an GeoJson MultiLineString:
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.5
/// {
///   "type": "MultiLineString",
///   "coordinates": [
///     [[lon0, lat0], [lon1, lat1], ...], ...
Result parseMultiLinestring(velocypack::Slice vpack,
                            S2MultiPolylineRegion& region);

/// @brief Expects an GeoJson Polygon:
/// Each loop should be closed, so should contains at least four points
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.6
/// {
///   "type": "Polygon",
///   "coordinates": [
///     [[lon0, lat0], [lon1, lat1], [lon2, lat2], [lon3, lat3], ...], ...
Result parsePolygon(velocypack::Slice vpack, S2Polygon& region);

/// @brief Expects an GeoJson MultiPolygon:
/// Each loop should be closed, so should contains at least four points
///
/// https://www.rfc-editor.org/rfc/rfc7946#section-3.1.7
/// {
///   "type": "MultiPolygon",
///   "coordinates": [
///     [[lon0, lat0], [lon1, lat1], [lon2, lat2], [lon3, lat3], ...], ...
Result parseMultiPolygon(velocypack::Slice vpack, S2Polygon& region);

/// @brief Convenience function to build a region from a GeoJson type.
Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   bool legacy);

template<bool Valid = true>
Result parseRegion(velocypack::Slice vpack, ShapeContainer& region,
                   std::vector<S2LatLng>& cache, bool legacy,
                   coding::Options options = coding::Options::kInvalid,
                   Encoder* encoder = nullptr);

template<bool Valid = true>
Result parseCoordinates(velocypack::Slice vpack, ShapeContainer& region,
                        bool geoJson,
                        coding::Options options = coding::Options::kInvalid,
                        Encoder* encoder = nullptr);

/// @brief Parse a loop (LinearRing)
///
/// Note that at the moment we do not enforce that the final coordinate must
/// match the first, as is required of a proper LinearRing in GeoJSON format.
///
/// @param vpack  an array of arrays with 2 elements each, representing the
///               points of the polygon
/// @param loop  output parameter to hold the parsed loop
/// @param geoJson  If true, the points are assumed to be [lon, lat],
///                 otherwise [lat, lon]
[[deprecated(
    "Subject to removal"
    " when deprecated IS_IN_POLYGON function is removed.")]] Result
parseLoop(velocypack::Slice vpack, S2Loop& loop, bool geoJson);

}  // namespace json
}  // namespace arangodb::geo
