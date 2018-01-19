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

#include "GeoParams.h"
#include "Basics/Common.h"

#include <geometry/s1angle.h>
#include <geometry/s2cap.h>
#include <geometry/s2regioncoverer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::geo;

RegionCoverParams::RegionCoverParams()
    : maxNumCoverCells(20),
      worstIndexedLevel(
          S2::kAvgEdge.GetClosestLevel(2000 * 1000.0 / kEarthRadiusInMeters)),
      bestIndexedLevel(
          S2::kAvgEdge.GetClosestLevel(105.0 / kEarthRadiusInMeters)) {
  // optimize levels for buildings, points are converted without S2RegionCoverer
}

/// @brief read the options from a vpack slice
void RegionCoverParams::fromVelocyPack(VPackSlice const& params) {
  TRI_ASSERT(params.isObject());
  VPackSlice v;
  if ((v = params.get("maxNumCoverCells")).isInteger()) {
    maxNumCoverCells = v.getNumber<int>();
  }
  if ((v = params.get("worstIndexedLevel")).isInteger()) {
    worstIndexedLevel = v.getNumber<int>();
  }
  if ((v = params.get("bestIndexedLevel")).isInteger()) {
    bestIndexedLevel = v.getNumber<int>();
  }
}

/// @brief add the options to an opened vpack builder
void RegionCoverParams::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("maxNumCoverCells", VPackValue(maxNumCoverCells));
  builder.add("worstIndexedLevel", VPackValue(worstIndexedLevel));
  builder.add("bestIndexedLevel", VPackValue(bestIndexedLevel));
}

void RegionCoverParams::configureS2RegionCoverer(
    S2RegionCoverer* coverer) const {
  // This is a soft limit, only levels are strict
  coverer->set_max_cells(maxNumCoverCells);
  coverer->set_min_level(worstIndexedLevel);
  coverer->set_max_level(bestIndexedLevel);
}

double geo::QueryParams::minDistanceRad() const noexcept {
  return std::max(0.0, std::min(minDistance / kEarthRadiusInMeters, M_PI));
}

double geo::QueryParams::maxDistanceRad() const noexcept {
  return std::max(0.0, std::min(maxDistance / kEarthRadiusInMeters, M_PI));
}
