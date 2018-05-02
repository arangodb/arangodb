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

#include "Shapes.h"

#include <s2/s2latlng.h>

using namespace arangodb;
using namespace arangodb::geo;

Coordinate Coordinate::fromLatLng(S2LatLng const& ll) noexcept {
  return Coordinate(ll.lat().degrees(), ll.lng().degrees());
}

S2Point Coordinate::toPoint() const noexcept {
  return S2LatLng::FromDegrees(latitude, longitude).ToPoint();
}
