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

#include "IResearch/Geo.h"
#include "IResearch/IResearchCommon.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Logger/LogMacros.h"

#include <s2/s2latlng.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb::iresearch {

template<Parsing p>
bool parseShape(velocypack::Slice vpack, geo::ShapeContainer& region,
                std::vector<S2LatLng>& cache, bool legacy,
                geo::coding::Options options, Encoder* encoder) {
  TRI_ASSERT(encoder == nullptr || encoder->length() == 0);
  Result r;
  if (vpack.isArray()) {
    r = geo::json::parseCoordinates<p != Parsing::FromIndex>(vpack, region,
                                                             /*geoJson=*/true,
                                                             options, encoder);
  } else if constexpr (p == Parsing::OnlyPoint) {
    auto parsePoint = [&] {
      S2LatLng latLng;
      r = geo::json::parsePoint(vpack, latLng);
      if (r.ok() && encoder != nullptr) {
        TRI_ASSERT(options != geo::coding::Options::kInvalid);
        TRI_ASSERT(encoder->avail() >= sizeof(uint8_t));
        // We store type, because parseCoordinates store it
        encoder->put8(0);  // In store to column we will remove it
        if (geo::coding::isOptionsS2(options)) {
          auto point = latLng.ToPoint();
          geo::encodePoint(*encoder, point);
          return point;
        } else {
          geo::encodeLatLng(*encoder, latLng, options);
        }
      } else if (r.ok() && options == geo::coding::Options::kS2LatLngInt) {
        geo::toLatLngInt(latLng);
      }
      return latLng.ToPoint();
    };
    if (r.ok()) {
      region.reset(parsePoint(), options);
    }
  } else {
    r = geo::json::parseRegion<p != Parsing::FromIndex>(
        vpack, region, cache, legacy, options, encoder);
  }
  if (p != Parsing::FromIndex && r.fail()) {
    LOG_TOPIC("4549c", DEBUG, TOPIC)
        << "Failed to parse value as GEO JSON or array of coordinates, error '"
        << r.errorMessage() << "'";
    return false;
  }
  return true;
}

template bool parseShape<Parsing::FromIndex>(velocypack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2LatLng>& cache,
                                             bool legacy,
                                             geo::coding::Options options,
                                             Encoder* encoder);
template bool parseShape<Parsing::OnlyPoint>(velocypack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2LatLng>& cache,
                                             bool legacy,
                                             geo::coding::Options options,
                                             Encoder* encoder);
template bool parseShape<Parsing::GeoJson>(velocypack::Slice slice,
                                           geo::ShapeContainer& shape,
                                           std::vector<S2LatLng>& cache,
                                           bool legacy,
                                           geo::coding::Options options,
                                           Encoder* encoder);

void toVelocyPack(velocypack::Builder& builder, S2LatLng point) {
  TRI_ASSERT(point.is_valid());
  // false because with false it's smaller
  // in general we want only doubles, but format requires it should be array
  // so we generate most smaller vpack array
  builder.openArray(false);
  builder.add(velocypack::Value{point.lng().degrees()});
  builder.add(velocypack::Value{point.lat().degrees()});
  builder.close();
  TRI_ASSERT(builder.slice().isArray());
  TRI_ASSERT(builder.slice().head() == 0x02);
}

}  // namespace arangodb::iresearch
