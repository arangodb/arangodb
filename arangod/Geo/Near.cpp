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

#include "Near.h"
#include "Basics/Common.h"
#include "Geo/GeoParams.h"
#include "Geo/GeoUtils.h"
#include "Logger/Logger.h"

#include <geometry/s2latlng.h>
#include <geometry/s2region.h>
#include <geometry/s2regioncoverer.h>
#include <geometry/s2regionintersection.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::geo;

NearUtils::NearUtils(QueryParams&& qp)
  : _params(std::move(qp)),
      _centroid(
          S2LatLng::FromDegrees(qp.centroid.latitude, qp.centroid.longitude)
              .ToPoint()),
      _maxBounds(qp.maxDistanceRad()) {
  qp.cover.configureS2RegionCoverer(&_coverer);
  reset();
  TRI_ASSERT(_params.sorted && _params.ascending);
        
  LOG_TOPIC(ERR, Logger::FIXME)
      << "--------------------------------------------------------";
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] centroid target: "
                                 << _params.centroid.toString();
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] minBounds: " << (size_t)(_params.minDistance * kEarthRadiusInMeters)
        << "  maxBounds:" << (size_t)(_maxBounds * kEarthRadiusInMeters);//*/
  TRI_ASSERT(_maxBounds > 0 && _maxBounds <= M_PI);
}

void NearUtils::reset() {
  if (!_seen.empty() || !_buffer.empty()) {
    _seen.clear();
    while (!_buffer.empty()) {
      _buffer.pop();
    }
  }

  if (_boundDelta <= 0) {  // do not reset everytime
    int level = std::max(1, _params.cover.bestIndexedLevel - 2);
    // Level 15 == 474.142m
    level = std::min(level, S2::kMaxDiag.GetClosestLevel(500 / kEarthRadiusInMeters));
    _boundDelta = S2::kMaxDiag.GetValue(level);  // in radians
    TRI_ASSERT(_boundDelta * kEarthRadiusInMeters >= 450);
  }
  TRI_ASSERT(_boundDelta > 0);

  // this initial interval is never used like that, see intervals()
  _innerBound = 0;
  _outerBound = std::max(0.0, _params.minDistance / kEarthRadiusInMeters);
  _statsFoundLastInterval = 0;
  TRI_ASSERT(_innerBound <= _outerBound && _outerBound <= _maxBounds);
}

std::vector<geo::Interval> NearUtils::intervals() {
  TRI_ASSERT(_innerBound != _maxBounds);
  TRI_ASSERT(!hasNearest());
  
  if (_innerBound > 0) {
    if (_outerBound == _maxBounds) {
      _innerBound = _outerBound;
      return {}; // search is finished
    }
    double const minBounds = S2::kAvgEdge.GetValue(S2::kMaxCellLevel - 2);
    // we already scanned the entire planet, if this fails
    TRI_ASSERT(_innerBound != _outerBound && _innerBound != _maxBounds);
    if (_statsFoundLastInterval < 256) {
      _boundDelta *= (_statsFoundLastInterval == 0 ? 4 : 2);
    } else if (_statsFoundLastInterval > 512 && _boundDelta > minBounds) {
      _boundDelta /= 2.0;
    }
    _statsFoundLastInterval = 0;
  }
  TRI_ASSERT(_boundDelta >= S2::kAvgEdge.GetValue(S2::kMaxCellLevel - 2));
  
  _innerBound = _outerBound; // initially _outerBounds == _innerBounds
  _outerBound = std::min(_outerBound + _boundDelta, _maxBounds);
  TRI_ASSERT(_innerBound <= _outerBound && _outerBound <= _maxBounds);
  TRI_ASSERT(_innerBound != _outerBound);
  LOG_TOPIC(INFO, Logger::FIXME) << "[Bounds] "
    << (size_t)(_innerBound * kEarthRadiusInMeters) << " - "
    << (size_t)(_outerBound * kEarthRadiusInMeters) << "  delta: "
    << (size_t)(_boundDelta * kEarthRadiusInMeters);//*/
  
  std::vector<S2CellId> cover;
  if (0.0 == _innerBound && _outerBound <= _maxBounds) {
    LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] 0 to something";
    S2Cap ob = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_outerBound));
    _coverer.GetCovering(ob, &cover);
  } else if (0.0 < _innerBound && _outerBound <= _maxBounds) {
    LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] something inbetween";
    // create a search ring
    std::vector<S2Region*> regions;
    S2Cap ib = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_innerBound));
    regions.push_back(new S2Cap(ib.Complement()));
    S2Cap ob = S2Cap::FromAxisAngle(_centroid, S1Angle::Radians(_outerBound));
    regions.push_back(new S2Cap(ob));
    S2RegionIntersection ring(&regions);
    _coverer.GetCovering(ring, &cover);
  } else { // invalid bounds
    TRI_ASSERT(false);
    return {};
  }

  std::vector<geo::Interval> intervals;
  if (!cover.empty()) {  // not sure if this can ever happen
    if (_scannedCells.num_cells() != 0) {
      // substract already scanned areas from cover
      S2CellUnion coverUnion;
      coverUnion.InitSwap(&cover);
      TRI_ASSERT(cover.empty());
      S2CellUnion lookup;
      lookup.GetDifference(&coverUnion, &_scannedCells);
      cover = lookup.cell_ids();
      
      /*for (auto cellId : lookup.cell_ids()) {
        if (region->MayIntersect(S2Cell(cellId))) {
          cover.push_back(cellId);
        }
      }*/
    }
    
    if (!cover.empty()) {
      GeoUtils::scanIntervals(_params.cover.worstIndexedLevel, cover,
                              intervals);
      _scannedCells.Add(cover);
    }
  }

  /*if (_params.region != nullptr) { // expensive rejection
    TRI_ASSERT(_params.filter != geo::FilterType::NONE);
   // FIXME: Use S2RegionIntersection instead ?
   for (int i = 0; i < lookup.num_cells(); i++) {
    if (region->MayIntersect(S2Cell(lookup.cell_id(i)))) {
      cover.push_back(cellId);
    }
   }
  }
  */

  // prune the _seen list revision IDs
  /*auto it = _seen.begin();
  while (it != _seen.end()) {
    if (it->second < _innerBound) {
      it = _seen.erase(it);
    } else {
      it++;
    }
  }*/

  return intervals;
}

void NearUtils::reportFound(TRI_voc_rid_t rid, geo::Coordinate const& center) {
  S2LatLng cords = S2LatLng::FromDegrees(center.latitude, center.longitude);
  S2Point pp = cords.ToPoint();
  double rad = _centroid.Angle(pp);
  
  LOG_TOPIC(INFO, Logger::FIXME)
  << "[Found] " << rid << " at " << (center.toString())
  << " distance: " << (rad * kEarthRadiusInMeters);
  
  // cheap rejections based on distance to target
  if (rad < _innerBound || _maxBounds <= rad) {
    return;
  }
  
  _statsFoundLastInterval++;  // we have to estimate scan bounds
  auto const& it = _seen.find(rid);
  if (it == _seen.end()) {
    _seen.emplace(rid, rad);

    if (_params.filterType != FilterType::NONE) { // expensive rejection
      if (!_params.filterShape.contains(&center)) {
        return;
      }
    }
    _buffer.emplace(rid, rad);
  } else {                          // deduplication
    LOG_TOPIC(ERR, Logger::FIXME) << "[Duplicate] " << rid;
    TRI_ASSERT(it->second == rad);  // should never change
  }
}

void NearUtils::estimateDensity(geo::Coordinate const& found) {
  S2LatLng cords = S2LatLng::FromDegrees(found.latitude, found.longitude);
  double delta = _centroid.Angle(cords.ToPoint()) * 4;
  if (delta > 0) {
    _boundDelta = delta;
    TRI_ASSERT(_innerBound == 0 && _buffer.empty());  // only call after reset
    LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "Estimating density with "
                                      << _boundDelta * kEarthRadiusInMeters
                                      << "m";
  }
}

bool NearUtils::isDone() const {
  TRI_ASSERT(_innerBound >= 0 && _innerBound <= _outerBound);
  TRI_ASSERT(_outerBound <= _maxBounds && _maxBounds <= M_PI);
  return _buffer.empty() && _innerBound > 0 && _outerBound == _maxBounds;
}

void NearUtils::invalidate() {
  _innerBound = _maxBounds;
  _outerBound = _maxBounds;
}
