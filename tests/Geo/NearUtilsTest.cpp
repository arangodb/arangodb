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
#include "Geo/GeoJson.h"
#include "Geo/GeoUtils.h"
#include "GeoIndex/Near.h"
#include "VocBase/voc-types.h"

#include <cmath>
#include <s2/s1angle.h>
#include <s2/s2metrics.h>
#include <s2/s2latlng.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#ifdef WIN32
#undef near
#endif

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

typedef std::multimap<S2CellId, LocalDocumentId> index_t;
typedef std::map<LocalDocumentId, S2LatLng> coords_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

/// Perform indexx scan
template<typename CMP>
static std::vector<LocalDocumentId> nearSearch(index_t const& index,
                                               coords_t const& coords,
                                               geo_index::NearUtils<CMP>& near,
                                               size_t limit) {
  std::vector<LocalDocumentId> result;

  while (!near.isDone()) {
    std::vector<geo::Interval> intervals = near.intervals();
    for (geo::Interval const& interval : intervals) {
      // seek to first element after or equal interval.min
      index_t::const_iterator it = index.lower_bound(interval.range_min);

      while (it != index.end() && it->first <= interval.range_max) {
        REQUIRE(it->first >= interval.range_min);

        S2Point center = coords.at(it->second).ToPoint();
        near.reportFound(it->second, center);
        it++;
      }
    }
    near.didScanIntervals(); // calculate new bounds

    while (near.hasNearest()) {
      geo_index::Document doc = near.nearest();
      result.push_back(doc.token);
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

static std::vector<S2LatLng> convert(coords_t const& coords,
                                    std::vector<LocalDocumentId> const& docs) {
  std::vector<S2LatLng> result;
  for (LocalDocumentId rid : docs) {
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

typedef geo_index::NearUtils<geo_index::DocumentsAscending> AscIterator;
typedef geo_index::NearUtils<geo_index::DocumentsDescending> DescIterator;

TEST_CASE("Simple near queries", "[geo][s2index]") {

  // simulated index
  index_t index;
  coords_t docs;
  size_t counter = 0;

  // add some entries to it
  for (double lat=-90; lat <=90 ; ++lat) {
    for (double lon=-180; lon <= 180; ++lon) {
      //geocol.insert({lat,lon});
      S2LatLng cc = S2LatLng::FromDegrees(lat, lon);
      S2CellId cell(cc.ToPoint());
      REQUIRE(cell.level() == S2::kMaxCellLevel);

      LocalDocumentId rev(counter++);
      index.emplace(cell, rev);
      docs.emplace(rev, cc);
    }
  }
  REQUIRE(65341 == counter);
  REQUIRE(docs.size() == counter);
  REQUIRE(index.size() == counter);

  geo::QueryParams params;
  params.sorted = true;
  params.origin = S2LatLng::FromDegrees(0,0);

  SECTION("query all sorted ascending") {
    params.ascending = true;
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, SIZE_MAX);
    std::set<LocalDocumentId> unique;
    REQUIRE(result.size() == counter);

    double lastRad = 0;
    for (LocalDocumentId rev : result) {
      // check that we get every document exactly once
      REQUIRE(unique.find(rev) == unique.end());
      unique.insert(rev);

      // check sort order
      S2LatLng const& cords = docs.at(rev);
      S2Point pp = cords.ToPoint();
      double rad = near.origin().Angle(pp);
      bool eq = rad > lastRad || std::fabs(rad - lastRad) <= geo::kRadEps;
      REQUIRE(eq); // rad >= lastRad
      lastRad = rad;
    }
    REQUIRE(lastRad != 0);
  }

  SECTION("query all sorted ascending with limit") {
    params.ascending = true;
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 5);
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end());
    REQUIRE(coords[0] == S2LatLng::FromDegrees(-1,0));
    REQUIRE(coords[1] == S2LatLng::FromDegrees(0,-1));
    REQUIRE(coords[2] == S2LatLng::FromDegrees(0,0));
    REQUIRE(coords[3] == S2LatLng::FromDegrees(0,1));
    REQUIRE(coords[4] == S2LatLng::FromDegrees(1,0));
  }

  SECTION("query sorted ascending with limit and max distance") {
    params.ascending = true;
    params.maxDistance = 111200.0;
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end());
    REQUIRE(coords[0] == S2LatLng::FromDegrees(-1,0));
    REQUIRE(coords[1] == S2LatLng::FromDegrees(0,-1));
    REQUIRE(coords[2] == S2LatLng::FromDegrees(0,0));
    REQUIRE(coords[3] == S2LatLng::FromDegrees(0,1));
    REQUIRE(coords[4] == S2LatLng::FromDegrees(1,0));
  }

  SECTION("query sorted ascending with different inital delta") {
    params.ascending = true;
    params.maxDistance = 111200;
    AscIterator near(std::move(params));

    near.estimateDensity(S2LatLng::FromDegrees(0,1).ToPoint());

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end());
    REQUIRE(coords[0] == S2LatLng::FromDegrees(-1,0));
    REQUIRE(coords[1] == S2LatLng::FromDegrees(0,-1));
    REQUIRE(coords[2] == S2LatLng::FromDegrees(0,0));
    REQUIRE(coords[3] == S2LatLng::FromDegrees(0,1));
    REQUIRE(coords[4] == S2LatLng::FromDegrees(1,0));
  }

  SECTION("query sorted ascending with different inital delta") {
    params.ascending = true;
    params.maxDistance = 111200;
    AscIterator near(std::move(params));

    near.estimateDensity(S2LatLng::FromDegrees(0,1).ToPoint());

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    std::sort(coords.begin(), coords.end());
    REQUIRE(coords[0] == S2LatLng::FromDegrees(-1,0));
    REQUIRE(coords[1] == S2LatLng::FromDegrees(0,-1));
    REQUIRE(coords[2] == S2LatLng::FromDegrees(0,0));
    REQUIRE(coords[3] == S2LatLng::FromDegrees(0,1));
    REQUIRE(coords[4] == S2LatLng::FromDegrees(1,0));
  }

  SECTION("query all sorted descending") {
    params.ascending = false;
    DescIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, SIZE_MAX);
    std::set<LocalDocumentId> unique;
    REQUIRE(result.size() == counter);

    double lastRad = geo::kEarthRadiusInMeters;
    for (LocalDocumentId rev : result) {
      // check that we get every document exactly once
      REQUIRE(unique.find(rev) == unique.end());
      unique.insert(rev);

      // check sort order
      S2LatLng const& coords = docs.at(rev);
      double rad = near.origin().Angle(coords.ToPoint());
      bool eq = rad < lastRad || std::fabs(rad - lastRad) <= geo::kRadEps;
      REQUIRE(eq); // rad <= lastRad
      lastRad = rad;
    }
    REQUIRE(lastRad == 0);
  }

  SECTION("query all sorted descending with limit") {
    params.ascending = false;
    DescIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 5);
    std::set<LocalDocumentId> unique;
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    // [0,180], [0,-180] in any order
    for (size_t i = 0; i < 2; i++) {
      REQUIRE(coords[i].lat().degrees() == 0.0);
      REQUIRE(std::abs(coords[i].lng().degrees()) == 180.0);
    }
  }

  SECTION("query all sorted descending with limit and max distance") {
    params.ascending = false;
    params.maxDistance = 111200;
    DescIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
    std::set<LocalDocumentId> unique;
    REQUIRE(result.size() == 5);

    std::vector<S2LatLng> coords = convert(docs, result);
    REQUIRE(coords[4] == S2LatLng::FromDegrees(0,0));

    for (size_t i = 0; i < 4; i++) {
      double lat = std::abs(coords[i].lat().degrees()),
             lng = std::abs(coords[i].lng().degrees());
      REQUIRE(lat + lng == 1); // lat == 1 => lng == 0, etc
    }
  }
}


/* second main batch of tests                      */
/* insert 10 x 10 array of points near south pole  */
/* then do some searches, results checked against  */
/* the same run with full table scan               */

TEST_CASE("Query point around", "[geo][s2index]") {

  index_t index;
  coords_t docs;
  size_t counter = 0;

  double lat=-89.0;
  for(int i=0;i<10;i++) {
    double lon = 17.0;
    for(int j=0;j<10;j++) {
      //geocol.insert({lat,lon});
      S2LatLng cc = S2LatLng::FromDegrees(lat, lon);

      S2CellId cell(cc);
      REQUIRE(cell.level() == S2::kMaxCellLevel);

      LocalDocumentId rev(counter++);
      index.emplace(cell, rev);
      docs.emplace(rev, cc);
      lon += 1.0;
    }
    lat += 1.0;
  }

  geo::QueryParams params;
  params.sorted = true;
  params.ascending = true;

  auto checkResult = [&](S2Point const& origin,
                         std::vector<LocalDocumentId> const& result) {
    double lastRad = 0;
    for (LocalDocumentId const& rev : result) {
      // check sort order
      S2LatLng const& cords = docs.at(rev);
      double rad = origin.Angle(cords.ToPoint());
      REQUIRE(rad >= lastRad);
      lastRad = rad;
    }
    REQUIRE(lastRad != 0);
  };

  SECTION("southpole (1)") {
    params.origin = S2LatLng::FromDegrees(-83.2, 19.2);
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 7);
    REQUIRE(result.size() == 7);
    checkResult(near.origin(), result);
  }

  SECTION("southpole (2)") {
    params.origin = S2LatLng::FromDegrees(-83.2, 19.2);
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 110);
    REQUIRE(result.size() == 100);
    checkResult(near.origin(), result);
  }

  SECTION("southpole (3)") {
    params.origin = S2LatLng::FromDegrees(-89.9, 0);
    AscIterator near(std::move(params));

    std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 110);
    REQUIRE(result.size() == 100);
    checkResult(near.origin(), result);
  }
}

/* third main batch of tests                      */
/* adding grid of 40x40 point                     */
/* performing query sorted by result with  */
/*                */

static std::shared_ptr<VPackBuilder> createBuilder(char const* c) {
  
  VPackOptions options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);
  
  return parser.steal();
}


TEST_CASE("Query points contained in", "[geo][s2index]") {
  
  index_t index;
  coords_t docs;
  size_t counter = 0;
  
  // add some entries to it
  for (double lat=-40; lat <=40 ; ++lat) {
    for (double lon=-40; lon <= 40; ++lon) {
      //geocol.insert({lat,lon});
      S2LatLng cc = S2LatLng::FromDegrees(lat, lon);
      S2CellId cell(cc.ToPoint());
      REQUIRE(cell.level() == S2::kMaxCellLevel);
      
      LocalDocumentId rev(counter++);
      index.emplace(cell, rev);
      docs.emplace(rev, cc);
    }
  }
  REQUIRE(6561 == counter);
  REQUIRE(docs.size() == counter);
  REQUIRE(index.size() == counter);
  

  geo::QueryParams params;
  params.sorted = true;
  params.ascending = true;
  
  auto checkResult = [&](std::vector<LocalDocumentId> const& result,
                         std::vector<std::pair<double,double>> expected) {
    REQUIRE(result.size() == expected.size());

    std::vector<std::pair<double,double>> latLngResult;
    for (LocalDocumentId const& rev : result) {
      // check sort order
      S2LatLng const& cords = docs.at(rev);
      latLngResult.emplace_back(cords.lat().degrees(), cords.lng().degrees());
    }
    
    std::sort(latLngResult.begin(), latLngResult.end());
    std::sort(expected.begin(), expected.end());
    auto it = latLngResult.begin();
    auto it2 = expected.begin();
    for(; it != latLngResult.end(); it++) {
      double diff = std::fabs(std::max(it->first - it2->first, it->second - it2->second));
      REQUIRE(diff < 0.00001);
      it2++;
    }
  };
  
  params.filterType = geo::FilterType::CONTAINS;

  SECTION("polygon") {
    auto polygon = createBuilder(R"=({"type": "Polygon", "coordinates":
                                 [[[-11.5, 23.5], [-6, 26], [-10.5, 26.1], [-11.5, 23.5]]]})=");
    
    geo::geojson::parsePolygon(polygon->slice(), params.filterShape);
    params.filterShape.updateBounds(params);
    
    AscIterator near(std::move(params));
    checkResult(nearSearch(index, docs, near, 10000),
                {{ 24.0, -11.0 }, { 25.0, -10.0 }, { 25.0, -9.0 },
                  { 26.0, -10.0 }, { 26.0, -9.0 }, { 26.0, -8.0 }, { 26.0, -7.0 }, { 26.0, -6.0 }});
  }
  
  SECTION("rectangle") {
    auto rect = createBuilder(R"=({"type": "Polygon", "coordinates":[[[0,0],[1.5,0],[1.5,1.5],[0,1.5],[0,0]]]})=");
    geo::geojson::parsePolygon(rect->slice(), params.filterShape);
    REQUIRE(params.filterShape.type() == geo::ShapeContainer::Type::S2_LATLNGRECT);
    params.filterShape.updateBounds(params);
    
    AscIterator near(std::move(params));
    checkResult(nearSearch(index, docs, near, 10000),
                {{ 0.0, 0.0 }, { 1.0, 0.0 }, { 1.0, 1.0 }, { 0.0, 1.0 }});
  }
}

/* end of NearUtilsTest.cpp  */
