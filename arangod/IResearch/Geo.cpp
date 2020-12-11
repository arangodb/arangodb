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

#include "Geo.h"

#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Logger/LogMacros.h"
#include "IResearch/IResearchCommon.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace arangodb {
namespace iresearch {

bool parseShape(VPackSlice slice, geo::ShapeContainer& shape, bool onlyPoint) {
  Result res;
  if (slice.isObject()) {
    if (onlyPoint) {
      S2LatLng ll;
      res = geo::geojson::parsePoint(slice, ll);

      if (res.ok()) {
        shape.resetCoordinates(ll);
      }
    } else {
      res = geo::geojson::parseRegion(slice, shape);
    }
  } else if (slice.isArray() && slice.length() >= 2) {
    res = shape.parseCoordinates(slice, /*geoJson*/ true);
  } else {
    LOG_TOPIC("4449c", WARN, arangodb::iresearch::TOPIC)
      << "Geo JSON or array of coordinates expected, got '"
      << slice.typeName() << "'";

    return false;
  }

  if (res.fail()) {
    LOG_TOPIC("4549c", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO JSON or array of coordinates, error '"
      << res.errorMessage() << "'";

    return false;
  }

  return true;
}

bool parsePoint(VPackSlice latSlice, VPackSlice lngSlice, S2LatLng& out) {
  if (!latSlice.isNumber() || !lngSlice.isNumber()) {
    LOG_TOPIC("4579a", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO POINT, error 'Invalid latitude/longitude pair type'.";

    return false;
  }

  double_t lat, lng;
  try {
    lat = latSlice.getNumber<double_t>();
    lng = lngSlice.getNumber<double_t>();
  } catch (...) {
    LOG_TOPIC("4579c", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO POINT, error 'Failed to parse latitude/longitude pair' as double.";
    return false;
  }

  // FIXME Normalized()?
  out = S2LatLng::FromDegrees(lat, lng);

  if (!out.is_valid()) {
    LOG_TOPIC("4279c", WARN, arangodb::iresearch::TOPIC)
      << "Failed to parse value as GEO POINT, error 'Invalid latitude/longitude pair'.";
    return false;
  }

  return true;
}

void toVelocyPack(velocypack::Builder& builder, S2LatLng const& point) {
  builder.openArray();
  builder.add(VPackValue(point.lng().degrees()));
  builder.add(VPackValue(point.lat().degrees()));
  builder.close();
}

} // iresearch
} // arangodb
