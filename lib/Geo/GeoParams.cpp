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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Geo/GeoParams.h"

#include <s2/s2earth.h>
#include <s2/s2metrics.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

namespace arangodb::geo {

RegionCoverParams::RegionCoverParams()
    : maxNumCoverCells(kMaxNumCoverCellsDefault),
      worstIndexedLevel(
          S2::kAvgEdge.GetClosestLevel(S2Earth::KmToRadians(600))),
      bestIndexedLevel(
          S2::kAvgEdge.GetClosestLevel(S2Earth::MetersToRadians(100.0))) {
  // optimize levels for buildings, points are converted without S2RegionCoverer
}

/// @brief read the options from a velocypack::Slice
void RegionCoverParams::fromVelocyPack(velocypack::Slice slice) {
  TRI_ASSERT(slice.isObject());
  velocypack::Slice v;
  if ((v = slice.get("maxNumCoverCells")).isNumber<int>()) {
    maxNumCoverCells = v.getNumber<int>();
  }
  if ((v = slice.get("worstIndexedLevel")).isNumber<int>()) {
    worstIndexedLevel = v.getNumber<int>();
  }
  if ((v = slice.get("bestIndexedLevel")).isNumber<int>()) {
    bestIndexedLevel = v.getNumber<int>();
  }
}

/// @brief add the options to an opened velocypack:: builder
void RegionCoverParams::toVelocyPack(velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("maxNumCoverCells", velocypack::Value(maxNumCoverCells));
  builder.add("worstIndexedLevel", velocypack::Value(worstIndexedLevel));
  builder.add("bestIndexedLevel", velocypack::Value(bestIndexedLevel));
}

S2RegionCoverer::Options RegionCoverParams::regionCovererOpts() const {
  S2RegionCoverer::Options opts;
  opts.set_max_cells(maxNumCoverCells);   // This is a soft limit
  opts.set_min_level(worstIndexedLevel);  // Levels are a strict limit
  opts.set_max_level(bestIndexedLevel);
  return opts;
}

double QueryParams::minDistanceRad() const noexcept {
  return metersToRadians(minDistance);
}

double QueryParams::maxDistanceRad() const noexcept {
  return metersToRadians(maxDistance);
}

std::string QueryParams::toString() const {
  auto t = [](bool x) -> std::string_view { return x ? "true" : "false"; };
  return absl::StrCat(  // clang-format off
     "minDistance: ", minDistance,
    " incl: ", t(minInclusive),
    " maxDistance: ", maxDistance,
    " incl: ", t(maxInclusive),
    " distanceRestricted: ", t(distanceRestricted),
    " sorted: ", t(sorted),
    " ascending: ", t(ascending),
    " origin: ", origin.lng().degrees(), " , ", origin.lat().degrees(),
    " pointsOnly: ", t(pointsOnly),
    " limit: ", limit,
    " filterType: ", static_cast<int>(filterType),
    " filterShape: ", static_cast<int>(filterShape.type())
  );  // clang-format on
}

}  // namespace arangodb::geo
