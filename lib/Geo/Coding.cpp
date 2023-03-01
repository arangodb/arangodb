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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#include "Geo/Coding.h"

namespace arangodb::geo {

void ensureLittleEndian() {}

void toLatLngInt(S2LatLng& /*latLng*/) noexcept {}

void encodeLatLng(Encoder& /*encoder*/, S2LatLng& /*latLng*/,
                  coding::Options /*options*/) noexcept {}

void encodePoint(Encoder& /*encoder*/, S2Point const& /*point*/) noexcept {}

void toLatLngInt(std::span<S2LatLng> /*vertices*/) noexcept {}

void encodeVertices(Encoder& /*encoder*/,
                    std::span<S2Point const> /*vertices*/) {}

void encodeVertices(Encoder& /*encoder*/, std::span<S2LatLng> /*vertices*/,
                    coding::Options /*options*/) {}

bool decodeVertices(Decoder& /*decoder*/, std::span<S2Point> /*vertices*/,
                    uint8_t /*tag*/) {
  return false;
}

bool decodePoint(Decoder& /*decoder*/, S2Point& /*point*/, uint8_t* /*tag*/) {
  return false;
}

bool decodePoint(Decoder& /*decoder*/, S2Point& /*point*/, uint8_t /*tag*/) {
  return false;
}

void encodePolyline(Encoder& /*encoder*/, S2Polyline const& /*polyline*/,
                    coding::Options /*options*/) {}

bool decodePolyline(Decoder& /*decoder*/, S2Polyline& /*polyline*/,
                    uint8_t /*tag*/, std::vector<S2Point>& /*cache*/) {
  return false;
}

void encodePolygon(Encoder& /*encoder*/, S2Polygon const& /*polygon*/,
                   coding::Options /*options*/) {}

bool decodePolygon(Decoder& /*decoder*/, S2Polygon& /*polygon*/,
                   uint8_t /*tag*/, std::vector<S2Point>& /*cache*/) {
  return false;
}

void encodePolylines(Encoder& /*encoder*/,
                     std::span<S2Polyline const> /*polylines*/,
                     coding::Options /*options*/) {}

bool decodePolylines(Decoder& /*decoder*/, std::vector<S2Polyline>& polylines,
                     uint8_t /*tag*/, std::vector<S2Point>& /*cache*/) {
  return false;
}

}  // namespace arangodb::geo
