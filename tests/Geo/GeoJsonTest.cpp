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
#include <s2/s2loop.h>
#include <s2/s2point.h>
#include <s2/s2polyline.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Geo/ShapeContainer.h"

#include "Geo/GeoJson.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("Invalid GeoJSON input", "[geo][s2index]") {
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;
  using arangodb::geo::geojson::Type;
  using arangodb::geo::geojson::type;
  using arangodb::geo::geojson::parsePoint;
  using arangodb::geo::geojson::parsePoints;
  using arangodb::geo::geojson::parseLinestring;
  using arangodb::geo::geojson::parseMultiLinestring;
  using arangodb::geo::geojson::parseRegion;
  using arangodb::geo::geojson::parsePolygon;
  using arangodb::geo::geojson::parseLoop;
  using arangodb::geo::ShapeContainer;

  ShapeContainer shape;
  S2LatLng point;
  std::vector<S2Point> points;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;

  VPackBuilder builder;

  SECTION("Empty object") {
    {
      ObjectBuilder object(&builder);
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::UNKNOWN == type(vpack));

    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parsePoints(vpack, true, points).is(TRI_ERROR_BAD_PARAMETER));

    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));

    REQUIRE(parseRegion(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Wrong type, expecting Point") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Wrong type, expecting Linestring") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Wrong type, expecting MultiLinestring") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Wrong type, expecting Polygon") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Point, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Point, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Point, too few coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Point, too many coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Point, multiple points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiPoint, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiPoint, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
      ArrayBuilder coords(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiPoint, numbers instead of points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiPoint, extra numbers in bad points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
      ArrayBuilder coords(&builder, "coordinates");
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  }

}

TEST_CASE("Valid GeoJSON input", "[geo][s2index]") {
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;
  using arangodb::geo::geojson::Type;
  using arangodb::geo::geojson::type;
  using arangodb::geo::geojson::parsePoint;
  using arangodb::geo::geojson::parsePoints;
  using arangodb::geo::geojson::parseLinestring;
  using arangodb::geo::geojson::parseMultiLinestring;
  using arangodb::geo::geojson::parseRegion;
  using arangodb::geo::geojson::parsePolygon;
  using arangodb::geo::geojson::parseLoop;
  using arangodb::geo::ShapeContainer;

  ShapeContainer shape;
  S2LatLng point;
  std::vector<S2Point> points;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;

  VPackBuilder builder;

  SECTION("Valid point") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parsePoint(vpack, point).ok());
  }
}
