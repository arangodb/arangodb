// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)
//
// Note that the "real" testing of these methods is in s2loop_measures_test
// and s2polyline_measures_test.  This file only checks the handling of shapes
// of different dimensions and shapes with multiple edge chains.

#include "s2/s2shape_measures.h"

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2edge_vector_shape.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2text_format.h"

using s2textformat::MakeIndexOrDie;
using s2textformat::MakeLaxPolygonOrDie;
using s2textformat::MakeLaxPolylineOrDie;
using s2textformat::MakePolygonOrDie;
using s2textformat::ParsePointsOrDie;

namespace {

TEST(GetLength, WrongDimension) {
  EXPECT_EQ(S1Angle::Zero(),
            S2::GetLength(*MakeIndexOrDie("0:0 # #")->shape(0)));
  EXPECT_EQ(S1Angle::Zero(),
            S2::GetLength(*MakeLaxPolygonOrDie("0:0, 0:1, 1:0")));
}

TEST(GetLength, NoPolylines) {
  EXPECT_EQ(S1Angle::Zero(), S2::GetLength(*MakeLaxPolylineOrDie("")));
}

TEST(GetLength, ThreePolylinesInOneShape) {
  // S2EdgeVectorShape is the only standard S2Shape that can have more than
  // one edge chain of dimension 1.
  auto p = ParsePointsOrDie("0:0, 1:0, 2:0, 3:0");
  S2EdgeVectorShape shape({{p[0], p[1]}, {p[0], p[2]}, {p[0], p[3]}});
  EXPECT_EQ(S1Angle::Degrees(6), S2::GetLength(shape));
}

TEST(GetPerimeter, WrongDimension) {
  EXPECT_EQ(S1Angle::Zero(),
            S2::GetPerimeter(*MakeIndexOrDie("0:0 # #")->shape(0)));
  EXPECT_EQ(S1Angle::Zero(),
            S2::GetPerimeter(*MakeLaxPolylineOrDie("0:0, 0:1, 1:0")));
}

TEST(GetPerimeter, EmptyPolygon) {
  EXPECT_EQ(S1Angle::Zero(), S2::GetPerimeter(*MakeLaxPolygonOrDie("empty")));
}

TEST(GetPerimeter, FullPolygon) {
  EXPECT_EQ(S1Angle::Zero(), S2::GetPerimeter(*MakeLaxPolygonOrDie("full")));
}

TEST(GetPerimeter, TwoLoopPolygon) {
  // To ensure that all edges are 1 degree long, we use degenerate loops.
  EXPECT_EQ(S1Angle::Degrees(6),
            S2::GetPerimeter(*MakeLaxPolygonOrDie("0:0, 1:0; 0:1, 0:2, 0:3")));
}

TEST(GetArea, WrongDimension) {
  EXPECT_EQ(0.0, S2::GetArea(*MakeIndexOrDie("0:0 # #")->shape(0)));
  EXPECT_EQ(0.0, S2::GetArea(*MakeLaxPolylineOrDie("0:0, 0:1, 1:0")));
}

TEST(GetArea, EmptyPolygon) {
  EXPECT_EQ(0.0, S2::GetArea(*MakeLaxPolygonOrDie("empty")));
}

TEST(GetArea, FullPolygon) {
  EXPECT_EQ(4 * M_PI, S2::GetArea(*MakeLaxPolygonOrDie("full")));
  EXPECT_EQ(4 * M_PI,
            S2::GetArea(S2Polygon::OwningShape(MakePolygonOrDie("full"))));
}

TEST(GetArea, TwoTinyShells) {
  // Two small squares with sides about 10 um (micrometers) long.
  double side = S1Angle::Degrees(1e-10).radians();
  EXPECT_EQ(2 * side * side, S2::GetArea(*MakeLaxPolygonOrDie(
      "0:0, 0:1e-10, 1e-10:1e-10, 1e-10:0; "
      "0:0, 0:-1e-10, -1e-10:-1e-10, -1e-10:0")));
}

TEST(GetArea, TinyShellAndHole) {
  // A square about 20 um on each side with a hole 10 um on each side.
  double side = S1Angle::Degrees(1e-10).radians();
  EXPECT_EQ(3 * side * side, S2::GetArea(*MakeLaxPolygonOrDie(
      "0:0, 0:2e-10, 2e-10:2e-10, 2e-10:0; "
      "0.5e-10:0.5e-10, 1.5e-10:0.5e-10, 1.5e-10:1.5e-10, 0.5e-10:1.5e-10")));
}

TEST(GetApproxArea, LargeShellAndHolePolygon) {
  // Make sure that GetApproxArea works well for large polygons.
  EXPECT_NEAR(S2::GetApproxArea(*MakeLaxPolygonOrDie(
      "0:0, 0:90, 90:0; 0:22.5, 90:0, 0:67.5")),
              M_PI_4, 1e-12);
}

TEST(GetCentroid, Points) {
  // GetCentroid() returns the centroid multiplied by the number of points.
  EXPECT_EQ(S2Point(1, 1, 0),
            S2::GetCentroid(*MakeIndexOrDie("0:0 | 0:90 # #")->shape(0)));
}

TEST(GetCentroid, Polyline) {
  // GetCentroid returns the centroid multiplied by the length of the polyline.
  EXPECT_TRUE(S2::ApproxEquals(
      S2Point(1, 1, 0),
      S2::GetCentroid(*MakeLaxPolylineOrDie("0:0, 0:90"))));
}

TEST(GetCentroid, Polygon) {
  // GetCentroid returns the centroid multiplied by the area of the polygon.
  EXPECT_TRUE(S2::ApproxEquals(
      S2Point(M_PI_4, M_PI_4, M_PI_4),
      S2::GetCentroid(*MakeLaxPolygonOrDie("0:0, 0:90, 90:0"))));
}

}  // namespace
