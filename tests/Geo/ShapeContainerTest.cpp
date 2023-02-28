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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <s2/s2latlng.h>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

#include "Aql/VelocyPackHelper.h"

#include "Basics/Common.h"
#include "Basics/error.h"
#include "Basics/Exceptions.h"

#include "Geo/ShapeContainer.h"

#include "Geo/Ellipsoid.h"
#include "Geo/GeoJson.h"
#include "Geo/GeoParams.h"
#include "Geo/ShapeContainer.h"

#include <iostream>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

namespace {
double AcceptableDistanceError = 30.0;
}

namespace {
double distance(double degreesDiffLat, double degreesDiffLng) {
  using namespace arangodb::geo;
  double radLat = M_PI * degreesDiffLat / 180.0;
  double radLng = M_PI * degreesDiffLng / 180.0;
  double distRad = std::sqrt(std::pow(radLat, 2) + std::pow(radLng, 2));
  return distRad * kEarthRadiusInMeters;
}
}  // namespace

namespace {
bool pointsEqual(S2Point const& a, S2Point const& b) {
  bool equal = (a.Angle(b) * arangodb::geo::kEarthRadiusInMeters) <=
               AcceptableDistanceError;
  if (!equal) {
    // std::cout << "EXPECTING EQUAL POINTS, GOT " <<
    // S2LatLng(a).ToStringInDegrees()
    //           << " AND " << S2LatLng(b).ToStringInDegrees() << " AT DISTANCE
    //           "
    //           << (a.Angle(b) * arangodb::geo::kEarthRadiusInMeters);
  }
  return equal;
}
}  // namespace

namespace {
bool pointsEqual(S2LatLng const& a, S2LatLng const& b) {
  return ::pointsEqual(a.ToPoint(), b.ToPoint());
}
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

namespace arangodb {
namespace geo {

#define NOT_IMPL_EXC(expr)                              \
  do {                                                  \
    try {                                               \
      expr;                                             \
      ASSERT_TRUE(false);                               \
    } catch (arangodb::basics::Exception const& exc) {  \
      ASSERT_EQ(exc.code(), TRI_ERROR_NOT_IMPLEMENTED); \
    }                                                   \
  } while (false)

class ShapeContainerTest : public ::testing::Test {
 protected:
  using ShapeType = arangodb::geo::ShapeContainer::Type;

  ShapeContainer shape;
  ShapeContainer coord;
  VPackBuilder builder;
};

TEST_F(ShapeContainerTest, empty_region) {
  ASSERT_EQ(ShapeType::EMPTY, shape.type());
  ASSERT_TRUE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());
}

TEST_F(ShapeContainerTest, valid_point_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(1.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POINT, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(1.0, 0.0).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(
      ::distance(1.0, 0.0) ==
      shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));

  geo::Ellipsoid const& e = geo::WGS84_ELLIPSOID;
  double dist = shape.distanceFromCentroid(
      S2LatLng::FromDegrees(-24.993289, 151.960336).ToPoint(), e);
  ASSERT_LE(std::fabs(dist - 16004725.0), 0.5);

  // equality works
  ASSERT_TRUE(shape.equals(shape));
  coord.reset(S2LatLng::FromDegrees(1.0, 0.0).ToPoint());
  ASSERT_TRUE(shape.equals(coord));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(coord));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.0, 0.0).ToPoint());
  ASSERT_FALSE(shape.intersects(coord));

  ASSERT_EQ(shape.area(e), 0.0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_EQ(S2LatLng::FromDegrees(1.0, 0.0), qp.origin);
  ASSERT_EQ(0.0, qp.maxDistance);
}

TEST_F(ShapeContainerTest, valid_multipoint_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(1.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(1.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_MULTIPOINT, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(
      ::AcceptableDistanceError >=
      shape.distanceFromCentroid(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.5) -
                       shape.distanceFromCentroid(
                           S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(shape));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.0, 0.0).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(1.0, 0.0).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(1.0, 1.0).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(0.0, 1.0).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.5, 0.5).ToPoint());
  ASSERT_FALSE(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(2.0, 2.0).ToPoint());
  ASSERT_FALSE(shape.intersects(coord));

  ASSERT_EQ(shape.area(geo::WGS84_ELLIPSOID), 0);
  ASSERT_EQ(shape.area(geo::SPHERE), 0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.5) - qp.maxDistance));
}

TEST_F(ShapeContainerTest, valid_linestring_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(1.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(1.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POLYLINE, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(
                  S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.66666667) -
                       shape.distanceFromCentroid(
                           S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(shape));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.0, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.0, 0.5).ToPoint());
  NOT_IMPL_EXC(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(0.5, 0.5).ToPoint());
  NOT_IMPL_EXC(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(2.0, 2.0).ToPoint());
  NOT_IMPL_EXC(shape.intersects(coord));

  ASSERT_EQ(shape.area(geo::WGS84_ELLIPSOID), 0);
  ASSERT_EQ(shape.area(geo::SPHERE), 0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.66666667), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.66666667) - qp.maxDistance));
}

TEST_F(ShapeContainerTest, valid_multilinestring_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(2.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(2.0));
        point->add(VPackValue(2.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(2.0));
      }
    }
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_MULTIPOLYLINE, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(
                  S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.91666666666) -
                       shape.distanceFromCentroid(
                           S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(shape));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.5, 0.5).ToPoint());
  NOT_IMPL_EXC(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(3.0, 3.0).ToPoint());
  NOT_IMPL_EXC(shape.intersects(coord));

  ASSERT_EQ(shape.area(geo::WGS84_ELLIPSOID), 0);
  ASSERT_EQ(shape.area(geo::SPHERE), 0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(
      ::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(1.5, 1.91666666666) - qp.maxDistance));
}

TEST_F(ShapeContainerTest, valid_polygon_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POLYGON, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_TRUE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(
      ::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint(),
                    shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(
                  S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.33333333, 0.33333333) -
                       shape.distanceFromCentroid(
                           S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(shape));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(0.99, 0.01).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));
  coord.reset(S2LatLng::FromDegrees(0.01, 0.99).ToPoint());
  ASSERT_TRUE(shape.intersects(coord));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  coord.reset(S2LatLng::FromDegrees(1.0, 1.0).ToPoint());
  ASSERT_FALSE(shape.intersects(coord));

  ASSERT_NEAR(shape.area(geo::SPHERE), 6182469722.73085, 1000);
  ASSERT_NEAR(shape.area(geo::WGS84_ELLIPSOID), 6154854786.72143, 1000);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(
      ::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.66666667, 0.66666667) - qp.maxDistance));
}

TEST_F(ShapeContainerTest, polygon_area_test) {
  // approx australia
  const char* json =
      "{\"type\": \"Polygon\", \"coordinates\": [[[125, -15],[113, -22],[117, "
      "-37],[130, -33],"
      "[148, -39],[154, -27],[144, -15],[125, -15]]]}";

  auto builder = VPackParser::fromJson(json);
  VPackSlice vpack = builder->slice();

  ASSERT_TRUE(json::parseRegion(vpack, shape, false).ok());

  // tolerance 50.000 km^2 vs 7.692.000 km^2 total
  ASSERT_NEAR(shape.area(geo::SPHERE), 7800367402432, 50000000000);
  ASSERT_NEAR(shape.area(geo::WGS84_ELLIPSOID), 7800367402432, 50000000000);
}

using namespace arangodb::tests;

TEST_F(ShapeContainerTest, compare_new_legacy) {
  // Check that legacy parsing detects LngLatRects and new style finds .
  // a polygon Also check containment of a point to make sure          .
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[[10, 10], [20, 10], [20, 20], [10, 20], [10, 10]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  Result res = json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(ShapeType::S2_POLYGON, shape.type());
  S2Point point = S2LatLng::FromDegrees(10.0, 15.0).ToPoint();
  ASSERT_FALSE(shape.contains(point));

  res = json::parseRegion(polyS, shape, true);
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(ShapeType::S2_LATLNGRECT, shape.type());
  ASSERT_TRUE(shape.contains(point));

  // Check that legacy parsing normalizes polygons whereas new style allows
  // for polygons covering more than half of the world:
  poly = R"({
    "type": "Polygon",
    "coordinates": [[[10, 10], [15, 15], [20, 10], [15, 5], [10, 10]]]
  })"_vpack;
  // This polygon contains what is to the left of the polyline, which is
  // the complement of a small shape around [15, 10]!
  polyS = velocypack::Slice(poly->data());
  res = json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(ShapeType::S2_POLYGON, shape.type());
  point = S2LatLng::FromDegrees(10.0, 15.0).ToPoint();
  ASSERT_FALSE(shape.contains(point));

  res = json::parseRegion(polyS, shape, true);
  ASSERT_TRUE(res.ok());
  ASSERT_EQ(ShapeType::S2_POLYGON, shape.type());
  ASSERT_TRUE(shape.contains(point));
}

class ShapeContainerTest2 : public ::testing::Test {
 protected:
  using ShapeType = arangodb::geo::ShapeContainer::Type;

  ShapeContainer point;
  ShapeContainer multipoint;
  ShapeContainer line;
  ShapeContainer multiline;
  ShapeContainer poly;
  ShapeContainer multipoly;
  ShapeContainer rect;
  ShapeContainer line2;
  ShapeContainer rects[4];
  ShapeContainer nearly[4];  // nearly rects, but not quite

  ShapeContainerTest2() {
    auto builder = VPackParser::fromJson(R"=(
      { "type": "Point",
      "coordinates": [ 6.537, 50.332 ]
      })=");
    json::parseRegion(builder->slice(), point, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "MultiPoint",
        "coordinates": [ [ 6.537, 50.332 ], [ 6.537, 50.376 ] ]
      })=");
    json::parseRegion(builder->slice(), multipoint, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "LineString",
        "coordinates": [ [ 6.537, 50.332 ], [ 6.537, 50.376 ] ]
      })=");
    json::parseRegion(builder->slice(), line, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "MultiLineString",
        "coordinates": [ [ [ 6.537, 50.332 ], [ 6.537, 50.376 ] ],
                         [ [ 6.621, 50.332 ], [ 6.621, 50.376 ] ] ]
      })=");
    json::parseRegion(builder->slice(), multiline, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [6,50], [7.5,50], [7.5,52], [6,51], [6,50] ] ]
      })=");
    json::parseRegion(builder->slice(), poly, false);
    // Note that internally, a multipolygon is just a special polygon with
    // holes, which could have been initialized as polygon, too!
    builder = VPackParser::fromJson(R"=(
      { "type": "MultiPolygon",
        "coordinates": [ [ [ [6.501,50], [7.5,50], [7.5,51],
                             [6.501,51], [6.501,50] ] ],
                         [ [ [6,50], [6.5,50], [6.5,51], [6,51], [6,50] ] ] ]
      })=");
    json::parseRegion(builder->slice(), multipoly, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [6,50], [7.5,50], [7.5,51], [6,51], [6,50] ] ]
      })=");
    json::parseRegion(builder->slice(), rect, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "LineString",
        "coordinates": [ [ 5.437, 50.332 ], [ 7.537, 50.376 ] ]
      })=");
    json::parseRegion(builder->slice(), line2, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [1.0,1.0], [4.0,1.0], [4.0,4.0], [1.0,4.0], [1.0,1.0] ] ]
      })=");
    json::parseRegion(builder->slice(), rects[0], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [2.0,2.0], [3.0,2.0], [3.0,3.0], [2.0,3.0], [2.0,2.0] ] ]
      })=");
    json::parseRegion(builder->slice(), rects[1], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [2.0,2.0], [5.0,2.0], [5.0,5.0], [2.0,5.0], [2.0,2.0] ] ]
      })=");
    json::parseRegion(builder->slice(), rects[2], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [7.0,7.0], [8.0,7.0], [8.0,8.0], [7.0,8.0], [7.0,7.0] ] ]
      })=");
    json::parseRegion(builder->slice(), rects[3], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [1.0,1.0], [4.0,1.0], [4.1,4.1], [1.0,4.0], [1.0,1.0] ] ]
      })=");
    json::parseRegion(builder->slice(), nearly[0], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [2.0,2.0], [3.0,2.0], [3.1,3.1], [2.0,3.0], [2.0,2.0] ] ]
      })=");
    json::parseRegion(builder->slice(), nearly[1], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [2.0,2.0], [5.0,2.0], [5.1,5.1], [2.0,5.0], [2.0,2.0] ] ]
      })=");
    json::parseRegion(builder->slice(), nearly[2], false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [7.0,7.0], [8.0,7.0], [8.1,8.1], [7.0,8.0], [7.0,7.0] ] ]
      })=");
    json::parseRegion(builder->slice(), nearly[3], false);
  }
};

// point      TT--TTT
// multipoint TT----T
// line       --TTTTT
// multiline  -------
// poly       TTTTTTT
// multipoly  TTTTTTT
// rect       TTTTTTT

TEST_F(ShapeContainerTest2, intersections_point) {
  // All 7 with each other:
  // The ones which are commented out run into assertion failures right now:
  NOT_IMPL_EXC(point.intersects(line));
  NOT_IMPL_EXC(point.intersects(multiline));
  ASSERT_TRUE(point.intersects(point));
  ASSERT_TRUE(point.intersects(multipoint));
  ASSERT_TRUE(point.intersects(poly));
  ASSERT_TRUE(point.intersects(multipoly));
  ASSERT_TRUE(point.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_multipoint) {
  NOT_IMPL_EXC(multipoint.intersects(line));
  NOT_IMPL_EXC(multipoint.intersects(multiline));
  ASSERT_TRUE(multipoint.intersects(point));
  ASSERT_TRUE(multipoint.intersects(multipoint));
  ASSERT_TRUE(multipoint.intersects(poly));
  ASSERT_TRUE(multipoint.intersects(multipoly));
  ASSERT_TRUE(multipoint.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_line) {
  // Note that in the S2 geo library intersections of points and lines
  // will always return false, since they are not well-defined numerically!
  NOT_IMPL_EXC(line.intersects(point));
  NOT_IMPL_EXC(line.intersects(multipoint));
  ASSERT_TRUE(line.intersects(line));
  ASSERT_TRUE(line.intersects(multiline));
  ASSERT_TRUE(line.intersects(poly));
  ASSERT_TRUE(line.intersects(multipoly));
  ASSERT_TRUE(line.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_multiline) {
  // Note that in the S2 geo library intersections of points and lines
  // will always return false, since they are not well-defined numerically!
  NOT_IMPL_EXC(multiline.intersects(point));
  NOT_IMPL_EXC(multiline.intersects(multipoint));
  ASSERT_TRUE(multiline.intersects(line));
  ASSERT_TRUE(multiline.intersects(multiline));
  ASSERT_TRUE(multiline.intersects(poly));
  ASSERT_TRUE(multiline.intersects(multipoly));
  ASSERT_TRUE(multiline.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_poly) {
  ASSERT_TRUE(poly.intersects(point));
  ASSERT_TRUE(poly.intersects(multipoint));
  ASSERT_TRUE(poly.intersects(line));
  ASSERT_TRUE(poly.intersects(multiline));
  ASSERT_TRUE(poly.intersects(poly));
  ASSERT_TRUE(poly.intersects(multipoly));
  ASSERT_TRUE(poly.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_multipoly) {
  ASSERT_TRUE(multipoly.intersects(point));
  ASSERT_TRUE(multipoly.intersects(multipoint));
  ASSERT_TRUE(multipoly.intersects(line));
  ASSERT_TRUE(multipoly.intersects(multiline));
  ASSERT_TRUE(multipoly.intersects(poly));
  ASSERT_TRUE(multipoly.intersects(multipoly));
  ASSERT_TRUE(multipoly.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_rect) {
  ASSERT_TRUE(rect.intersects(point));
  ASSERT_TRUE(rect.intersects(multipoint));
  ASSERT_TRUE(rect.intersects(line));
  ASSERT_TRUE(rect.intersects(multiline));
  ASSERT_TRUE(rect.intersects(poly));
  ASSERT_TRUE(rect.intersects(multipoly));
  ASSERT_TRUE(rect.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_special) {
  ASSERT_TRUE(rect.intersects(line2));
  ASSERT_TRUE(line2.intersects(rect));
}

TEST_F(ShapeContainerTest2, intersections_latlntrects) {
  ASSERT_TRUE(rects[0].intersects(rects[0]));
  ASSERT_TRUE(rects[0].intersects(rects[1]));
  ASSERT_TRUE(rects[1].intersects(rects[0]));
  ASSERT_TRUE(rects[0].intersects(rects[2]));
  ASSERT_TRUE(rects[2].intersects(rects[0]));
  ASSERT_TRUE(rects[1].intersects(rects[2]));
  ASSERT_TRUE(rects[2].intersects(rects[1]));
  ASSERT_FALSE(rects[0].intersects(rects[3]));
  ASSERT_FALSE(rects[3].intersects(rects[0]));
}

TEST_F(ShapeContainerTest2, intersections_latlntrects_nearly) {
  ASSERT_TRUE(rects[0].intersects(nearly[0]));
  ASSERT_TRUE(rects[0].intersects(nearly[1]));
  ASSERT_TRUE(rects[1].intersects(nearly[0]));
  ASSERT_TRUE(rects[0].intersects(nearly[2]));
  ASSERT_TRUE(rects[2].intersects(nearly[0]));
  ASSERT_TRUE(rects[1].intersects(nearly[2]));
  ASSERT_TRUE(rects[2].intersects(nearly[1]));
  ASSERT_FALSE(rects[0].intersects(nearly[3]));
  ASSERT_FALSE(rects[3].intersects(nearly[0]));
}

TEST_F(ShapeContainerTest2, intersections_nearly_latlntrects) {
  ASSERT_TRUE(nearly[0].intersects(rects[0]));
  ASSERT_TRUE(nearly[0].intersects(rects[1]));
  ASSERT_TRUE(nearly[1].intersects(rects[0]));
  ASSERT_TRUE(nearly[0].intersects(rects[2]));
  ASSERT_TRUE(nearly[2].intersects(rects[0]));
  ASSERT_TRUE(nearly[1].intersects(rects[2]));
  ASSERT_TRUE(nearly[2].intersects(rects[1]));
  ASSERT_FALSE(nearly[0].intersects(rects[3]));
  ASSERT_FALSE(nearly[3].intersects(rects[0]));
}

class ShapeContainerTest3 : public ::testing::Test {
 protected:
  using ShapeType = arangodb::geo::ShapeContainer::Type;

  ShapeContainer line;
  ShapeContainer multiline;
  ShapeContainer poly;

  ShapeContainerTest3() {
    auto builder = VPackParser::fromJson(R"=(
      { "type": "LineString",
        "coordinates": [ [ 5, 5 ], [ 6, 6 ] ]
      })=");
    json::parseRegion(builder->slice(), line, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "MultiLineString",
        "coordinates": [ [ [ 5, 5 ], [ 6, 6 ] ],
                         [ [ 7, 7 ], [ 8, 8 ] ] ]
      })=");
    json::parseRegion(builder->slice(), multiline, false);
    builder = VPackParser::fromJson(R"=(
      { "type": "Polygon",
        "coordinates": [ [ [0,0], [10,0], [10,10], [0,10], [0,0] ] ]
      })=");
    json::parseRegion(builder->slice(), poly, false);
  }
};

TEST_F(ShapeContainerTest3, contains) {
  ASSERT_TRUE(poly.contains(line));
  ASSERT_TRUE(poly.contains(multiline));
}
}  // namespace geo
}  // namespace arangodb
