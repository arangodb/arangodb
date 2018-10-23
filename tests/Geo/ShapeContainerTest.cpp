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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include <s2/s2latlng.h>

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Geo/ShapeContainer.h"

#include "Geo/GeoJson.h"
#include "Geo/GeoParams.h"
#include "Geo/ShapeContainer.h"

#include <iostream>

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

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
  bool equal = (a.Angle(b) * arangodb::geo::kEarthRadiusInMeters) <=
               AcceptableDistanceError;
  if (!equal) {
    std::cout << "EXPECTING EQUAL POINTS, GOT "
              << S2LatLng(a).ToStringInDegrees() << " AND "
              << S2LatLng(b).ToStringInDegrees() << " AT DISTANCE "
              << (a.Angle(b) * arangodb::geo::kEarthRadiusInMeters);
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

TEST_CASE("Shape container tests", "[geo][s2index]") {
  using arangodb::geo::Coordinate;
  using arangodb::geo::QueryParams;
  using arangodb::geo::ShapeContainer;
  using arangodb::geo::geojson::parseRegion;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;
  using ShapeType = arangodb::geo::ShapeContainer::Type;

  ShapeContainer shape;
  VPackBuilder builder;

  SECTION("Empty region") {
    REQUIRE(ShapeType::EMPTY == shape.type());
    REQUIRE(shape.empty());
    REQUIRE(!shape.isAreaType());
  }

  SECTION("Valid point as region") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(1.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseRegion(vpack, shape).ok());

    // properties match
    REQUIRE(ShapeType::S2_POINT == shape.type());
    REQUIRE(!shape.empty());
    REQUIRE(!shape.isAreaType());

    // location utilities
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(1.0, 0.0).ToPoint(),
                          shape.centroid()));
    REQUIRE(::distance(1.0, 0.0) ==
            shape.distanceFrom(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));

    // equality works
    REQUIRE(shape.equals(&shape));
    Coordinate coord(1.0, 0.0);
    REQUIRE(shape.equals(&coord));
    REQUIRE(shape.equals(1.0, 0.0));

    // contains what it should
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

    // doesn't contain what it shouldn't
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));

    // query params
    QueryParams qp;
    shape.updateBounds(qp);
    REQUIRE(S2LatLng::FromDegrees(1.0, 0.0) == qp.origin);
    REQUIRE(0.0 == qp.maxDistance);
  }

  SECTION("Valid MultiPoint as region") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
      ArrayBuilder points(&builder, "coordinates");
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseRegion(vpack, shape).ok());

    // properties match
    REQUIRE(ShapeType::S2_MULTIPOINT == shape.type());
    REQUIRE(!shape.empty());
    REQUIRE(!shape.isAreaType());

    // location utilities
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5).ToPoint(),
                          shape.centroid()));
    REQUIRE(::AcceptableDistanceError >=
            shape.distanceFrom(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(
                ::distance(0.5, 0.5) -
                shape.distanceFrom(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

    // equality works
    REQUIRE(shape.equals(&shape));

    // contains what it should
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));

    // doesn't contain what it shouldn't
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));

    // query params
    QueryParams qp;
    shape.updateBounds(qp);
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.5), qp.origin));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(::distance(0.5, 0.5) - qp.maxDistance));
  }

  SECTION("Valid Linestring as region") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
      ArrayBuilder points(&builder, "coordinates");
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseRegion(vpack, shape).ok());

    // properties match
    REQUIRE(ShapeType::S2_POLYLINE == shape.type());
    REQUIRE(!shape.empty());
    REQUIRE(!shape.isAreaType());

    // location utilities
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint(),
                          shape.centroid()));
    REQUIRE(
        ::AcceptableDistanceError >=
        shape.distanceFrom(S2LatLng::FromDegrees(0.5, 0.66666667).ToPoint()));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(
                ::distance(0.5, 0.66666667) -
                shape.distanceFrom(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

    // equality works
    REQUIRE(shape.equals(&shape));

    // doesn't contain what it shouldn't
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.0, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(0.0, 0.5).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));

    // query params
    QueryParams qp;
    shape.updateBounds(qp);
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.66666667), qp.origin));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(::distance(0.5, 0.66666667) - qp.maxDistance));
  }

  SECTION("Valid MultiLinestring as region") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(2.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(2.0));
          point->add(VPackValue(2.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(2.0));
        }
      }
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1.0));
          point->add(VPackValue(1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(1.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseRegion(vpack, shape).ok());

    // properties match
    REQUIRE(ShapeType::S2_MULTIPOLYLINE == shape.type());
    REQUIRE(!shape.empty());
    REQUIRE(!shape.isAreaType());

    // location utilities
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint(),
                          shape.centroid()));
    REQUIRE(::AcceptableDistanceError >=
            shape.distanceFrom(
                S2LatLng::FromDegrees(0.5, 0.91666666666).ToPoint()));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(
                ::distance(0.5, 0.91666666666) -
                shape.distanceFrom(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

    // equality works
    REQUIRE(shape.equals(&shape));

    // doesn't contain what it shouldn't
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));

    // query params
    QueryParams qp;
    shape.updateBounds(qp);
    REQUIRE(
        ::pointsEqual(S2LatLng::FromDegrees(0.5, 0.91666666666), qp.origin));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(::distance(1.5, 1.91666666666) - qp.maxDistance));
  }

  SECTION("Valid Polygon, as region") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder rings(&builder, "coordinates");
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseRegion(vpack, shape).ok());

    // properties match
    REQUIRE(ShapeType::S2_POLYGON == shape.type());
    REQUIRE(!shape.empty());
    REQUIRE(shape.isAreaType());

    // location utilities
    REQUIRE(
        ::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint(),
                      shape.centroid()));
    REQUIRE(::AcceptableDistanceError >=
            shape.distanceFrom(
                S2LatLng::FromDegrees(0.33333333, 0.33333333).ToPoint()));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(
                ::distance(0.33333333, 0.33333333) -
                shape.distanceFrom(S2LatLng::FromDegrees(0.0, 0.0).ToPoint())));

    // equality works
    REQUIRE(shape.equals(&shape));

    // contains what it should
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
    REQUIRE(shape.intersects(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));

    // doesn't contain what it shouldn't
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(!shape.intersects(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));

    // query params
    QueryParams qp;
    shape.updateBounds(qp);
    REQUIRE(::pointsEqual(S2LatLng::FromDegrees(0.33333333, 0.33333333),
                          qp.origin));
    REQUIRE(::AcceptableDistanceError >=
            std::abs(::distance(0.66666667, 0.66666667) - qp.maxDistance));
  }
}
