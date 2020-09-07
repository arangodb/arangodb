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
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"

#include "Geo/ShapeContainer.h"

#include "Geo/Ellipsoid.h"
#include "Geo/GeoJson.h"
#include "Geo/GeoParams.h"
#include "Geo/ShapeContainer.h"
#include "Geo/Shapes.h"

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
  double radLat = kPi * degreesDiffLat / 180.0;
  double radLng = kPi * degreesDiffLng / 180.0;
  double distRad = std::sqrt(std::pow(radLat, 2) + std::pow(radLng, 2));
  return distRad * kEarthRadiusInMeters;
}
}  // namespace

namespace {
bool pointsEqual(S2Point const& a, S2Point const& b) {
  bool equal = (a.Angle(b) * arangodb::geo::kEarthRadiusInMeters) <= AcceptableDistanceError;
  if (!equal) {
    // std::cout << "EXPECTING EQUAL POINTS, GOT " << S2LatLng(a).ToStringInDegrees()
    //           << " AND " << S2LatLng(b).ToStringInDegrees() << " AT DISTANCE "
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

class ShapeContainerTest : public ::testing::Test {
 protected:
  using ShapeType = arangodb::geo::ShapeContainer::Type;

  ShapeContainer shape;
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

  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POINT, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(1.0, 0.0).ToPoint(), shape.centroid()));
  ASSERT_TRUE(::distance(1.0, 0.0) ==
              shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  
  geo::Ellipsoid const& e = geo::WGS84_ELLIPSOID;
  double dist = shape.distanceFromCentroid(S2LatLng::FromDegrees(-24.993289, 151.960336).ToPoint(), e);
  ASSERT_LE(std::fabs(dist - 16004725.0), 0.5);

  // equality works
  ASSERT_TRUE(shape.equals(&shape));
  Coordinate coord(1.0, 0.0);
  ASSERT_TRUE(shape.equals(&coord));
  ASSERT_TRUE(shape.equals(1.0, 0.0));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  
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

  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_MULTIPOINT, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5).ToPoint(), shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.5) -
                       shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(&shape));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  
  ASSERT_EQ(shape.area(geo::WGS84_ELLIPSOID), 0);
  ASSERT_EQ(shape.area(geo::SPHERE), 0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >= std::abs(::distance(0.5, 0.5) - qp.maxDistance));
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

  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POLYLINE, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.66666667) -
                       shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(&shape));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.0, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(0.0, 0.5).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  
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

  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_MULTIPOLYLINE, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_FALSE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.5, 0.91666666666) -
                       shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(&shape));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
  
  ASSERT_EQ(shape.area(geo::WGS84_ELLIPSOID), 0);
  ASSERT_EQ(shape.area(geo::SPHERE), 0);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666), qp.origin));
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

  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // properties match
  ASSERT_EQ(ShapeType::S2_POLYGON, shape.type());
  ASSERT_FALSE(shape.empty());
  ASSERT_TRUE(shape.isAreaType());

  // location utilities
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint(),
                            shape.centroid()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              shape.distanceFromCentroid(S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint()));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.33333333, 0.33333333) -
                       shape.distanceFromCentroid(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

  // equality works
  ASSERT_TRUE(shape.equals(&shape));

  // contains what it should
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
  ASSERT_TRUE(shape.intersects(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));

  // doesn't contain what it shouldn't
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_FALSE(shape.intersects(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  
  ASSERT_NEAR(shape.area(geo::SPHERE), 6182469722.73085, 1000);
  ASSERT_NEAR(shape.area(geo::WGS84_ELLIPSOID), 6154854786.72143, 1000);

  // query params
  QueryParams qp;
  shape.updateBounds(qp);
  ASSERT_TRUE(::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333), qp.origin));
  ASSERT_TRUE(::AcceptableDistanceError >=
              std::abs(::distance(0.66666667, 0.66666667) - qp.maxDistance));
}

TEST_F(ShapeContainerTest, polygon_area_test) {
  // approx australia
  const char* json = "{\"type\": \"Polygon\", \"coordinates\": [[[125, -15],[113, -22],[117, -37],[130, -33],"
                     "[148, -39],[154, -27],[144, -15],[125, -15]]]}";
  
  auto builder = VPackParser::fromJson(json);
  VPackSlice vpack = builder->slice();
  
  ASSERT_TRUE(geojson::parseRegion(vpack, shape).ok());

  // tolerance 50.000 km^2 vs 7.692.000 km^2 total
  ASSERT_NEAR(shape.area(geo::SPHERE), 7800367402432, 50000000000);
  ASSERT_NEAR(shape.area(geo::WGS84_ELLIPSOID), 7800367402432, 50000000000);
}
  
  
  
}}
