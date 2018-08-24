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
  using arangodb::geo::ShapeContainer;
  using arangodb::geo::geojson::parseLinestring;
  using arangodb::geo::geojson::parseLoop;
  using arangodb::geo::geojson::parseMultiLinestring;
  using arangodb::geo::geojson::parseMultiPoint;
  using arangodb::geo::geojson::parsePoint;
  using arangodb::geo::geojson::parsePolygon;
  using arangodb::geo::geojson::parseMultiPolygon;
  using arangodb::geo::geojson::parseRegion;
  using arangodb::geo::geojson::Type;
  using arangodb::geo::geojson::type;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;

  S2LatLng point;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;
  ShapeContainer shape;

  VPackBuilder builder;

  SECTION("Empty object") {
    { ObjectBuilder object(&builder); }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::UNKNOWN == type(vpack));

    REQUIRE(parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));

    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));

    REQUIRE(parseRegion(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  SECTION("Wrong type, expecting MultiPoint") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiPoint, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPoint"));
      ArrayBuilder coords(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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
    REQUIRE(parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Linestring, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Linestring, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
      ArrayBuilder points(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Linestring, numbers instead of points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
      ArrayBuilder points(&builder, "coordinates");
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Linestring, extra numbers in bad points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Linestring"));
      ArrayBuilder points(&builder, "coordinates");
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

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, numbers instead of lines") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
      lines->add(VPackValue(0.0));
      lines->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, numbers instead of points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
      {
        ArrayBuilder points(&builder);
        points->add(VPackValue(0.0));
        points->add(VPackValue(0.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, extra numbers in bad points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
      ArrayBuilder points(&builder);
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

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad MultiLinestring, points outside of line") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiLinestring"));
      ArrayBuilder lines(&builder, "coordinates");
      // don't open linestring, just add points directly
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad loop, object not array") {
    { ObjectBuilder object(&builder); }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad loop, empty array") {
    { ArrayBuilder object(&builder); }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad loop, numbers instead of points") {
    {
      ArrayBuilder points(&builder);
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad loop, extra numbers in bad points") {
    {
      ArrayBuilder points(&builder);
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

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad loop, full-GeoJSON input") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder points(&builder, "coordinates");
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder points(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, numbers instead of rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder points(&builder, "coordinates");
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, points instead of rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder rings(&builder, "coordinates");
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
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, extra numbers in bad points") {
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
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, too few points") {
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
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, not closed") {
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

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, non-nested rings") {
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
          point->add(VPackValue(1.0));
          point->add(VPackValue(1.0));
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
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  SECTION("Bad Polygon, outer ring not first") {
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
          point->add(VPackValue(1.0));
          point->add(VPackValue(1.0));
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }

  // ===========================
  
  
  SECTION("Bad MultiPolygon, no coordinates") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, no coordinates (empty)") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder points(&builder, "coordinates");
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, numbers instead of polygons") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder points(&builder, "coordinates");
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, numbers instead of rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      ArrayBuilder points(&builder);
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad Polygon, points instead of rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      ArrayBuilder rings(&builder);
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
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, extra numbers in bad points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      {
        ArrayBuilder rings(&builder);
        {
          ArrayBuilder points(&builder);
          {
            ArrayBuilder point(&builder);
            point->add(VPackValue(0.0));
            point->add(VPackValue(0.0));
            point->add(VPackValue(0.0));
          }
          {
            ArrayBuilder point(&builder);
            point->add(VPackValue(1.0));
            point->add(VPackValue(0.0));
            point->add(VPackValue(0.0));
          }
          {
            ArrayBuilder point(&builder);
            point->add(VPackValue(0.0));
            point->add(VPackValue(1.0));
            point->add(VPackValue(0.0));
          }
          {
            ArrayBuilder point(&builder);
            point->add(VPackValue(0.0));
            point->add(VPackValue(0.0));
            point->add(VPackValue(0.0));
          }
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, too few points") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder rings(&builder, "coordinates");
      ArrayBuilder polygons(&builder);
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
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, not closed") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder rings(&builder, "coordinates");
      ArrayBuilder polygons(&builder);
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
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, non-nested rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      ArrayBuilder rings(&builder);
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
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
          point->add(VPackValue(-1.0));
          point->add(VPackValue(0.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(-1.0));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
  
  SECTION("Bad MultiPolygon, outer ring not first") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      ArrayBuilder rings(&builder);
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
  }
}

TEST_CASE("Valid GeoJSON input", "[geo][s2index]") {
  using arangodb::geo::ShapeContainer;
  using arangodb::geo::geojson::parseLinestring;
  using arangodb::geo::geojson::parseLoop;
  using arangodb::geo::geojson::parseMultiLinestring;
  using arangodb::geo::geojson::parseMultiPoint;
  using arangodb::geo::geojson::parsePoint;
  using arangodb::geo::geojson::parsePolygon;
  using arangodb::geo::geojson::parseMultiPolygon;
  using arangodb::geo::geojson::parseRegion;
  using arangodb::geo::geojson::Type;
  using arangodb::geo::geojson::type;
  using arangodb::velocypack::ArrayBuilder;
  using arangodb::velocypack::ObjectBuilder;

  S2LatLng point;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;
  ShapeContainer shape;

  VPackBuilder builder;

  SECTION("Valid point") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Point"));
      ArrayBuilder coords(&builder, "coordinates");
      coords->add(VPackValue(0.0));
      coords->add(VPackValue(1.0));
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parsePoint(vpack, point).ok());
    REQUIRE(0.0 == point.lng().degrees());
    REQUIRE(1.0 == point.lat().degrees());
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

    REQUIRE(Type::POINT == type(vpack));
    REQUIRE(parseRegion(vpack, shape).ok());
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  }

  SECTION("Valid MultiPoint") {
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

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parseMultiPoint(vpack, shape).ok());

    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
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

    REQUIRE(Type::MULTI_POINT == type(vpack));
    REQUIRE(parseRegion(vpack, shape).ok());

    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
  }

  SECTION("Valid Linestring") {
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

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseLinestring(vpack, line).ok());

    REQUIRE(4 == line.num_vertices());
    REQUIRE(S2LatLng::FromDegrees(0.0, 0.0).ToPoint() == line.vertex(0));
    REQUIRE(S2LatLng::FromDegrees(0.0, 1.0).ToPoint() == line.vertex(1));
    REQUIRE(S2LatLng::FromDegrees(1.0, 1.0).ToPoint() == line.vertex(2));
    REQUIRE(S2LatLng::FromDegrees(1.0, 0.0).ToPoint() == line.vertex(3));
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

    REQUIRE(Type::LINESTRING == type(vpack));
    REQUIRE(parseRegion(vpack, shape).ok());
    REQUIRE(ShapeContainer::Type::S2_POLYLINE == shape.type());
  }

  SECTION("Valid MultiLinestring") {
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

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseMultiLinestring(vpack, multiline).ok());

    REQUIRE(2 == multiline.size());

    REQUIRE(4 == multiline[0].num_vertices());
    REQUIRE(S2LatLng::FromDegrees(-1.0, -1.0).ToPoint() ==
            multiline[0].vertex(0));
    REQUIRE(S2LatLng::FromDegrees(-1.0, 2.0).ToPoint() ==
            multiline[0].vertex(1));
    REQUIRE(S2LatLng::FromDegrees(2.0, 2.0).ToPoint() ==
            multiline[0].vertex(2));
    REQUIRE(S2LatLng::FromDegrees(2.0, -1.0).ToPoint() ==
            multiline[0].vertex(3));

    REQUIRE(4 == multiline[1].num_vertices());
    REQUIRE(S2LatLng::FromDegrees(0.0, 0.0).ToPoint() ==
            multiline[1].vertex(0));
    REQUIRE(S2LatLng::FromDegrees(0.0, 1.0).ToPoint() ==
            multiline[1].vertex(1));
    REQUIRE(S2LatLng::FromDegrees(1.0, 1.0).ToPoint() ==
            multiline[1].vertex(2));
    REQUIRE(S2LatLng::FromDegrees(1.0, 0.0).ToPoint() ==
            multiline[1].vertex(3));
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

    REQUIRE(Type::MULTI_LINESTRING == type(vpack));
    REQUIRE(parseRegion(vpack, shape).ok());
    REQUIRE(ShapeContainer::Type::S2_MULTIPOLYLINE == shape.type());
  }

  SECTION("Valid Polygon, triangle") {
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

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).ok());

    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));

    REQUIRE(!shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  }
  
  SECTION("Valid Polygon, empty rectangle") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder rings(&builder, "coordinates");
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(41.41));
          point->add(VPackValue(41.41));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(41.41));
          point->add(VPackValue(41.41));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(41.41));
          point->add(VPackValue(41.41));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(41.41));
          point->add(VPackValue(41.41));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(41.41));
          point->add(VPackValue(41.41));
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).ok());
    
    REQUIRE(shape.type() == ShapeContainer::Type::S2_LATLNGRECT);
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(41.0, 41.0).ToPoint()));
  }
  
  SECTION("Valid Polygon, rectangle") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder rings(&builder, "coordinates");
      {
        ArrayBuilder points(&builder);
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1));
          point->add(VPackValue(-1));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1));
          point->add(VPackValue(-1));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(1));
          point->add(VPackValue(1));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1));
          point->add(VPackValue(1));
        }
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1));
          point->add(VPackValue(-1));
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).ok());
    
    REQUIRE(shape.type() == ShapeContainer::Type::S2_LATLNGRECT);
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1, 0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-1, 0).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0, -1).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0, 1).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1, -1).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1, 1).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-1,-1).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(-1.00001,-1.00001).ToPoint()));
  }

  SECTION("Valid Polygon, nested rings") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("Polygon"));
      ArrayBuilder rings(&builder, "coordinates");
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(-1.0));
          point->add(VPackValue(-1.0));
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
        {
          ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
    VPackSlice vpack = builder.slice();

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parsePolygon(vpack, shape).ok());

    REQUIRE(shape.contains(S2LatLng::FromDegrees(-0.99, -0.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-0.99, 1.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.99, 1.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.99, -0.99).ToPoint()));

    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.5, -0.5).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(1.5, 0.5).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-0.5, 1.5).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(-0.5, 0.5).ToPoint()));

    REQUIRE(!shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
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

    REQUIRE(Type::POLYGON == type(vpack));
    REQUIRE(parseRegion(vpack, shape).ok());

    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));

    REQUIRE(!shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  }
  SECTION("Valid MultiPolygon") {
    {
      ObjectBuilder object(&builder);
      object->add("type", VPackValue("MultiPolygon"));
      ArrayBuilder polygons(&builder, "coordinates");
      {
        {
          ArrayBuilder rings(&builder);
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
        {
          ArrayBuilder rings(&builder);
          {
            ArrayBuilder points(&builder);
            {
              ArrayBuilder point(&builder);
              point->add(VPackValue(2.0));
              point->add(VPackValue(2.0));
            }
            {
              ArrayBuilder point(&builder);
              point->add(VPackValue(3.0));
              point->add(VPackValue(2.0));
            }
            {
              ArrayBuilder point(&builder);
              point->add(VPackValue(2.0));
              point->add(VPackValue(3.0));
            }
            {
              ArrayBuilder point(&builder);
              point->add(VPackValue(2.0));
              point->add(VPackValue(2.0));
            }
          }
        }
      }
    }
    VPackSlice vpack = builder.slice();
    
    REQUIRE(Type::MULTI_POLYGON == type(vpack));
    REQUIRE(parseMultiPolygon(vpack, shape).ok());
    
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));
    
    REQUIRE(shape.contains(S2LatLng::FromDegrees(2.01, 2.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(2.01, 2.99).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(2.99, 2.01).ToPoint()));
    REQUIRE(shape.contains(S2LatLng::FromDegrees(2.49, 2.49).ToPoint()));
    
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
    REQUIRE(!shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));

  }
}
