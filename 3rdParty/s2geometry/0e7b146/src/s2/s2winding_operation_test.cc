// Copyright 2020 Google Inc. All Rights Reserved.
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

#include "s2/s2winding_operation.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2builderutil_lax_polygon_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2cap.h"
#include "s2/s2error.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2loop.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IdentitySnapFunction;
using s2builderutil::IntLatLngSnapFunction;
using std::string;
using std::unique_ptr;
using std::vector;

using WindingRule = S2WindingOperation::WindingRule;

namespace {

// Verifies that the S2WindingOperation with the given arguments produces the
// given result.  In order to ensure that results are not affected by the
// cyclic order of the loop vertices or the S2ShapeIndex loop ordering,
// we compute the symmetric difference using S2BooleanOperation.  (We don't
// use s2builderutil::IndexMatchingLayer because that class does not
// distinguish empty from full polygons, and we don't need its ability to
// match edge multiplicities here.)
void ExpectWindingResult(
    const S2WindingOperation::Options& options, const vector<string>& loop_strs,
    const string& ref_point_str, int ref_winding,
    S2WindingOperation::WindingRule rule, const string& expected_str) {
  MutableS2ShapeIndex expected;
  expected.Add(s2textformat::MakeLaxPolygonOrDie(expected_str));
  MutableS2ShapeIndex actual;
  S2WindingOperation winding_op(
      make_unique<s2builderutil::IndexedLaxPolygonLayer>(&actual), options);
  for (const string& loop_str : loop_strs) {
    winding_op.AddLoop(s2textformat::ParsePointsOrDie(loop_str));
  }
  S2Error error;
  ASSERT_TRUE(winding_op.Build(s2textformat::MakePointOrDie(ref_point_str),
                               ref_winding, rule, &error)) << error;
  S2_LOG(INFO) << "Actual: " << s2textformat::ToString(actual);
  S2LaxPolygonShape difference;
  S2BooleanOperation diff_op(
      S2BooleanOperation::OpType::SYMMETRIC_DIFFERENCE,
      make_unique<s2builderutil::LaxPolygonLayer>(&difference));
  ASSERT_TRUE(diff_op.Build(actual, expected, &error)) << error;
  EXPECT_TRUE(difference.is_empty())
      << "Difference S2Polygon: " << s2textformat::ToString(difference);
}

// Like ExpectWindingResult(), but with two different expected results
// depending on whether options.include_degeneracies() is false or true.
void ExpectDegenerateWindingResult(
    S2WindingOperation::Options options, const vector<string>& loop_strs,
    const string& ref_point_str, int ref_winding,
    S2WindingOperation::WindingRule rule,
    const string& expected_str_false, const string& expected_str_true) {
  options.set_include_degeneracies(false);
  ExpectWindingResult(options, loop_strs, ref_point_str, ref_winding, rule,
                      expected_str_false);
  options.set_include_degeneracies(true);
  ExpectWindingResult(options, loop_strs, ref_point_str, ref_winding, rule,
                      expected_str_true);
}

TEST(S2WindingOperation, Empty) {
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {""}, "5:5", 0, WindingRule::POSITIVE, "");
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {""}, "5:5", 1, WindingRule::POSITIVE, "full");
}

TEST(S2WindingOperation, PointLoop) {
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{},
      {"2:2"}, "5:5", 0, WindingRule::POSITIVE,
      "", "2:2");
}

TEST(S2WindingOperation, SiblingPairLoop) {
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{},
      {"2:2, 3:3"}, "5:5", 0, WindingRule::POSITIVE,
      "", "2:2, 3:3");
}

TEST(S2WindingOperation, Rectangle) {
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:10, 10:10, 10:0"}, "5:5", 1, WindingRule::POSITIVE,
      "0:0, 0:10, 10:10, 10:0");
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:10, 10:10, 10:0"}, "5:5", 1, WindingRule::NEGATIVE,
      "");
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:10, 10:10, 10:0"}, "5:5", 1, WindingRule::NON_ZERO,
      "0:0, 0:10, 10:10, 10:0");
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:10, 10:10, 10:0"}, "5:5", 1, WindingRule::ODD,
      "0:0, 0:10, 10:10, 10:0");
}

TEST(S2WindingOperation, BowTie) {
  // Note that NEGATIVE, NON_ZERO, and ODD effectively reverse the orientation
  // of one of the two triangles that form the bow tie.
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(1))},
      {"5:-5, -5:5, 5:5, -5:-5"}, "10:0", 0, WindingRule::POSITIVE,
      "0:0, -5:5, 5:5");
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(1))},
      {"5:-5, -5:5, 5:5, -5:-5"}, "10:0", 0, WindingRule::NEGATIVE,
      "-5:-5, 0:0, 5:-5");
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(1))},
      {"5:-5, -5:5, 5:5, -5:-5"}, "10:0", 0, WindingRule::NON_ZERO,
      "0:0, -5:5, 5:5; -5:-5, 0:0, 5:-5");
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(1))},
      {"5:-5, -5:5, 5:5, -5:-5"}, "10:0", 0, WindingRule::ODD,
      "0:0, -5:5, 5:5; -5:-5, 0:0, 5:-5");
}

TEST(S2WindingOperation, CollapsingShell) {
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(5))},
      {"0:0, 0:3, 3:3"}, "10:0", 0, WindingRule::POSITIVE,
      "", "0:0");
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(5))},
      {"0:0, 0:3, 3:3"}, "1:1", 1, WindingRule::POSITIVE,
      "", "0:0");
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(5))},
      {"0:0, 3:3, 0:3"}, "10:0", 1, WindingRule::POSITIVE,
      "full");
  ExpectWindingResult(
      S2WindingOperation::Options{IdentitySnapFunction(S1Angle::Degrees(5))},
      {"0:0, 3:3, 0:3"}, "1:1", 0, WindingRule::POSITIVE,
      "full");
}

// Two triangles that touch along a common boundary.
TEST(S2WindingOperation, TouchingTriangles) {
  // The touch edges are considered to form a degenerate hole.  Such holes are
  // removed by WindingRule::POSITIVE since they are not needed for computing
  // N-way unions.  They are kept by WindingRule::ODD since they are needed in
  // order to compute N-way symmetric differences.
  ExpectWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:8, 8:8", "0:0, 8:8, 8:0"}, "1:1", 1, WindingRule::POSITIVE,
      "0:0, 0:8, 8:8, 8:0");
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{},
      {"0:0, 0:8, 8:8", "0:0, 8:8, 8:0"}, "2:2", 1, WindingRule::ODD,
      "0:0, 0:8, 8:8, 8:0", "0:0, 0:8, 8:8; 0:0, 8:8, 8:0");
}

// Like the test above, but the triangles only touch after snapping.
TEST(S2WindingOperation, TouchingTrianglesAfterSnapping) {
  // The snap function below rounds coordinates to the nearest degree.
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(0)},
      {"0.1:0.2, 0:7.8, 7.6:8.2",
       "0.3:0.2, 8.1:7.8, 7.6:0.4"}, "6:2", 1, WindingRule::POSITIVE,
      "0:0, 0:8, 8:8, 8:0");
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(0)},
      {"0.1:0.2, 0:7.8, 7.6:8.2",
       "0.3:0.2, 8.1:7.8, 7.6:0.4"}, "2:6", 1, WindingRule::ODD,
      "0:0, 0:8, 8:8, 8:0", "0:0, 0:8, 8:8; 0:0, 8:8, 8:0");
}

// This tests an N-way union of 5 overlapping squares forming a "staircase".
TEST(S2WindingOperation, UnionOfSquares) {
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:4, 4:4, 4:0", "1:1, 1:5, 5:5, 5:1", "2:2, 2:6, 6:6, 6:2",
       "3:3, 3:7, 7:7, 7:3", "4:4, 4:8, 8:8, 8:4"},
      "0.5:0.5", 1, WindingRule::POSITIVE,
      "7:4, 7:3, 6:3, 6:2, 5:2, 5:1, 4:1, 4:0, 0:0, 0:4, "
      "1:4, 1:5, 2:5, 2:6, 3:6, 3:7, 4:7, 4:8, 8:8, 8:4");

  // This computes the region overlapped by at least two squares.
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:4, 4:4, 4:0", "1:1, 1:5, 5:5, 5:1", "2:2, 2:6, 6:6, 6:2",
       "3:3, 3:7, 7:7, 7:3", "4:4, 4:8, 8:8, 8:4"},
      "0.5:0.5", 0, WindingRule::POSITIVE,
      "6:4, 6:3, 5:3, 5:2, 4:2, 4:1, 1:1, 1:4, 2:4, 2:5, "
      "3:5, 3:6, 4:6, 4:7, 7:7, 7:4");

  // This computes the region overlapped by at least three squares.
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:4, 4:4, 4:0", "1:1, 1:5, 5:5, 5:1", "2:2, 2:6, 6:6, 6:2",
       "3:3, 3:7, 7:7, 7:3", "4:4, 4:8, 8:8, 8:4"},
      "0.5:0.5", -1, WindingRule::POSITIVE,
      "5:4, 5:3, 4:3, 4:2, 2:2, 2:4, 3:4, 3:5, 4:5, 4:6, 6:6, 6:4");

  // This computes the region overlapped by at least four squares.
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:4, 4:4, 4:0", "1:1, 1:5, 5:5, 5:1", "2:2, 2:6, 6:6, 6:2",
       "3:3, 3:7, 7:7, 7:3", "4:4, 4:8, 8:8, 8:4"},
      "0.5:0.5", -2, WindingRule::POSITIVE,
      "3:3, 3:4, 4:4, 4:3; 4:4, 4:5, 5:5, 5:4");

  // WindingRule::ODD yields a pattern reminiscent of a checkerboard.
  ExpectWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:4, 4:4, 4:0", "1:1, 1:5, 5:5, 5:1", "2:2, 2:6, 6:6, 6:2",
       "3:3, 3:7, 7:7, 7:3", "4:4, 4:8, 8:8, 8:4"},
      "0.5:0.5", 1, WindingRule::ODD,
      "4:1, 4:0, 0:0, 0:4, 1:4, 1:1; "
      "4:3, 4:2, 2:2, 2:4, 3:4, 3:3; "
      "1:4, 1:5, 2:5, 2:4; "
      "5:4, 5:3, 4:3, 4:4; "
      "5:2, 5:1, 4:1, 4:2; "
      "2:5, 2:6, 3:6, 3:5; "
      "6:3, 6:2, 5:2, 5:3; "
      "3:6, 3:7, 4:7, 4:6; "
      "3:4, 3:5, 4:5, 4:4; "
      "7:4, 7:3, 6:3, 6:4; "
      "4:7, 4:8, 8:8, 8:4, 7:4, 7:7; "
      "4:5, 4:6, 6:6, 6:4, 5:4, 5:5");
}

// This tests that WindingRule::ODD can be used to compute the symmtric
// difference even for input geometries with degeneracies, e.g. one geometry
// has a degenerate hole or degenerate shell that the other does not.
TEST(S2WindingOperation, SymmetricDifferenceDegeneracies) {
  ExpectDegenerateWindingResult(
      S2WindingOperation::Options{IntLatLngSnapFunction(1)},
      {"0:0, 0:3, 3:3, 3:0", "1:1", "2:2", "4:4",   // Geometry 1
       "0:0, 0:3, 3:3, 3:0", "1:1", "4:4", "5:5"},  // Geometry 2
      "10:10", 0, WindingRule::ODD,
      "", "2:2; 5:5");
}

}  // namespace
