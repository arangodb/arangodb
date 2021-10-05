////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include <map>

#include "Basics/StringUtils.h"
#include "Geo/GeoJson.h"
#include "Geo/Utils.h"
#include "GeoIndex/Near.h"
#include "Logger/Logger.h"
#include "VocBase/voc-types.h"

#include <s2/s1angle.h>
#include <s2/s2latlng.h>
#include <s2/s2metrics.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>
#include <cmath>

#ifdef _WIN32
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
template <typename CMP>
static std::vector<LocalDocumentId> nearSearch(index_t const& index, coords_t const& coords,
                                               geo_index::NearUtils<CMP>& near,
                                               size_t limit) {
  std::vector<LocalDocumentId> result;

  while (!near.isDone()) {
    std::vector<geo::Interval> intervals = near.intervals();
    for (geo::Interval const& interval : intervals) {
      // seek to first element after or equal interval.min
      index_t::const_iterator it = index.lower_bound(interval.range_min);

      while (it != index.end() && it->first <= interval.range_max) {
        EXPECT_TRUE(it->first >= interval.range_min);

        S2Point center = coords.at(it->second).ToPoint();
        near.reportFound(it->second, center);
        it++;
      }
    }
    near.didScanIntervals();  // calculate new bounds

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

class SimpleNearQueriesTest : public ::testing::Test {
 protected:
  // simulated index
  index_t index;
  coords_t docs;
  size_t counter = 0;
  geo::QueryParams params;

  SimpleNearQueriesTest() : counter(0) {
    // add some entries to it
    for (double lat = -90; lat <= 90; ++lat) {
      for (double lon = -180; lon <= 180; ++lon) {
        // geocol.insert({lat,lon});
        S2LatLng cc = S2LatLng::FromDegrees(lat, lon);
        S2CellId cell(cc.ToPoint());
        EXPECT_EQ(cell.level(), S2::kMaxCellLevel);

        LocalDocumentId rev(counter++);
        index.emplace(cell, rev);
        docs.emplace(rev, cc);
      }
    }
    EXPECT_EQ(65341, counter);
    EXPECT_EQ(docs.size(), counter);
    EXPECT_EQ(index.size(), counter);

    params.sorted = true;
    params.origin = S2LatLng::FromDegrees(0, 0);
  }
};

TEST_F(SimpleNearQueriesTest, query_all_sorted_ascending) {
  params.ascending = true;
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, SIZE_MAX);
  std::set<LocalDocumentId> unique;
  ASSERT_EQ(result.size(), counter);

  double lastRad = 0;
  for (LocalDocumentId rev : result) {
    // check that we get every document exactly once
    ASSERT_EQ(unique.find(rev), unique.end());
    unique.insert(rev);

    // check sort order
    S2LatLng const& cords = docs.at(rev);
    S2Point pp = cords.ToPoint();
    double rad = near.origin().Angle(pp);
    bool eq = rad > lastRad || std::fabs(rad - lastRad) <= geo::kRadEps;
    ASSERT_TRUE(eq);  // rad >= lastRad
    lastRad = rad;
  }
  ASSERT_NE(lastRad, 0);
}

TEST_F(SimpleNearQueriesTest, query_all_sorted_ascending_with_limit) {
  params.ascending = true;
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 5);
  ASSERT_EQ(result.size(), 5);

  std::vector<S2LatLng> coords = convert(docs, result);
  std::sort(coords.begin(), coords.end());
  ASSERT_EQ(coords[0], S2LatLng::FromDegrees(-1, 0));
  ASSERT_EQ(coords[1], S2LatLng::FromDegrees(0, -1));
  ASSERT_EQ(coords[2], S2LatLng::FromDegrees(0, 0));
  ASSERT_EQ(coords[3], S2LatLng::FromDegrees(0, 1));
  ASSERT_EQ(coords[4], S2LatLng::FromDegrees(1, 0));
}

TEST_F(SimpleNearQueriesTest, query_sorted_ascending_with_limit_and_max_distance) {
  params.ascending = true;
  params.maxDistance = 111200.0;
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
  ASSERT_EQ(result.size(), 5);

  std::vector<S2LatLng> coords = convert(docs, result);
  std::sort(coords.begin(), coords.end());
  ASSERT_EQ(coords[0], S2LatLng::FromDegrees(-1, 0));
  ASSERT_EQ(coords[1], S2LatLng::FromDegrees(0, -1));
  ASSERT_EQ(coords[2], S2LatLng::FromDegrees(0, 0));
  ASSERT_EQ(coords[3], S2LatLng::FromDegrees(0, 1));
  ASSERT_EQ(coords[4], S2LatLng::FromDegrees(1, 0));
}

TEST_F(SimpleNearQueriesTest, query_sorted_ascending_with_different_initial_delta) {
  params.ascending = true;
  params.maxDistance = 111200;
  AscIterator near(std::move(params));

  near.estimateDensity(S2LatLng::FromDegrees(0, 1).ToPoint());

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
  ASSERT_EQ(result.size(), 5);

  std::vector<S2LatLng> coords = convert(docs, result);
  std::sort(coords.begin(), coords.end());
  ASSERT_EQ(coords[0], S2LatLng::FromDegrees(-1, 0));
  ASSERT_EQ(coords[1], S2LatLng::FromDegrees(0, -1));
  ASSERT_EQ(coords[2], S2LatLng::FromDegrees(0, 0));
  ASSERT_EQ(coords[3], S2LatLng::FromDegrees(0, 1));
  ASSERT_EQ(coords[4], S2LatLng::FromDegrees(1, 0));
}

TEST_F(SimpleNearQueriesTest, query_all_sorted_descending) {
  params.ascending = false;
  DescIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, SIZE_MAX);
  std::set<LocalDocumentId> unique;
  ASSERT_EQ(result.size(), counter);

  double lastRad = geo::kEarthRadiusInMeters;
  for (LocalDocumentId rev : result) {
    // check that we get every document exactly once
    ASSERT_EQ(unique.find(rev), unique.end());
    unique.insert(rev);

    // check sort order
    S2LatLng const& coords = docs.at(rev);
    double rad = near.origin().Angle(coords.ToPoint());
    bool eq = rad < lastRad || std::fabs(rad - lastRad) <= geo::kRadEps;
    ASSERT_TRUE(eq);  // rad <= lastRad
    lastRad = rad;
  }
  ASSERT_EQ(lastRad, 0);
}

TEST_F(SimpleNearQueriesTest, query_all_sorted_descending_with_limit) {
  params.ascending = false;
  DescIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 5);
  std::set<LocalDocumentId> unique;
  ASSERT_EQ(result.size(), 5);

  std::vector<S2LatLng> coords = convert(docs, result);
  // [0,180], [0,-180] in any order
  for (size_t i = 0; i < 2; i++) {
    ASSERT_EQ(coords[i].lat().degrees(), 0.0);
    ASSERT_EQ(std::abs(coords[i].lng().degrees()), 180.0);
  }
}

TEST_F(SimpleNearQueriesTest, query_all_sorted_descending_with_limit_and_max_distance) {
  params.ascending = false;
  params.maxDistance = 111200;
  DescIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 1000);
  std::set<LocalDocumentId> unique;
  ASSERT_EQ(result.size(), 5);

  std::vector<S2LatLng> coords = convert(docs, result);
  ASSERT_EQ(coords[4], S2LatLng::FromDegrees(0, 0));

  for (size_t i = 0; i < 4; i++) {
    double lat = std::abs(coords[i].lat().degrees()),
           lng = std::abs(coords[i].lng().degrees());
    ASSERT_EQ(lat + lng, 1);  // lat == 1 => lng == 0, etc
  }
}

/* second main batch of tests                      */
/* insert 10 x 10 array of points near south pole  */
/* then do some searches, results checked against  */
/* the same run with full table scan               */

class QueryPointAroundTest : public ::testing::Test {
protected:
  index_t index;
  coords_t docs;
  size_t counter;
  geo::QueryParams params;

  QueryPointAroundTest() : counter(0) {
    double lat = -89.0;
    for (int i = 0; i < 10; i++) {
      double lon = 17.0;
      for (int j = 0; j < 10; j++) {
        // geocol.insert({lat,lon});
        S2LatLng cc = S2LatLng::FromDegrees(lat, lon);

        S2CellId cell(cc);
        EXPECT_EQ(cell.level(), S2::kMaxCellLevel);

        LocalDocumentId rev(counter++);
        index.emplace(cell, rev);
        docs.emplace(rev, cc);
        lon += 1.0;
      }
      lat += 1.0;
    }

    params.sorted = true;
    params.ascending = true;
  }

  void checkResult(S2Point const& origin, std::vector<LocalDocumentId> const& result) {
    double lastRad = 0;
    for (LocalDocumentId const& rev : result) {
      // check sort order
      S2LatLng const& cords = docs.at(rev);
      double rad = origin.Angle(cords.ToPoint());
      ASSERT_TRUE(rad >= lastRad);
      lastRad = rad;
    }
    ASSERT_NE(lastRad, 0);
  };
};

TEST_F(QueryPointAroundTest, southpole_1) {
  params.origin = S2LatLng::FromDegrees(-83.2, 19.2);
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 7);
  ASSERT_EQ(result.size(), 7);
  checkResult(near.origin(), result);
}

TEST_F(QueryPointAroundTest, southpole_2) {
  params.origin = S2LatLng::FromDegrees(-83.2, 19.2);
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 110);
  ASSERT_EQ(result.size(), 100);
  checkResult(near.origin(), result);
}

TEST_F(QueryPointAroundTest, southpole_3) {
  params.origin = S2LatLng::FromDegrees(-89.9, 0);
  AscIterator near(std::move(params));

  std::vector<LocalDocumentId> result = nearSearch(index, docs, near, 110);
  ASSERT_EQ(result.size(), 100);
  checkResult(near.origin(), result);
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

class QueryPointsContainedInTest : public ::testing::Test {
protected:
  index_t index;
  coords_t docs;
  size_t counter;
  geo::QueryParams params;

  QueryPointsContainedInTest() : counter(0) {
    // add some entries to it
    for (double lat = -40; lat <= 40; ++lat) {
      for (double lon = -40; lon <= 40; ++lon) {
        // geocol.insert({lat,lon});
        S2LatLng cc = S2LatLng::FromDegrees(lat, lon);
        S2CellId cell(cc.ToPoint());
        EXPECT_EQ(cell.level(), S2::kMaxCellLevel);

        LocalDocumentId rev(counter++);
        index.emplace(cell, rev);
        docs.emplace(rev, cc);
      }
    }
    EXPECT_EQ(6561, counter);
    EXPECT_EQ(docs.size(), counter);
    EXPECT_EQ(index.size(), counter);

    params.sorted = true;
    params.ascending = true;
    params.filterType = geo::FilterType::CONTAINS;
  }

  void checkResult(std::vector<LocalDocumentId> const& result,
                   std::vector<std::pair<double, double>> expected) {
    ASSERT_EQ(result.size(), expected.size());

    std::vector<std::pair<double, double>> latLngResult;
    for (LocalDocumentId const& rev : result) {
      // check sort order
      S2LatLng const& cords = docs.at(rev);
      latLngResult.emplace_back(cords.lat().degrees(), cords.lng().degrees());
    }

    std::sort(latLngResult.begin(), latLngResult.end());
    std::sort(expected.begin(), expected.end());
    auto it = latLngResult.begin();
    auto it2 = expected.begin();
    for (; it != latLngResult.end(); it++) {
      double diff = std::fabs(std::max(it->first - it2->first, it->second - it2->second));
      ASSERT_TRUE(diff < 0.00001);
      it2++;
    }
  };
};

TEST_F(QueryPointsContainedInTest, polygon) {
  auto polygon = createBuilder(R"=({"type": "Polygon", "coordinates":
                                 [[[-11.5, 23.5], [-6, 26], [-10.5, 26.1], [-11.5, 23.5]]]})=");

  geo::geojson::parsePolygon(polygon->slice(), params.filterShape);
  params.filterShape.updateBounds(params);

  AscIterator near(std::move(params));
  checkResult(nearSearch(index, docs, near, 10000), {{24.0, -11.0},
                                                     {25.0, -10.0},
                                                     {25.0, -9.0},
                                                     {26.0, -10.0},
                                                     {26.0, -9.0},
                                                     {26.0, -8.0},
                                                     {26.0, -7.0},
                                                     {26.0, -6.0}});
}

TEST_F(QueryPointsContainedInTest, rectangle) {
  auto rect = createBuilder(R"=({"type": "Polygon", "coordinates":[[[0,0],[1.5,0],[1.5,1.5],[0,1.5],[0,0]]]})=");
  geo::geojson::parsePolygon(rect->slice(), params.filterShape);
  ASSERT_EQ(params.filterShape.type(), geo::ShapeContainer::Type::S2_LATLNGRECT);
  params.filterShape.updateBounds(params);

  AscIterator near(std::move(params));
  checkResult(nearSearch(index, docs, near, 10000),
              {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
}
