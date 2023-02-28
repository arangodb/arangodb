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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <deque>
#include <vector>

#include <s2/s2cap.h>
#include <s2/s2cell_id.h>
#include <s2/s2region.h>
#include <s2/s2region_coverer.h>

#include "Geo/GeoParams.h"
#include "Geo/Utils.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "Containers/FlatHashSet.h"

namespace arangodb::geo_index {

/// @brief Helper class to build a simple covering query iterator.
/// Will return findings not sorted by anything, will filter according
/// to the filterType. Should be storage engine agnostic.
class CoveringUtils {
 public:
  /// @brief Type of documents buffer
  typedef std::deque<LocalDocumentId> GeoDocumentsQueue;

  explicit CoveringUtils(geo::QueryParams&& params) noexcept;
  ~CoveringUtils();

 public:
  geo::FilterType filterType() const { return _params.filterType; }

  geo::ShapeContainer const& filterShape() const { return _params.filterShape; }

  /// @brief all intervals are covered, no more buffered results
  bool isDone() const { return _buffer.empty() && _allIntervalsCovered; }

  /// @brief has buffered results
  bool hasNext() const { return !_buffer.empty(); }

  size_t bufferSize() const { return _buffer.size(); }

  LocalDocumentId const& getNext() const {
    TRI_ASSERT(hasNext());
    return _buffer.front();
  }

  void next() {
    TRI_ASSERT(hasNext());
    _buffer.pop_front();
  }

  /// @brief reset query to initial state
  void reset();

  /// Call only when current scan intervals contain no more results
  std::vector<geo::Interval> intervals();

  /// buffer and sort results
  void reportFound(LocalDocumentId lid, S2Point const& center);

  /// @brief Reference to parameters used
  geo::QueryParams const& params() const { return _params; }

  size_t _found = 0;
  size_t _rejection = 0;

 private:
  bool isFilterNone() const noexcept {
    return _params.filterType == geo::FilterType::NONE;
  }

  bool isFilterContains() const noexcept {
    return _params.filterType == geo::FilterType::CONTAINS;
  }

  bool isFilterIntersects() const noexcept {
    return _params.filterType == geo::FilterType::INTERSECTS;
  }

 private:
  geo::QueryParams const _params;

  bool _allIntervalsCovered;

  /// Total number of interval calculations
  size_t _numScans;

  /// buffer of found documents
  GeoDocumentsQueue _buffer;

  // deduplication filter
  containers::FlatHashSet<uint64_t> _seenDocs;

  /// Coverer instance to use
  S2RegionCoverer _coverer;
};

}  // namespace arangodb::geo_index
