////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"

#include "Basics/Common.h"
#include "Logger/Logger.h"
#include "Basics/StringUtils.h"
//#include "btree/btree.h"
#include "Geo/Near.h"
#include "Geo/GeoUtils.h"
#include "VocBase/voc-types.h"

#include <geometry/s1angle.h>
#include <geometry/s2.h>
#include <geometry/s2latlng.h>

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

typedef std::multimap<S2CellId, TRI_voc_rid_t> index_t;
typedef std::map<TRI_voc_rid_t, geo::Coordinate> coords_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

struct CoordAscCompare {
  inline bool operator() (const geo::Coordinate& val1, const geo::Coordinate& val2) {
    return val1.latitude < val2.latitude ||
           (val1.latitude == val2.latitude && val1.longitude < val2.longitude);
  }
};

/// Perform indexx scan
template<typename CMP>
static std::vector<TRI_voc_rid_t> nearSearch(index_t const& index,
                                             coords_t const& coords,
                                             geo::NearUtils<CMP>& near,
                                             size_t limit) {
  std::vector<TRI_voc_rid_t> result;
  
  while (!near.isDone()) {
    std::vector<geo::Interval> intervals = near.intervals();
    for (geo::Interval const& interval : intervals) {
      // seek to first element after or equal interval.min
      index_t::const_iterator it = index.lower_bound(interval.min);

      while (it != index.end() && it->first <= interval.max) {
        REQUIRE(it->first >= interval.min);
        near.reportFound(it->second, coords.at(it->second));
        it++;
      }
    }
    
    while (near.hasNearest()) {
      geo::Document doc = near.nearest();
      result.push_back(doc.rid);
      near.popNearest();
      
      if (result.size() >= limit) {
        break;
      }
    }
    if (result.size() >= limit) {
      break;
    }
  }
  return result;
}

static std::vector<geo::Coordinate> convert(coords_t const& coords,
                                            std::vector<TRI_voc_rid_t> const& docs) {
  std::vector<geo::Coordinate> result;
  for (TRI_voc_rid_t rid : docs) {
    result.push_back(coords.at(rid));
  }
  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

/*           1-9 some real world distance          */
/*   1 is London    +51.500000 -0.166666           */
/*   2 is Honolulu  +21.306111 -157.859722         */
/*   3 is Auckland  -36.916667 +174.783333         */
/*   4 is Jo'burg   -26.166667  +28.033333         */

/***********************************/
/* Simple Near queries  */
TEST_CASE("geo::NearUtils", "[geo][s2index]") {
  
  // simulated index
  index_t index;
  coords_t docs;
  TRI_voc_rid_t counter = 0;
  
  // add some entries to it
  for (double lat=-40; lat <=40 ; ++lat) {
    for (double lon=-40; lon <= 40; ++lon) {
      //geocol.insert({lat,lon});
      geo::Coordinate cc(lat, lon);
      
      std::vector<S2CellId> cells;
      Result res = geo::GeoUtils::indexCells(cc, cells);
      REQUIRE(res.ok());
      REQUIRE(cells.size() == 1);
      REQUIRE(cells[0].level() == S2::kMaxCellLevel);
      
      TRI_voc_rid_t rev = counter++;
      index.emplace(cells[0], rev);
      docs.emplace(rev, cc);
    }
  }
  
  geo::QueryParams params;
  params.sorted = true;
  params.centroid = geo::Coordinate(0,0);
  
  SECTION("query all sorted ascending") {
    params.ascending = true;
    geo::NearUtils<geo::DocumentsAscending> near(std::move(params));
    
    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, SIZE_T_MAX);
    std::set<TRI_voc_rid_t> unique;
    REQUIRE(result.size() == counter);
    
    double lastRad = 0;
    for (TRI_voc_rid_t rev : result) {
      // check that we get every document exactly once
      REQUIRE(unique.find(rev) == unique.end());
      unique.insert(rev);
      
      // check sort order
      geo::Coordinate const& cc = docs.at(rev);
      S2LatLng cords = S2LatLng::FromDegrees(cc.latitude, cc.longitude);
      S2Point pp = cords.ToPoint();
      double rad = near.centroid().Angle(pp);
      REQUIRE(rad >= lastRad);
      lastRad = rad;
    }
    REQUIRE(lastRad != 0);
  }
  
  SECTION("query all sorted ascending with limit") {
    params.ascending = true;
    geo::NearUtils<geo::DocumentsAscending> near(std::move(params));

    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 5);
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end(), CoordAscCompare());
    REQUIRE(coords[0] == geo::Coordinate(-1,0));
    REQUIRE(coords[1] == geo::Coordinate(0,-1));
    REQUIRE(coords[2] == geo::Coordinate(0,0));
    REQUIRE(coords[3] == geo::Coordinate(0,1));
    REQUIRE(coords[4] == geo::Coordinate(1,0));
  }
  
  SECTION("query sorted ascending with limit and max distance") {
    params.ascending = true;
    params.maxDistance = 111200.0;
    geo::NearUtils<geo::DocumentsAscending> near(std::move(params));

    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end(), CoordAscCompare());
    REQUIRE(coords[0] == geo::Coordinate(-1,0));
    REQUIRE(coords[1] == geo::Coordinate(0,-1));
    REQUIRE(coords[2] == geo::Coordinate(0,0));
    REQUIRE(coords[3] == geo::Coordinate(0,1));
    REQUIRE(coords[4] == geo::Coordinate(1,0));
  }
  
  SECTION("query sorted ascending with different inital delta") {
    params.ascending = true;
    params.maxDistance = 111200;
    geo::NearUtils<geo::DocumentsAscending> near(std::move(params));

    near.estimateDensity(geo::Coordinate(0,1));
    
    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end(), CoordAscCompare());
    REQUIRE(coords[0] == geo::Coordinate(-1,0));
    REQUIRE(coords[1] == geo::Coordinate(0,-1));
    REQUIRE(coords[2] == geo::Coordinate(0,0));
    REQUIRE(coords[3] == geo::Coordinate(0,1));
    REQUIRE(coords[4] == geo::Coordinate(1,0));
  }
  
  SECTION("query sorted ascending with different inital delta") {
    params.ascending = true;
    params.maxDistance = 111200;
    geo::NearUtils<geo::DocumentsAscending> near(std::move(params));

    near.estimateDensity(geo::Coordinate(0,1));
    
    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end(), CoordAscCompare());
    REQUIRE(coords[0] == geo::Coordinate(-1,0));
    REQUIRE(coords[1] == geo::Coordinate(0,-1));
    REQUIRE(coords[2] == geo::Coordinate(0,0));
    REQUIRE(coords[3] == geo::Coordinate(0,1));
    REQUIRE(coords[4] == geo::Coordinate(1,0));
  }
  
  SECTION("query all sorted descending") {
    params.ascending = false;
    geo::NearUtils<geo::DocumentsDescending> near(std::move(params));

    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, SIZE_T_MAX);
    std::set<TRI_voc_rid_t> unique;
    REQUIRE(result.size() == counter);
    
    double lastRad = geo::kEarthRadiusInMeters;
    for (TRI_voc_rid_t rev : result) {
      // check that we get every document exactly once
      REQUIRE(unique.find(rev) == unique.end());
      unique.insert(rev);
      
      // check sort order
      geo::Coordinate const& cc = docs.at(rev);
      S2LatLng cords = S2LatLng::FromDegrees(cc.latitude, cc.longitude);
      S2Point pp = cords.ToPoint();
      double rad = near.centroid().Angle(pp);
      REQUIRE(rad <= lastRad);
      lastRad = rad;
    }
    REQUIRE(lastRad == 0);
  }
  
  SECTION("query all sorted descending with limit") {
    params.ascending = false;
    geo::NearUtils<geo::DocumentsDescending> near(std::move(params));
    
    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 5);
    std::set<TRI_voc_rid_t> unique;
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    // [40,40], [40,-40], [-40, 40], [-40, -40] in any order
    for (size_t i = 0; i < 4; i++) {
      REQUIRE(std::abs(coords[i].latitude) == 40.0);
      REQUIRE(std::abs(coords[i].longitude) == 40.0);
    }
  }
  
  SECTION("query all sorted descending with limit and max distance") {
    params.ascending = false;
    params.maxDistance = 111200;
    geo::NearUtils<geo::DocumentsDescending> near(std::move(params));
    
    std::vector<TRI_voc_rid_t> result = nearSearch(index, docs, near, 1000);
    std::set<TRI_voc_rid_t> unique;
    REQUIRE(result.size() == 5);
    
    std::vector<geo::Coordinate> coords = convert(docs, result);
    REQUIRE(coords[4] == geo::Coordinate(0,0));

    for (size_t i = 0; i < 4; i++) {
      double lat = std::abs(coords[i].latitude),
             lng = std::abs(coords[i].longitude);
      REQUIRE(lat + lng == 1); // lat == 1 => lng == 0, etc
    }
  }
}

/* end of NearUtilsTest.cpp  */
