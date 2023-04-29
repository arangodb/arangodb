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

#include "GeoIndex/Covering.h"

#include <s2/s2region.h>
#include <s2/s2region_coverer.h>
#include <s2/s2region_intersection.h>

#include "Basics/Common.h"
#include "Geo/GeoParams.h"
#include "Geo/Utils.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

namespace arangodb::geo_index {

CoveringUtils::CoveringUtils(geo::QueryParams&& qp) noexcept
    : _params(std::move(qp)),
      _allIntervalsCovered(false),
      _numScans(0),
      _coverer(_params.cover.regionCovererOpts()) {
  reset();
  // Level 15 == 474.142m (start with 15 essentially)
}

CoveringUtils::~CoveringUtils() {
  // LOG_TOPIC("5d2f2", ERR, Logger::FIXME) << "Scans: " << _numScans << "
  // Found: " << _found << " Rejections: " << _rejection;
}

void CoveringUtils::reset() {
  _seenDocs.clear();
  _buffer.clear();
  _allIntervalsCovered = false;
  _numScans = 0;
}

std::vector<geo::Interval> CoveringUtils::intervals() {
  TRI_ASSERT(_params.filterType != geo::FilterType::NONE);
  TRI_ASSERT(!hasNext());
  TRI_ASSERT(!isDone());

  std::vector<geo::Interval> intervals;
  auto cover = _params.filterShape.covering(_coverer);
  geo::utils::scanIntervals(_params, cover, intervals);
  _allIntervalsCovered = true;
  return intervals;
}

void CoveringUtils::reportFound(LocalDocumentId lid, S2Point const& center) {
  if (!_params.pointsOnly) {
    auto result = _seenDocs.insert(lid.id());
    if (!result.second) {
      ++_rejection;
      return;  // ignore repeated documents
    }
  }

  // possibly expensive point rejection, but saves parsing of document
  if (isFilterContains()) {
    TRI_ASSERT(!_params.filterShape.empty());
    if (!_params.filterShape.contains(center)) {
      ++_rejection;
      return;
    }
  }
  ++_found;
  _buffer.emplace_back(lid);
}

}  // namespace arangodb::geo_index
