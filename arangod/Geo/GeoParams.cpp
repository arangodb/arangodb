////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "GeoParams.h"

#include <velocypack/velocypack-aliases.h>
#include <geometry/s2regioncoverer.h>

using namespace arangodb;
using namespace arangodb::geo;

void GeoIndexParams::configureS2RegionCoverer(S2RegionCoverer* coverer) const {
  // This is a soft limit, only levels are strict
  coverer->set_max_cells(maxCoverCells);
  coverer->set_min_level(minIndexedLevel);
  coverer->set_max_level(maxIndexedLevel);
}

