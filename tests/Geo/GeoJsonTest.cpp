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
#include <s2/s2loop.h>
#include <s2/s2point.h>
#include <s2/s2polyline.h>
#include <s2/s2polygon.h>
#include <velocypack/Builder.h>

#include "Aql/VelocyPackHelper.h"
#include "Basics/Common.h"
#include "Basics/voc-errors.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Geo/S2/S2MultiPolylineRegion.h"
#include "Geo/S2/S2MultiPointRegion.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------
namespace arangodb {

class InvalidGeoJSONInputTest : public ::testing::Test {
 protected:
  S2LatLng point;
  S2Polyline line;
  geo::S2MultiPointRegion points;
  geo::S2MultiPolylineRegion polylines;
  S2Polygon polygon;
  S2Loop loop;
  geo::ShapeContainer shape;

  VPackBuilder builder;
};

TEST_F(InvalidGeoJSONInputTest, empty_object) {
  { velocypack::ObjectBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::UNKNOWN, geo::json::type(vpack));

  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(
      geo::json::parseRegion(vpack, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_point) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_multipoint) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_linestring) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_multilinestring) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_polygon) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_too_few_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_too_many_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_multiple_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_numbers_instead_of_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_extra_numbers_in_bad_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPoint(vpack, points).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_numbers_instead_of_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_extra_numbers_in_bad_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_numbers_instead_of_lines) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
    lines->add(VPackValue(0.0));
    lines->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_numbers_instead_of_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      points->add(VPackValue(0.0));
      points->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest,
       bad_multilinestring_extra_numbers_in_bad_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
    velocypack::ArrayBuilder points(&builder);
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_points_outside_of_line) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
    // don't open linestring, just add points directly
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(1.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines)
                  .is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_object_not_array) {
  { velocypack::ObjectBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_empty_array) {
  { velocypack::ArrayBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_numbers_instead_of_points) {
  {
    velocypack::ArrayBuilder points(&builder);
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_extra_numbers_in_bad_points) {
  {
    velocypack::ArrayBuilder points(&builder);
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_full_geojson_input) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(1.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(1.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(1.0));
      point->add(VPackValue(0.0));
    }
    {
      velocypack::ArrayBuilder point(&builder);
      point->add(VPackValue(0.0));
      point->add(VPackValue(0.0));
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(
      geo::json::parseLoop(vpack, loop, true).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_numbers_instead_of_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_points_instead_of_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
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
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_extra_numbers_in_bad_points) {
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
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_too_few_points) {
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
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_not_closed) {
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

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_nonnested_rings) {
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
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
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
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_outer_ring_not_first) {
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
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parsePolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

// ===========================

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_numbers_instead_of_polygons) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_numbers_instead_of_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    velocypack::ArrayBuilder points(&builder);
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_points_instead_of_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    velocypack::ArrayBuilder rings(&builder);
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
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_extra_numbers_in_bad_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    {
      velocypack::ArrayBuilder rings(&builder);
      {
        velocypack::ArrayBuilder points(&builder);
        {
          velocypack::ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          velocypack::ArrayBuilder point(&builder);
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
        {
          velocypack::ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(1.0));
          point->add(VPackValue(0.0));
        }
        {
          velocypack::ArrayBuilder point(&builder);
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
          point->add(VPackValue(0.0));
        }
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_too_few_points) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    velocypack::ArrayBuilder polygons(&builder);
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
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_not_closed) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    velocypack::ArrayBuilder polygons(&builder);
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

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_nonnested_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    velocypack::ArrayBuilder rings(&builder);
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
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
        point->add(VPackValue(-1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(-1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_outer_ring_not_first) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    velocypack::ArrayBuilder rings(&builder);
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(
      geo::json::parseMultiPolygon(vpack, polygon).is(TRI_ERROR_BAD_PARAMETER));
}

class ValidGeoJSONInputTest : public ::testing::Test {
 protected:
  S2LatLng point;
  S2Polyline line;
  geo::S2MultiPolylineRegion polylines;
  geo::ShapeContainer shape;

  VPackBuilder builder;
};

TEST_F(ValidGeoJSONInputTest, valid_point) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(1.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parsePoint(vpack, point).ok());
  ASSERT_EQ(0.0, point.lng().degrees());
  ASSERT_EQ(1.0, point.lat().degrees());
}

TEST_F(ValidGeoJSONInputTest, valid_point_as_region) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
    coords->add(VPackValue(1.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_multipoint) {
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

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_multipoint_as_region) {
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

  ASSERT_EQ(geo::json::Type::MULTI_POINT, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 0.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.0, 0.0).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(2.0, 2.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_linestring) {
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

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseLinestring(vpack, line).ok());

  ASSERT_EQ(4, line.num_vertices());
  ASSERT_EQ(S2LatLng::FromDegrees(0.0, 0.0).ToPoint(), line.vertex(0));
  ASSERT_EQ(S2LatLng::FromDegrees(0.0, 1.0).ToPoint(), line.vertex(1));
  ASSERT_EQ(S2LatLng::FromDegrees(1.0, 1.0).ToPoint(), line.vertex(2));
  ASSERT_EQ(S2LatLng::FromDegrees(1.0, 0.0).ToPoint(), line.vertex(3));
}

TEST_F(ValidGeoJSONInputTest, valid_linestring_as_region) {
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

  ASSERT_EQ(geo::json::Type::LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());
  ASSERT_TRUE(geo::ShapeContainer::Type::S2_POLYLINE == shape.type());
}

TEST_F(ValidGeoJSONInputTest, valid_multilinestring) {
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

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseMultiLinestring(vpack, polylines).ok());

  auto& multiline = polylines.Impl();
  ASSERT_EQ(2, multiline.size());

  ASSERT_EQ(4, multiline[0].num_vertices());
  ASSERT_EQ(S2LatLng::FromDegrees(-1.0, -1.0).ToPoint(),
            multiline[0].vertex(0));
  ASSERT_EQ(S2LatLng::FromDegrees(-1.0, 2.0).ToPoint(), multiline[0].vertex(1));
  ASSERT_EQ(S2LatLng::FromDegrees(2.0, 2.0).ToPoint(), multiline[0].vertex(2));
  ASSERT_EQ(S2LatLng::FromDegrees(2.0, -1.0).ToPoint(), multiline[0].vertex(3));

  ASSERT_EQ(4, multiline[1].num_vertices());
  ASSERT_EQ(S2LatLng::FromDegrees(0.0, 0.0).ToPoint(), multiline[1].vertex(0));
  ASSERT_EQ(S2LatLng::FromDegrees(0.0, 1.0).ToPoint(), multiline[1].vertex(1));
  ASSERT_EQ(S2LatLng::FromDegrees(1.0, 1.0).ToPoint(), multiline[1].vertex(2));
  ASSERT_EQ(S2LatLng::FromDegrees(1.0, 0.0).ToPoint(), multiline[1].vertex(3));
}

TEST_F(ValidGeoJSONInputTest, valid_multilinestring_as_region) {
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

  ASSERT_EQ(geo::json::Type::MULTI_LINESTRING, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());
  ASSERT_TRUE(geo::ShapeContainer::Type::S2_MULTIPOLYLINE == shape.type());
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_triangle) {
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

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_empty_rectangle) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  auto r = geo::json::parseRegion(vpack, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();

  ASSERT_TRUE(shape.type() == geo::ShapeContainer::Type::S2_POLYGON);
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(41.0, 41.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_empty_rectangle_legacy) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(41.41));
        point->add(VPackValue(41.41));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, true).ok());

  ASSERT_TRUE(shape.type() == geo::ShapeContainer::Type::S2_LATLNGRECT);
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(41.0, 41.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_rectangle) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(-1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1));
        point->add(VPackValue(-1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1));
        point->add(VPackValue(1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(-1));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  // Please note: Some of the ASSERT_FALSEs below are not intuitive, since
  // one would expect this polygon to contain all points with coordinators
  // whose latitude is between -1 and 1 (including the boundaries) and whose
  // longitude is between -1 and 1 (including the boundaries). However, this
  // is not how S2 polygons work. Not even the vertices on the boundary
  // polyline need to be contained in the polygon. Rather, a tricky definition
  // says clearly, to which side of a polyline a vertex belongs with the
  // property, that if one cuts the earth into polygonal shapes with polylines,
  // every point is in exactly one area.
  // Unfortunately, this is not the same definition as GeoJSON uses!
  ASSERT_TRUE(shape.type() == geo::ShapeContainer::Type::S2_POLYGON);
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 0).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0, -1).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0, 1).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1, -1).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(-1, -1).ToPoint()));
  ASSERT_FALSE(
      shape.contains(S2LatLng::FromDegrees(-1.00001, -1.00001).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_rectangle_legacy) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(-1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1));
        point->add(VPackValue(-1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1));
        point->add(VPackValue(1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(1));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1));
        point->add(VPackValue(-1));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, true).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, -1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, -1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, -1).ToPoint()));
  ASSERT_FALSE(
      shape.contains(S2LatLng::FromDegrees(-1.00001, -1.00001).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_nested_rings) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
    }
    // Note that the following example is contrary to the rules of GeoJSON,
    // since the inner loop is not running clockwise around the hole. However,
    // we are lenient here and waive the problem, since the intention is clear.
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.99, -0.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.99, 1.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.99, 1.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.99, -0.99).ToPoint()));

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.5, -0.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.5, 0.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.5, 1.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.5, 0.5).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_nested_rings_standard) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder rings(&builder, "coordinates");
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
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(-1.0));
        point->add(VPackValue(-1.0));
      }
    }
    // Note that the following example is correct according to the
    // rules of GeoJSON, since the inner loop is running clockwise
    // around the hole. We internally invert it to get nested loops and
    // `InitNested` is happy. The intention is clear here.
    {
      velocypack::ArrayBuilder points(&builder);
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(1.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(1.0));
        point->add(VPackValue(0.0));
      }
      {
        velocypack::ArrayBuilder point(&builder);
        point->add(VPackValue(0.0));
        point->add(VPackValue(0.0));
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.99, -0.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.99, 1.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.99, 1.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.99, -0.99).ToPoint()));

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.5, -0.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1.5, 0.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.5, 1.5).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-0.5, 0.5).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_polygon_as_region) {
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

  ASSERT_EQ(geo::json::Type::POLYGON, geo::json::type(vpack));
  ASSERT_TRUE(geo::json::parseRegion(vpack, shape, false).ok());

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
}

TEST_F(ValidGeoJSONInputTest, valid_multipolygon) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder polygons(&builder, "coordinates");
    {
      {
        velocypack::ArrayBuilder rings(&builder);
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
      {
        velocypack::ArrayBuilder rings(&builder);
        {
          velocypack::ArrayBuilder points(&builder);
          {
            velocypack::ArrayBuilder point(&builder);
            point->add(VPackValue(2.0));
            point->add(VPackValue(2.0));
          }
          {
            velocypack::ArrayBuilder point(&builder);
            point->add(VPackValue(3.0));
            point->add(VPackValue(2.0));
          }
          {
            velocypack::ArrayBuilder point(&builder);
            point->add(VPackValue(2.0));
            point->add(VPackValue(3.0));
          }
          {
            velocypack::ArrayBuilder point(&builder);
            point->add(VPackValue(2.0));
            point->add(VPackValue(2.0));
          }
        }
      }
    }
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::json::Type::MULTI_POLYGON, geo::json::type(vpack));
  auto r = geo::json::parseRegion(vpack, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.01, 0.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.99, 0.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0.49, 0.49).ToPoint()));

  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(2.01, 2.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(2.01, 2.99).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(2.99, 2.01).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(2.49, 2.49).ToPoint()));

  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(1.0, 1.0).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(3.0, 3.0).ToPoint()));
}

using namespace arangodb::tests;

TEST_F(InvalidGeoJSONInputTest, self_intersecting_loop) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[10,10],[20,20],[20,10],[10,20],[10,10]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, sharing_edges) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[[10,10],[20,10],[20,20],[10,20],[10,10]],
                    [[10,10],[20,10],[15,15],[10,10]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, intersecting_edges) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[[10,10],[20,10],[20,20],[10,20],[10,10]],
                    [[10,10],[18,11],[15,15],[10,10]],
                    [[12,12],[19,12],[15,19],[12,12]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, repeated_vertices) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[10,10],[20,10],[15,15],[20,20],[10,20],[15,15],[10,10]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, crosses_edges_multi) {
  auto poly = R"({
    "type": "MultiPolygon",
    "coordinates": [[[[10,10],[20,10],[20,20],[10,20],[10,10]]],
                    [[[9,9],[19,9],[15,15],[9,9]]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, shares_edges_multi) {
  auto poly = R"({
    "type": "MultiPolygon",
    "coordinates": [[[[10,10],[20,10],[20,20],[10,20],[10,10]]],
                    [[[5,5],[20,10],[10,10],[5,5]]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(
      geo::json::parseRegion(polyS, shape, false).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(ValidGeoJSONInputTest, containing_multi) {
  auto poly = R"({
    "type": "MultiPolygon",
    "coordinates": [[[[10,10],[20,10],[20,20],[10,20],[10,10]]],
                    [[[11,11],[19,11],[19,19],[11,19],[11,11]]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  auto r = geo::json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();
}

TEST_F(ValidGeoJSONInputTest, sharing_vertices) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[[10,10],[20,10],[20,20],[10,20],[10,10]],
                    [[10,10],[18,11],[15,15],[10,10]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  ASSERT_TRUE(geo::json::parseRegion(polyS, shape, false).ok());
}

TEST_F(ValidGeoJSONInputTest, sharing_vertices_multi) {
  auto poly = R"({
    "type": "MultiPolygon",
    "coordinates": [[[[10,10],[20,10],[20,20],[10,20],[10,10]]],
                    [[[10,10],[5,5],[18,9],[10,10]]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  auto r = geo::json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();
}

TEST_F(ValidGeoJSONInputTest, proper_inclusion_testing_multi) {
  auto poly = R"({
    "type": "MultiPolygon",
    "coordinates": [ 
    [ [[40, 40], [20, 45], [45, 30], [40, 40]] ],
    [ [[20, 35], [10, 30], [10, 10], [30, 5], [45, 20], [20, 35]],
      [[30, 20], [20, 15], [20, 25], [30, 20]] ] ]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  auto r = geo::json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();
}

TEST_F(ValidGeoJSONInputTest, NestedHoles) {
  auto poly = R"({
    "type": "Polygon",
    "coordinates": [[[10,10],[20,10],[20,20],[10,20],[10,10]],
                    [[11,11],[19,11],[19,19],[11,19],[11,11]],
                    [[12,12],[18,12],[18,18],[12,18],[12,12]],
                    [[13,13],[17,13],[17,17],[13,17],[13,13]],
                    [[14,14],[16,14],[16,16],[14,16],[14,14]]]
  })"_vpack;
  VPackSlice polyS = velocypack::Slice(poly->data());
  auto r = geo::json::parseRegion(polyS, shape, false);
  ASSERT_TRUE(r.ok()) << r.errorMessage();
}

}  // namespace arangodb
