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

template <typename CMP>
NearUtils<CMP>::NearUtils(QueryParams&& qp)
    : _params(std::move(qp)),
      _origin(S2LatLng::FromDegrees(qp.origin.latitude,
                                    qp.origin.longitude).ToPoint()),
      _minBound(_params.minDistanceRad()),
      _maxBound(_params.maxDistanceRad()) {
  TRI_ASSERT(_params.origin.isValid());
  qp.cover.configureS2RegionCoverer(&_coverer);
  reset();
  TRI_ASSERT(_params.sorted);
  TRI_ASSERT(_maxBound > 0 && _maxBound <= M_PI);
  static_assert(isAcending() || isDescending(), "Invalid template");
  TRI_ASSERT(!isAcending() || _params.ascending);
  TRI_ASSERT(!isDescending() || !_params.ascending);

  /*LOG_TOPIC(ERR, Logger::FIXME)
      << "--------------------------------------------------------";
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] origin target: "
                                 << _params.origin.toString();
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] minBounds: " <<
  (size_t)(_params.minDistance * kEarthRadiusInMeters)
        << "  maxBounds:" << (size_t)(_maxBound * kEarthRadiusInMeters)
        << "  sorted " << (_params.ascending ? "asc" : "desc");//*/
}

template <typename CMP>
void NearUtils<CMP>::reset() {

  if (!_seen.empty() || !_buffer.empty()) {
    _seen.clear();
    while (!_buffer.empty()) {
      _buffer.pop();
    }
  }

  if (_boundDelta <= 0) {  // do not reset everytime
    int level = std::max(1, _params.cover.bestIndexedLevel - 2);
    // Level 15 == 474.142m
    level = std::min(level,
                     S2::kMaxDiag.GetClosestLevel(500 / kEarthRadiusInMeters));
    _boundDelta = S2::kMaxDiag.GetValue(level);  // in radians
    TRI_ASSERT(_boundDelta * kEarthRadiusInMeters >= 450);
  }
  TRI_ASSERT(_boundDelta > 0);

  // this initial interval is never used like that, see intervals()
  _innerBound = isAcending() ? _minBound : _maxBound;
  _outerBound = isAcending() ? _minBound : _maxBound;
  _statsFoundLastInterval = 0;
  TRI_ASSERT(_innerBound <= _outerBound && _outerBound <= _maxBound);
}

template <typename CMP>
std::vector<geo::Interval> NearUtils<CMP>::intervals() {
  TRI_ASSERT(!hasNearest());
  TRI_ASSERT(!isDone());
  TRI_ASSERT(!_params.ascending || _innerBound != _maxBound);

  TRI_ASSERT(_boundDelta >= S2::kMaxEdge.GetValue(S2::kMaxCellLevel - 2));
  estimateDelta();
  if (isAcending()) {
    _innerBound = _outerBound;  // initially _outerBounds == _innerBounds
    _outerBound = std::min(_outerBound + _boundDelta, _maxBound);
    if (_innerBound == _maxBound && _outerBound == _maxBound) {
      return {};  // search is finished
    }
  } else if (isDescending()) {
    _outerBound = _innerBound;  // initially _outerBounds == _innerBounds
    _innerBound = std::max(_innerBound - _boundDelta, _minBound);
    if (_outerBound == _minBound && _innerBound == _minBound) {
      return {};  // search is finished
    }
  }

  TRI_ASSERT(_innerBound <= _outerBound && _outerBound <= _maxBound);
  TRI_ASSERT(_innerBound != _outerBound);
  /*LOG_TOPIC(INFO, Logger::FIXME) << "[Bounds] "
    << (size_t)(_innerBound * kEarthRadiusInMeters) << " - "
    << (size_t)(_outerBound * kEarthRadiusInMeters) << "  delta: "
    << (size_t)(_boundDelta * kEarthRadiusInMeters);//*/

  std::vector<S2CellId> cover;
  if (_innerBound == _minBound) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] 0 to something";
    S2Cap ob = S2Cap::FromAxisAngle(_origin, S1Angle::Radians(_outerBound));
    _coverer.GetCovering(ob, &cover);
  } else if (_innerBound > _minBound) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] something inbetween";
    // create a search ring
    std::vector<S2Region*> regions;
    S2Cap ib = S2Cap::FromAxisAngle(_origin, S1Angle::Radians(_innerBound));
    regions.push_back(new S2Cap(ib.Complement()));
    S2Cap ob = S2Cap::FromAxisAngle(_origin, S1Angle::Radians(_outerBound));
    regions.push_back(new S2Cap(ob));
    S2RegionIntersection ring(&regions);
    _coverer.GetCovering(ring, &cover);
  } else {  // invalid bounds
    TRI_ASSERT(false);
    return {};
  }

  std::vector<geo::Interval> intervals;
  if (!cover.empty()) {  // not sure if this can ever happen
    if (_scannedCells.num_cells() != 0) {
      // substract already scanned areas from cover
      S2CellUnion coverUnion;
      coverUnion.InitSwap(&cover);
      S2CellUnion lookup;
      lookup.GetDifference(&coverUnion, &_scannedCells);

      TRI_ASSERT(cover.empty());  // swap should empty this
      if (!isFilterNone()) {
        TRI_ASSERT(!_params.filterShape.empty());
        for (S2CellId cellId : lookup.cell_ids()) {
          if (_params.filterShape.mayIntersect(cellId)) {
            cover.push_back(cellId);
          }
        }
      } else {
        cover = lookup.cell_ids();
      }
    }

    if (!cover.empty()) {
      GeoUtils::scanIntervals(_params.cover.worstIndexedLevel, cover,
                              intervals);
      _scannedCells.Add(cover);
    }
  }

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

template <typename CMP>
void NearUtils<CMP>::reportFound(LocalDocumentId lid,
                                 geo::Coordinate const& center) {
  S2LatLng cords = S2LatLng::FromDegrees(center.latitude, center.longitude);
  double rad = _origin.Angle(cords.ToPoint());  // distance in radians

  /*LOG_TOPIC(INFO, Logger::FIXME)
  << "[Found] " << rid << " at " << (center.toString())
  << " distance: " << (rad * kEarthRadiusInMeters);//*/

  // cheap rejections based on distance to target
  if (!isFilterIntersects()) {
    if ((isAcending() && rad < _innerBound) ||
        (isDescending() && rad > _outerBound) || rad > _maxBound ||
        rad < _minBound) {
      /*LOG_TOPIC(ERR, Logger::FIXME) << "Rejecting doc with center " << center.toString();
      LOG_TOPIC(ERR, Logger::FIXME) << "Dist: " << (rad * kEarthRadiusInMeters);//*/
      return;
    }
  }
  
  _statsFoundLastInterval++;  // we have to estimate scan bounds
  auto const& it = _seen.find(lid);
  if (it == _seen.end()) {
    _seen.emplace(lid);

    // possibly expensive point rejection, but saves parsing of document
    if (isFilterContains()) {
      TRI_ASSERT(!_params.filterShape.empty());
      if (!_params.filterShape.contains(&center)) {
        return;
      }
    }
    _buffer.emplace(lid, rad);
  }
}

template <typename CMP>
void NearUtils<CMP>::estimateDensity(geo::Coordinate const& found) {
  double const minBound = S2::kAvgDiag.GetValue(S2::kMaxCellLevel - 3);
  S2LatLng cords = S2LatLng::FromDegrees(found.latitude, found.longitude);
  double delta = _origin.Angle(cords.ToPoint()) * 4;
  if (minBound < delta && delta < M_PI) {
    _boundDelta = delta;
    // only call after reset
    TRI_ASSERT(!isAcending() || _innerBound == _minBound && _buffer.empty());
    TRI_ASSERT(!isDescending() || _innerBound == _maxBound && _buffer.empty());
    LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "Estimating density with "
                                      << _boundDelta * kEarthRadiusInMeters
                                      << "m";
  }
}

/// @brief adjust the bounds delta
template <typename CMP>
void NearUtils<CMP>::estimateDelta() {
  if ((isAcending() && _innerBound > _minBound) ||
      (isDescending() && _innerBound < _maxBound)) {
    double const minBound = S2::kMaxDiag.GetValue(S2::kMaxCellLevel - 3);
    // we already scanned the entire planet, if this fails
    TRI_ASSERT(_innerBound != _outerBound && _innerBound != _maxBound);
    if (_statsFoundLastInterval < 256) {
      _boundDelta *= (_statsFoundLastInterval == 0 ? 4 : 2);
    } else if (_statsFoundLastInterval > 1024 && _boundDelta > minBound) {
      _boundDelta /= 2.0;
    }
    TRI_ASSERT(_boundDelta > 0);
    _statsFoundLastInterval = 0;
  }
}

template class arangodb::geo::NearUtils<arangodb::geo::DocumentsAscending>;
template class arangodb::geo::NearUtils<arangodb::geo::DocumentsDescending>;
