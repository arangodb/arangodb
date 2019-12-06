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

#include "gtest/gtest.h"

#include <s2/s2latlng.h>
#include <s2/s2loop.h>
#include <s2/s2point.h>
#include <s2/s2polyline.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/Common.h"
#include "Basics/voc-errors.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------
namespace arangodb {

class InvalidGeoJSONInputTest : public ::testing::Test {
 protected:
  S2LatLng point;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;
  geo::ShapeContainer shape;

  VPackBuilder builder;
};

TEST_F(InvalidGeoJSONInputTest, empty_object) {
  { velocypack::ObjectBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::UNKNOWN, geo::geojson::type(vpack));

  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));

  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_point) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_multipoint) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_linestring) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_multilinestring) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, wrong_type_expecting_polygon) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_point_too_few_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Point"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
    coords->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipoint_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPoint"));
    velocypack::ArrayBuilder coords(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_linestring_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Linestring"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiLinestring"));
    velocypack::ArrayBuilder lines(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multilinestring_extra_numbers_in_bad_points) {
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_object_not_array) {
  { velocypack::ObjectBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_empty_array) {
  { velocypack::ArrayBuilder object(&builder); }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_loop_numbers_instead_of_points) {
  {
    velocypack::ArrayBuilder points(&builder);
    points->add(VPackValue(0.0));
    points->add(VPackValue(0.0));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_TRUE(geo::geojson::parseLoop(vpack, true, loop).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_polygon_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("Polygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

// ===========================

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_no_coordinates) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

TEST_F(InvalidGeoJSONInputTest, bad_multipolygon_no_coordinates_empty) {
  {
    velocypack::ObjectBuilder object(&builder);
    object->add("type", VPackValue("MultiPolygon"));
    velocypack::ArrayBuilder points(&builder, "coordinates");
  }
  VPackSlice vpack = builder.slice();

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).is(TRI_ERROR_BAD_PARAMETER));
}

class ValidGeoJSONInputTest : public ::testing::Test {
 protected:
  S2LatLng point;
  S2Polyline line;
  std::vector<S2Polyline> multiline;
  S2Loop loop;
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

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePoint(vpack, point).ok());
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

  ASSERT_EQ(geo::geojson::Type::POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).ok());
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

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPoint(vpack, shape).ok());

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

  ASSERT_EQ(geo::geojson::Type::MULTI_POINT, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).ok());

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

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseLinestring(vpack, line).ok());

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

  ASSERT_EQ(geo::geojson::Type::LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).ok());
  ASSERT_TRUE(geo::ShapeContainer::Type::S2_POLYLINE ==
              shape.type());
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiLinestring(vpack, multiline).ok());

  ASSERT_EQ(2, multiline.size());

  ASSERT_EQ(4, multiline[0].num_vertices());
  ASSERT_EQ(S2LatLng::FromDegrees(-1.0, -1.0).ToPoint(), multiline[0].vertex(0));
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

  ASSERT_EQ(geo::geojson::Type::MULTI_LINESTRING, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).ok());
  ASSERT_TRUE(geo::ShapeContainer::Type::S2_MULTIPOLYLINE ==
              shape.type());
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).ok());

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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).ok());

  ASSERT_TRUE(shape.type() ==
              geo::ShapeContainer::Type::S2_LATLNGRECT);
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).ok());

  ASSERT_TRUE(shape.type() ==
              geo::ShapeContainer::Type::S2_LATLNGRECT);
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 0).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, -1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(0, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, -1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(1, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, 1).ToPoint()));
  ASSERT_TRUE(shape.contains(S2LatLng::FromDegrees(-1, -1).ToPoint()));
  ASSERT_FALSE(shape.contains(S2LatLng::FromDegrees(-1.00001, -1.00001).ToPoint()));
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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parsePolygon(vpack, shape).ok());

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

  ASSERT_EQ(geo::geojson::Type::POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseRegion(vpack, shape).ok());

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

  ASSERT_EQ(geo::geojson::Type::MULTI_POLYGON, geo::geojson::type(vpack));
  ASSERT_TRUE(geo::geojson::parseMultiPolygon(vpack, shape).ok());

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

}  // namespace arangodb
