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

#include "NearQuery.h"
#include "Basics/Common.h"
#include "Geo/GeoCover.h"
#include "Geo/GeoParams.h"
#include "Logger/Logger.h"

#include <geometry/s1angle.h>
#include <geometry/s2latlng.h>
#include <geometry/s2region.h>
#include <geometry/s2regioncoverer.h>
#include <geometry/s2regionintersection.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::geo;

NearQuery::NearQuery(NearQueryParams const& params)
    : _params(params),
      _centroid(S2LatLng::FromDegrees(params.centroid.latitude,
                                      params.centroid.longitude)
                    .ToPoint()) {
  /*S1Angle angle = S1Angle::Radians(params.maxDistance / kEarthRadiusInMeters);
  S2LatLng pos = S2LatLng::FromDegrees(params.latitude, params.longitude);
  _fullScanBounds = S2Cap::FromAxisAngle(pos.ToPoint(), angle);*/
  params.cover.configureS2RegionCoverer(&_coverer);

  reset();
}

void NearQuery::reset() {
  constexpr int level = NearQueryParams::queryBestLevel - 2;
  static_assert(0 < level && level < S2::kMaxCellLevel - 2, "");

  _boundDelta = S2::kAvgEdge.GetValue(level);
  TRI_ASSERT(_boundDelta > 0);
  _innerBound = std::max(0.0, _params.minDistance / kEarthRadiusInMeters);
  _outerBound = _innerBound + _boundDelta;
  _maxBounds = std::min(_params.maxDistance / kEarthRadiusInMeters, 1.0);
  TRI_ASSERT(_innerBound < _outerBound && _outerBound <= _maxBounds);
}

/// Call only once current intervals contain
/// no more results
std::vector<GeoCover::Interval> NearQuery::intervals() {
  if (_scannedCells.num_cells() > 0) {
    if (_numFoundLastInterval < 256) {
      _boundDelta *= 2;
    } else if (_numFoundLastInterval > 512) {
      _boundDelta /= 2;
    }
    _numFoundLastInterval = 0;
  }

  std::vector<S2CellId> cover;
  if (0.0 < _innerBound && _outerBound < _maxBounds) {
    std::vector<S2Region*> regions;
    S2Cap ib = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_innerBound));
    regions.push_back(new S2Cap(ib.Complement()));
    S2Cap ob = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_outerBound));
    regions.push_back(new S2Cap(ob));
    S2RegionIntersection ring(&regions);
    _coverer.GetCovering(ring, &cover);
  } else if (0.0 == _innerBound && _outerBound < _maxBounds) {
    // scan everything up till bounds fix floating point errors
    S2Cap ob = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_outerBound));
    _coverer.GetCovering(ob, &cover);
  } else if (0.0 < _innerBound && _outerBound >= _maxBounds) {
    // no need to add region to scan entire earth
    S2Cap ib = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_innerBound));
    _coverer.GetCovering(ib, &cover);
  } else {  // done or invalid bounds
    TRI_ASSERT(_innerBound == _outerBound == _maxBounds);
    return {};
  }

  TRI_ASSERT(!cover.empty());
  S2CellUnion coverUnion;
  coverUnion.Init(cover);
  S2CellUnion lookup;
  lookup.GetDifference(&coverUnion, &_scannedCells);

  std::vector<GeoCover::Interval> intervals;
  if (lookup.num_cells() > 0) {
    GeoCover::scanIntervals(_params.cover.worstIndexedLevel, lookup.cell_ids(),
                            intervals);
  } else {
    LOG_TOPIC(WARN, Logger::ENGINES) << "empty lookup cellunion";
    _boundDelta *= 2;  // seems weird
  }
  // TODO why do I need this ?
  /*for (int i = 0; i < lookup.num_cells(); i++) {
    if (region->MayIntersect(S2Cell(lookup.cell_id(i)))) {
      cover.push_back(cellId);
    }
  }*/

  _innerBound = _outerBound;
  _outerBound = std::min(_outerBound + _boundDelta, _maxBounds);

  auto it = _seen.begin();
  while (it != _seen.end()) {
    if (it->second < _innerBound) {
      it = _seen.erase(it);
    } else {
      it++;
    }
  }

  return intervals;
}

void NearQuery::reportFound(TRI_voc_rid_t const& rid,
                            geo::Coordinate const& center) {
  S2LatLng cords = S2LatLng::FromDegrees(center.latitude, center.longitude);
  double rad = _centroid.Angle(cords.ToPoint());
  if (_innerBound < rad || _maxBounds < rad ||
      (!_params.maxInclusive && rad == _maxBounds)) {
    return;
  }

  auto const& it = _seen.find(rid);
  if (it == _seen.end()) {
    _buffer.emplace(rid, rad);
    _seen.emplace(rid, rad);
    _numFoundLastInterval++;  // we have to estimate scan bounds
  } else {                    // deduplication
    TRI_ASSERT(it->second == rad);
    /*if (doc.distance / kEarthRadiusInMeters < _innerBound) {
      _seen.erase(it); // no need to store
    }*/
  }
}

bool NearQuery::NearQuery::done() const {
  TRI_ASSERT(_innerBound >= 0 && _innerBound <= _outerBound);
  TRI_ASSERT(_outerBound <= _maxBounds && _maxBounds <= 1.0);
  return _buffer.empty() && _innerBound == _outerBound == _maxBounds;
}
