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
#include "Basics/voc-errors.h"
#include "Geo/GeoJsonParser.h"
#include "Geo/GeoParams.h"
#include "Logger/Logger.h"

#include <s2/s2metrics.h>
#include <s2/s2cap.h>
#include <s2/s2cell.h>
#include <s2/s2latlng_rect.h>
#include <s2/s2multipoint_region.h>
#include <s2/s2multipolyline.h>
#include <s2/s2point_region.h>
#include <s2/s2polygon.h>
#include <s2/s2region.h>
#include <s2/s2region_coverer.h>

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::geo;

Coordinate Coordinate::fromLatLng(S2LatLng const& ll) noexcept {
  return Coordinate(ll.lat().degrees(), ll.lng().degrees());
}


