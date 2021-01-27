////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Near.h"

#include <s2/s2cell_union.h>
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
#include "Geo/Utils.h"
#include "Logger/Logger.h"

namespace arangodb {
namespace geo_index {

template <typename CMP>
NearUtils<CMP>::NearUtils(geo::QueryParams&& qp) noexcept
    : _params(std::move(qp)),
      _origin(_params.origin.ToPoint()),
      _minAngle(S1ChordAngle::Radians(_params.minDistanceRad())),
      _maxAngle(S1ChordAngle::Radians(_params.maxDistanceRad())),
      _allIntervalsCovered(false),
      _statsFoundLastInterval(0),
      _numScans(0),
      _coverer(_params.cover.regionCovererOpts()) {
  reset();
  // Level 15 == 474.142m (start with 15 essentially)
  TRI_ASSERT(_params.origin.is_valid());
  TRI_ASSERT(_params.sorted);
  TRI_ASSERT(_maxAngle >= _minAngle && _maxAngle <= S1ChordAngle::Straight());
  static_assert(isAscending() || isDescending(), "Invalid template");
  TRI_ASSERT(!isAscending() || _params.ascending);
  TRI_ASSERT(!isDescending() || !_params.ascending);

  /*LOG_TOPIC("f2eed", ERR, Logger::FIXME)
      << "--------------------------------------------------------";
  LOG_TOPIC("f71a1", INFO, Logger::FIXME) << "[Near] origin target: "
                                 << _params.origin.ToStringInDegrees();
  LOG_TOPIC("179ef", INFO, Logger::FIXME) << "[Near] minBounds: " <<
  (size_t)(_params.minDistanceRad() * geo::kEarthRadiusInMeters)
        << "  maxBounds:" << (size_t)(_params.maxDistanceRad() *
  geo::kEarthRadiusInMeters)
        << "  sorted " << (_params.ascending ? "asc" : "desc");*/
}

template <typename CMP>
NearUtils<CMP>::~NearUtils() {
  // LOG_TOPIC("5d2f1", ERR, Logger::FIXME) << "Scans: " << _numScans << " Found: " <<
  // _found << " Rejections: " << _rejection;
}

template <typename CMP>
void NearUtils<CMP>::reset() {
  _seenDocs.clear();
  while (!_buffer.empty()) {
    _buffer.pop();
  }

  _allIntervalsCovered = false;
  _statsFoundLastInterval = 0;
  _numScans = 0;
  // this initial interval is never used like that, see intervals()
  _outerAngle = _innerAngle = isAscending() ? _minAngle : _maxAngle;
  const int level = S2::kAvgDiag.GetClosestLevel(8000 / geo::kEarthRadiusInMeters);
  _deltaAngle = S1ChordAngle::Radians(S2::kAvgDiag.GetValue(level));
  TRI_ASSERT(!_deltaAngle.is_zero());
  TRI_ASSERT(_deltaAngle.radians() * geo::kEarthRadiusInMeters >= 400);

  if (_minAngle == _maxAngle) {  // no search area
    _allIntervalsCovered = true;
  }
}

/// aid density estimation by reporting a result close
/// to the target coordinates
template <typename CMP>
void NearUtils<CMP>::estimateDensity(S2Point const& found) {
  S1ChordAngle minAngle = S1ChordAngle::Radians(250 / geo::kEarthRadiusInMeters);
  S1ChordAngle delta(_origin, found);
  if (minAngle < delta) {
    // overestimating the delta initially seems cheaper than doing more
    // iterations
    double fac = std::max(static_cast<double>(_params.limit) / M_PI, 2.0);
    _deltaAngle = S1ChordAngle::FromLength2(delta.length2() * fac);
    // only call after reset
    TRI_ASSERT(!isAscending() || (_innerAngle == _minAngle && _buffer.empty()));
    TRI_ASSERT(!isDescending() || (_innerAngle == _maxAngle && _buffer.empty()));
    /*LOG_TOPIC("42584", ERR, Logger::ENGINES)
    << "Estimating density with " << _deltaAngle.radians() *
    geo::kEarthRadiusInMeters
    << "m";*/
  }
}

/// Makes sure we do not have a search area already covered by cell_ids.
/// The parameter cell_ids must be normalized, otherwise result is undefined
/// Calculates id - cell_ids and adds the remaining cell(s) to result
static void GetDifference(std::vector<S2CellId> const& cell_ids, S2CellId id,
                          std::vector<S2CellId>* result) {
  // If they intersect but the difference is non-empty, divide and conquer.

  std::vector<S2CellId>::const_iterator i =
      std::lower_bound(cell_ids.begin(), cell_ids.end(), id);
  auto j = i;
  bool intersects = (i != cell_ids.end() && i->range_min() <= id.range_max()) ||
                    (i != cell_ids.begin() && (--i)->range_max() >= id.range_min());

  if (!intersects) {  // does not intersect cell_ids, just add to result
    result->push_back(id);
  } else {  // find child cells of id which might still remain
    // cell_ids.Intersects(id) == true
    bool contains = (j != cell_ids.end() && j->range_min() <= id) ||
                    (j != cell_ids.begin() && (--j)->range_max() >= id);
    if (!contains) {
      S2CellId child = id.child_begin();
      for (int x = 0;; ++x) {
        GetDifference(cell_ids, child, result);
        if (x == 3) break;  // Avoid unnecessary next() computation.
        child = child.next();
      }
    }
  }
}

template <typename CMP>
std::vector<geo::Interval> NearUtils<CMP>::intervals() {
  TRI_ASSERT(!hasNearest());
  TRI_ASSERT(!isDone());
  // TRI_ASSERT(!_params.ascending || _innerAngle != _maxAngle);
  TRI_ASSERT(_deltaAngle >=
             S1ChordAngle::Radians(S2::kMaxEdge.GetValue(S2::kMaxCellLevel - 2)));
  
  std::vector<geo::Interval> intervals;

  if (_numScans == 0) {
    calculateBounds();
    TRI_ASSERT(_innerAngle <= _outerAngle && _outerAngle <= _maxAngle);
  } /* else {
     LOG_TOPIC("a298e", WARN, Logger::FIXME) << "Found ("<<_numScans<<"): " << _found;
   }*/
  _numScans++;

  TRI_ASSERT(_innerAngle <= _outerAngle && _outerAngle <= _maxAngle);
  
  TRI_ASSERT(_innerAngle != _outerAngle);
  std::vector<S2CellId> cover;
  if (_innerAngle == _minAngle) {
    // LOG_TOPIC("55f3b", INFO, Logger::FIXME) << "[Scan] 0 to something";
    S2Cap ob = S2Cap(_origin, _outerAngle);
    //_coverer.GetCovering(ob, &cover);
    if (_scannedCells.empty()) {
      _coverer.GetFastCovering(ob, &cover);
    } else {
      std::vector<S2CellId> tmpCover;
      _coverer.GetFastCovering(ob, &tmpCover);
      for (S2CellId id : tmpCover) {
        GetDifference(_scannedCells, id, &cover);
      }
    }
  } else if (_innerAngle > _minAngle) {
    // create a search ring

    if (!_scannedCells.empty()) {
      S2Cap ob(_origin, _outerAngle);  // outer ring
      std::vector<S2CellId> tmpCover;
      _coverer.GetCovering(ob, &tmpCover);
      //_coverer.GetFastCovering(ob, &tmpCover);
      for (S2CellId id : tmpCover) {
        GetDifference(_scannedCells, id, &cover);
      }
    } else {
      // expensive exact cover
      std::vector<std::unique_ptr<S2Region>> regions;
      regions.reserve(2);
      S2Cap ib(_origin, _innerAngle);  // inner ring
      regions.push_back(std::make_unique<S2Cap>(ib.Complement()));
      regions.push_back(std::make_unique<S2Cap>(_origin, _outerAngle));
      S2RegionIntersection ring(std::move(regions));
      _coverer.GetCovering(ring, &cover);
    }

    /*std::vector<std::unique_ptr<S2Region>> regions;
    S2Cap ib(_origin, _innerAngle); // inner ring
    regions.push_back(std::make_unique<S2Cap>(ib.Complement()));
    regions.push_back(std::make_unique<S2Cap>(_origin, _outerAngle));
    S2RegionIntersection ring(std::move(regions));

    if (_scannedCells.num_cells() == 0) {
      _coverer.GetCovering(ring, &cover);
    } else {
      std::vector<S2CellId> tmpCover;
      _coverer.GetCovering(ring, &tmpCover);
      for (S2CellId id : tmpCover) {
        GetDifferenceInternal(id, _scannedCells, &cover);
      }
    }*/

  } else {  // invalid bounds
    TRI_ASSERT(false);
    return intervals;
  }

  if (!cover.empty()) {
    geo::utils::scanIntervals(_params, cover, intervals);
    _scannedCells.insert(_scannedCells.end(), cover.begin(), cover.end());
    // needed for difference calculation, will sort the IDs replace
    // 4 child cells with one parent cell and remove duplicates
    S2CellUnion::Normalize(&_scannedCells);
  }

  return intervals;
}

template <typename CMP>
void NearUtils<CMP>::reportFound(LocalDocumentId lid, S2Point const& center) {
  S1ChordAngle angle(_origin, center);

  // cheap rejections based on distance to target
  if (!isFilterIntersects()) {
    if ((isAscending() && angle < _innerAngle) || (isDescending() && angle > _outerAngle) ||
        angle > _maxAngle || angle < _minAngle) {
      /*LOG_TOPIC("8cc01", ERR, Logger::FIXME) << "Rejecting doc with center " <<
      center.toString();
      LOG_TOPIC("38f63", ERR, Logger::FIXME) << "Dist: " << (rad *
      geo::kEarthRadiusInMeters);*/
      _rejection++;
      return;
    }
  }

  if (!_params.pointsOnly) {
    auto result = _seenDocs.insert(lid.id());
    if (!result.second) {
      _rejection++;
      return;  // ignore repeated documents
    }
  }

  // possibly expensive point rejection, but saves parsing of document
  if (isFilterContains()) {
    TRI_ASSERT(!_params.filterShape.empty());
    if (!_params.filterShape.contains(center)) {
      _rejection++;
      return;
    }
  }
  _found++;
  _statsFoundLastInterval++;  // we have to estimate scan bounds
  _buffer.emplace(lid, angle);
}

/// called after current intervals were scanned
template <typename CMP>
void NearUtils<CMP>::didScanIntervals() {
  if (!_allIntervalsCovered) {
    estimateDelta();
    calculateBounds();
  }
}

/// @brief adjust the bounds delta
template <typename CMP>
void NearUtils<CMP>::estimateDelta() {
  S1ChordAngle minBound =
      S1ChordAngle::Radians(S2::kMaxDiag.GetValue(S2::kMaxCellLevel - 3));
  if (_statsFoundLastInterval <= 64) {
    _deltaAngle = S1ChordAngle::FromLength2(_deltaAngle.length2() * 4);
  } else if (_statsFoundLastInterval <= 256) {
    _deltaAngle += _deltaAngle;
  } else if (_statsFoundLastInterval > 1024 && _deltaAngle > minBound) {
    _deltaAngle = S1ChordAngle::FromLength2(_deltaAngle.length2() / 2);
  }
  _statsFoundLastInterval = 0;
  TRI_ASSERT(_deltaAngle > S1ChordAngle::Zero());
}

/// @brief estimate the scan bounds
template <typename CMP>
void NearUtils<CMP>::calculateBounds() {
  TRI_ASSERT(!_deltaAngle.is_zero() && _deltaAngle.is_valid());
  if (isAscending()) {
    _innerAngle = _outerAngle;  // initially _outerAngles == _innerAngles
    _outerAngle = std::min(_outerAngle + _deltaAngle, _maxAngle);
    if (_innerAngle == _maxAngle && _outerAngle == _maxAngle) {
      _allIntervalsCovered = true;
    }
  } else if (isDescending()) {
    _outerAngle = _innerAngle;  // initially _outerAngles == _innerAngles
    _innerAngle = std::max(_innerAngle - _deltaAngle, _minAngle);
    if (_outerAngle == _minAngle && _innerAngle == _minAngle) {
      _allIntervalsCovered = true;
    }
  }  // TRI_ASSERT(false)
}

template class NearUtils<DocumentsAscending>;
template class NearUtils<DocumentsDescending>;

}  // namespace geo_index
}  // namespace arangodb
