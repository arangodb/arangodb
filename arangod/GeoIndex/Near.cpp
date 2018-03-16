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

#include <s2/s2latlng.h>
#include <s2/s2metrics.h>
#include <s2/s2region.h>
#include <s2/s2region_coverer.h>
#include <s2/s2region_intersection.h>

#include <s2/s2distance_target.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Geo/GeoParams.h"
#include "Geo/GeoUtils.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace geo_index {

template <typename CMP, typename SEEN>
NearUtils<CMP, SEEN>::NearUtils(geo::QueryParams&& qp) noexcept
    : _params(std::move(qp)),
      _origin(_params.origin.ToPoint()),
      _minAngle(S1ChordAngle::Radians(_params.minDistanceRad())),
      _maxAngle(S1ChordAngle::Radians(_params.maxDistanceRad())),
      _allIntervalsCovered(false),
      _coverer(_params.cover.regionCovererOpts()) {
  TRI_ASSERT(_params.origin.is_valid());
  reset();
  TRI_ASSERT(_params.sorted);
  TRI_ASSERT(_maxAngle >= _minAngle &&
             _maxAngle <= S1ChordAngle::Straight());
  static_assert(isAscending() || isDescending(), "Invalid template");
  TRI_ASSERT(!isAscending() || _params.ascending);
  TRI_ASSERT(!isDescending() || !_params.ascending);

  /*LOG_TOPIC(ERR, Logger::FIXME)
      << "--------------------------------------------------------";
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] origin target: "
                                 << _params.origin.toString();
  LOG_TOPIC(INFO, Logger::FIXME) << "[Near] minBounds: " <<
  (size_t)(_params.minDistance * geo::kEarthRadiusInMeters)
        << "  maxBounds:" << (size_t)(_maxAngle * geo::kEarthRadiusInMeters)
        << "  sorted " << (_params.ascending ? "asc" : "desc");*/
}

template <typename CMP, typename SEEN>
void NearUtils<CMP, SEEN>::reset() {
  _deduplicator.clear();
  while (!_buffer.empty()) {
    _buffer.pop();
  }

  // Level 15 == 474.142m (start with 15 essentially)
  const int mL = S2::kAvgDiag.GetClosestLevel(500 / geo::kEarthRadiusInMeters);
  if (_deltaAngle.is_zero()) {  // do not reset everytime
    int level = std::max(std::min(mL, _params.cover.bestIndexedLevel), 2);
    _deltaAngle += S1ChordAngle::Radians(S2::kAvgDiag.GetValue(level));
    TRI_ASSERT(!_deltaAngle.is_zero());
    TRI_ASSERT(_deltaAngle.radians() * geo::kEarthRadiusInMeters >= 400);
  }
  
  _allIntervalsCovered = false;
  // this initial interval is never used like that, see intervals()
  _innerAngle = isAscending() ? _minAngle : _maxAngle;
  _outerAngle = isAscending() ? _minAngle : _maxAngle;
  _statsFoundLastInterval = 0;
  TRI_ASSERT(_innerAngle <= _outerAngle && _outerAngle <= _maxAngle);
}

template <typename CMP, typename SEEN>
std::vector<geo::Interval> NearUtils<CMP, SEEN>::intervals() {
  TRI_ASSERT(!hasNearest());
  TRI_ASSERT(!isDone());
  TRI_ASSERT(!_params.ascending || _innerAngle != _maxAngle);
  TRI_ASSERT(_deltaAngle >= S1ChordAngle::Radians(S2::kMaxEdge.GetValue(S2::kMaxCellLevel - 2)));
  // TRI_ASSERT(!_allIntervalsCovered);
  
  if (_params.fullRange) {
    _innerAngle = _minAngle;
    _outerAngle = _maxAngle;
    _allIntervalsCovered = true;
  } else if (isAscending()) {
    estimateDelta();
    _innerAngle = _outerAngle;  // initially _outerAngles == _innerAngles
    _outerAngle = std::min(_outerAngle + _deltaAngle, _maxAngle);
    if (_innerAngle == _maxAngle && _outerAngle == _maxAngle) {
      _allIntervalsCovered = true;
      return {};  // search is finished
    }
  } else if (isDescending()) {
    estimateDelta();
    _outerAngle = _innerAngle;  // initially _outerAngles == _innerAngles
    _innerAngle = std::max(_innerAngle - _deltaAngle, _minAngle);
    if (_outerAngle == _minAngle && _innerAngle == _minAngle) {
      _allIntervalsCovered = true;
      return {};  // search is finished
    }
  }

  TRI_ASSERT(_innerAngle <= _outerAngle && _outerAngle <= _maxAngle);
  TRI_ASSERT(_innerAngle != _outerAngle);
  /*LOG_TOPIC(INFO, Logger::FIXME) << "[Bounds] "
    << (size_t)(_innerAngle * geo::kEarthRadiusInMeters) << " - "
    << (size_t)(_outerAngle * geo::kEarthRadiusInMeters) << "  delta: "
    << (size_t)(_deltaAngle * geo::kEarthRadiusInMeters);*/

  std::vector<S2CellId> cover;
  if (_innerAngle == _minAngle) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] 0 to something";
    S2Cap ob = S2Cap(_origin, _outerAngle);
    //ob.GetCellUnionBound(&cover);
    //_coverer.GetFastCovering
    _coverer.GetCovering(ob, &cover);
  } else if (_innerAngle > _minAngle) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] something inbetween";
    // create a search ring
    std::vector<std::unique_ptr<S2Region>> regions;
    S2Cap ib(_origin, _innerAngle);
    regions.push_back(std::make_unique<S2Cap>(ib.Complement()));
    regions.push_back(
        std::make_unique<S2Cap>(_origin, _outerAngle));
    S2RegionIntersection ring(std::move(regions));
    _coverer.GetCovering(ring, &cover);
  } else {  // invalid bounds
    TRI_ASSERT(false);
    return {};
  }

  std::vector<geo::Interval> intervals;
  if (!cover.empty()) {  // not sure if this can ever happen
    if (_scannedCells.num_cells() != 0) {
      // substract already scanned areas from cover
      S2CellUnion coverUnion(std::move(cover));
      S2CellUnion lookup = coverUnion.Difference(_scannedCells);

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
      geo::GeoUtils::scanIntervals(_params, cover, intervals);
      _scannedCells.Add(cover);
    }
  }

  // prune the _seen list revision IDs
  /*auto it = _seen.begin();
  while (it != _seen.end()) {
    if (it->second < _innerAngle) {
      it = _seen.erase(it);
    } else {
      it++;
    }
  }*/

  return intervals;
}

template <typename CMP, typename SEEN>
void NearUtils<CMP, SEEN>::reportFound(LocalDocumentId lid,
                                       S2Point const& center) {
  /*S1ChordAngle angle(_origin, toPoint(center));
  double rad = angle.radians();*/
  S1ChordAngle angle(_origin, center);
  
  /*LOG_TOPIC(INFO, Logger::FIXME)
  << "[Found] " << rid << " at " << (center.toString())
  << " distance: " << (rad * geo::kEarthRadiusInMeters);*/

  // cheap rejections based on distance to target
  if (!isFilterIntersects()) {
    if ((isAscending() && angle < _innerAngle) ||
        (isDescending() && angle > _outerAngle) || angle > _maxAngle ||
        angle < _minAngle) {
      /*LOG_TOPIC(ERR, Logger::FIXME) << "Rejecting doc with center " <<
      center.toString();
      LOG_TOPIC(ERR, Logger::FIXME) << "Dist: " << (rad *
      geo::kEarthRadiusInMeters);*/
      return;
    }
  }
  if (_deduplicator(lid)) {
    return;  // ignore repeated documents
  }

  // possibly expensive point rejection, but saves parsing of document
  if (isFilterContains()) {
    TRI_ASSERT(!_params.filterShape.empty());
    if (!_params.filterShape.contains(center)) {
      return;
    }
  }
  _statsFoundLastInterval++;  // we have to estimate scan bounds
  _buffer.emplace(lid, angle);
}

template <typename CMP, typename SEEN>
void NearUtils<CMP, SEEN>::estimateDensity(S2Point const& found) {
  if (_params.fullRange) {
    return; // skip in any case
  }
  
  /*double const minBound = S2::kAvgDiag.GetValue(S2::kMaxCellLevel - 3);
  double fac = std::max(2.0 * std::sqrt(static_cast<double>(_params.limit) / M_PI), 4.0);
  double delta = _origin.Angle(toPoint(found)) * fac;
  if (minBound < delta && delta <= geo::kMaxRadiansBetweenPoints) {
    _deltaAngle = delta;
    // only call after reset
    TRI_ASSERT(!isAscending() || (_innerAngle == _minAngle && _buffer.empty()));
    TRI_ASSERT(!isDescending() ||
               (_innerAngle == _maxAngle && _buffer.empty()));
    LOG_TOPIC(DEBUG, Logger::ENGINES)
        << "Estimating density with " << _deltaAngle.radians() * geo::kEarthRadiusInMeters
        << "m";
  }*/
}

/// @brief adjust the bounds delta
template <typename CMP, typename SEEN>
void NearUtils<CMP, SEEN>::estimateDelta() {
  if ((isAscending() && _innerAngle > _minAngle) ||
      (isDescending() && _innerAngle < _maxAngle)) {
    
    S1ChordAngle minBound = S1ChordAngle::Radians(S2::kMaxDiag.GetValue(S2::kMaxCellLevel - 3));
    // we already scanned the entire planet, if this fails
    TRI_ASSERT(_innerAngle != _outerAngle && _innerAngle != _maxAngle);
    if (_statsFoundLastInterval < 256) {
      _deltaAngle += _deltaAngle + (_statsFoundLastInterval == 0 ? _deltaAngle : S1ChordAngle::Zero());
    } else if (_statsFoundLastInterval > 1024 && _deltaAngle > minBound) {
      _deltaAngle = S1ChordAngle::FromLength2(_deltaAngle.length2() / 2);
    }
    TRI_ASSERT(_deltaAngle > S1ChordAngle::Zero());
    _statsFoundLastInterval = 0;
  }
}

template class NearUtils<DocumentsAscending, Deduplicator>;
template class NearUtils<DocumentsDescending, Deduplicator>;
template class NearUtils<DocumentsAscending, NoopDeduplicator>;
template class NearUtils<DocumentsDescending, NoopDeduplicator>;

}  // namespace geo_index
}  // namespace arangodb
