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

NearQuery::NearQuery(NearQueryParams const& qp)
    : _params(qp),
      _centroid(S2LatLng::FromDegrees(qp.centroid.latitude,
                                      qp.centroid.longitude).ToPoint()),
      _maxBounds(std::min(qp.maxDistance / kEarthRadiusInMeters, M_PI)) {

  qp.cover.configureS2RegionCoverer(&_coverer);
  reset();
}

void NearQuery::reset() {
  if (!_seen.empty() || !_buffer.empty()) {
    _seen.clear();
    GeoDocumentsQueue emp;
    _buffer.swap(emp);
  }
  
  if (_boundDelta <= 0) { // do not reset everytime
    int level = std::max(1,  _params.cover.bestIndexedLevel - 1);
    level = std::min(level, S2::kMaxCellLevel - 4);
    _boundDelta = S2::kAvgEdge.GetValue(level);  // in radians ?
    TRI_ASSERT(_boundDelta * kEarthRadiusInMeters > 250);
  }

  _lastInnerBound = 0.0;  // used for fast out-of-bounds rejections
  _innerBound = std::max(0.0, _params.minDistance / kEarthRadiusInMeters);
  _outerBound = _innerBound + _boundDelta;
  TRI_ASSERT(_innerBound < _outerBound && _outerBound <= _maxBounds);
  _numFoundLastInterval = 0;
}

std::vector<GeoCover::Interval> NearQuery::intervals() {
  // we already scanned the entire planet, if this fails
  TRI_ASSERT(_innerBound != _outerBound && _innerBound != _maxBounds);

  if (_lastInnerBound > 0) {
    if (_numFoundLastInterval == 0) {
      _boundDelta *= 4;
    } else if (_numFoundLastInterval < 256) {
      _boundDelta *= 2;
    } else if (_numFoundLastInterval > 512) {
      _boundDelta /= 2;
    }
    _numFoundLastInterval = 0;
  }
  //LOG_TOPIC(ERR, Logger::FIXME) << "delta: " << _boundDelta;

  std::vector<S2CellId> cover;
  if (0.0 < _innerBound && _outerBound < _maxBounds) {
    // create a search ring
    std::vector<S2Region*> regions;
    S2Cap ib = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_innerBound));
    regions.push_back(new S2Cap(ib.Complement()));
    S2Cap ob = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_outerBound));
    regions.push_back(new S2Cap(ob));
    S2RegionIntersection ring(&regions);
    _coverer.GetCovering(ring, &cover);
  } else if (0.0 == _innerBound && _outerBound < _maxBounds) {
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

  std::vector<GeoCover::Interval> intervals;
  if (!cover.empty()) {  // not sure if this can ever happen
    if (_scannedCells.num_cells() == 0) {
      GeoCover::scanIntervals(_params.cover.worstIndexedLevel, cover,
                              intervals);
      _scannedCells.Add(cover);
    } else {
      S2CellUnion coverUnion;
      coverUnion.Init(cover);
      S2CellUnion lookup;
      lookup.GetDifference(&coverUnion, &_scannedCells);

      if (lookup.num_cells() > 0) {
        cover = lookup.cell_ids();
        GeoCover::scanIntervals(_params.cover.worstIndexedLevel, cover,
                                intervals);
        TRI_ASSERT(!cover.empty());
        _scannedCells.Add(cover);
      } /* else {
         LOG_TOPIC(WARN, Logger::ENGINES) << "empty lookup cellunion"
         << "inner " << _innerBound << " outer " << _outerBound;
       }*/
    }
  }

  // TODO why do I need this ?
  /*for (int i = 0; i < lookup.num_cells(); i++) {
    if (region->MayIntersect(S2Cell(lookup.cell_id(i)))) {
      cover.push_back(cellId);
    }
  }*/

  _lastInnerBound = _innerBound;
  _innerBound = _outerBound;
  _outerBound = std::min(_outerBound + _boundDelta, _maxBounds);

  // prune the _seen list revision IDs
  auto it = _seen.begin();
  while (it != _seen.end()) {
    if (it->second < _lastInnerBound) {
      it = _seen.erase(it);
    } else {
      it++;
    }
  }
  return intervals;
}

void NearQuery::reportFound(TRI_voc_rid_t rid, geo::Coordinate const& center) {
  S2LatLng cords = S2LatLng::FromDegrees(center.latitude, center.longitude);
  double rad = _centroid.Angle(cords.ToPoint());
  if (rad < _lastInnerBound || _maxBounds < rad ||
      (!_params.maxInclusive && rad == _maxBounds)) {
    return;
  }

  auto const& it = _seen.find(rid);
  if (it == _seen.end()) {
    _buffer.emplace(rid, rad);
    _seen.emplace(rid, rad);
    _numFoundLastInterval++;        // we have to estimate scan bounds
  } else {                          // deduplication
    TRI_ASSERT(it->second == rad);  // should never change
  }
}

void NearQuery::estimateDensity(geo::Coordinate const& found) {
  S2LatLng cords = S2LatLng::FromDegrees(found.latitude, found.longitude);
  _boundDelta = _centroid.Angle(cords.ToPoint()) * 2;
  LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "Estimating density with "
    << _boundDelta * kEarthRadiusInMeters << "m";
}

bool NearQuery::NearQuery::isDone() const {
  TRI_ASSERT(_innerBound >= 0 && _innerBound <= _outerBound);
  TRI_ASSERT(_outerBound <= _maxBounds && _maxBounds <= M_PI);
  return _buffer.empty() && _innerBound == _outerBound &&
         _outerBound == _maxBounds;
}

void NearQuery::invalidate() {
  _innerBound = _maxBounds;
  _outerBound = _maxBounds;
}
