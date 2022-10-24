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

#include "s2/s2builderutil_lax_polyline_layer.h"

#include <string>

#include "s2/base/casts.h"
#include "s2/base/integral_types.h"
#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IndexedLaxPolylineLayer;
using s2builderutil::LaxPolylineLayer;
using s2textformat::MakeLaxPolylineOrDie;
using std::string;
using std::vector;

using EdgeType = S2Builder::EdgeType;

namespace {

void TestS2LaxPolylineShape(
    const vector<const char*>& input_strs,
    const char* expected_str, EdgeType edge_type,
    const S2Builder::Options& options = S2Builder::Options()) {
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  S2Builder builder(options);
  S2LaxPolylineShape output;
  builder.StartLayer(make_unique<LaxPolylineLayer>(
      &output, LaxPolylineLayer::Options(edge_type)));
  for (auto input_str : input_strs) {
    builder.AddShape(*MakeLaxPolylineOrDie(input_str));
  }
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(expected_str, s2textformat::ToString(output));
}

// Convenience function that tests both directed and undirected edges.
void TestS2LaxPolylineShape(
    const vector<const char*>& input_strs, const char* expected_str,
    const S2Builder::Options& options = S2Builder::Options()) {
  TestS2LaxPolylineShape(input_strs, expected_str, EdgeType::DIRECTED, options);
  TestS2LaxPolylineShape(input_strs, expected_str, EdgeType::UNDIRECTED,
                         options);
}

void TestS2LaxPolylineShapeUnchanged(const char* input_str) {
  TestS2LaxPolylineShape(vector<const char*>{input_str}, input_str);
}

TEST(LaxPolylineLayer, NoEdges) {
  TestS2LaxPolylineShape({}, "");
}

TEST(LaxPolylineLayer, OneEdge) {
  // Even with undirected edges, LaxPolylineLayer prefers to reconstruct edges
  // in their original direction.
  TestS2LaxPolylineShapeUnchanged("3:4, 1:1");
  TestS2LaxPolylineShapeUnchanged("1:1, 3:4");
}

TEST(LaxPolylineLayer, StraightLineWithBacktracking) {
  TestS2LaxPolylineShapeUnchanged(
      "0:0, 1:0, 2:0, 3:0, 2:0, 1:0, 2:0, 3:0, 4:0");
}

TEST(LaxPolylineLayer, EarlyWalkTerminationWithEndLoop1) {
  // Test that the "early walk termination" code (which is needed by
  // S2LaxPolylineShapeVectorLayer in order to implement idempotency) does not
  // create two polylines when it is possible to assemble the edges into one.
  //
  // This example tests a code path where the early walk termination code
  // should not be triggered at all (but was at one point due to a bug).
  S2Builder::Options options;
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(2));
  TestS2LaxPolylineShape({"0:0, 0:2, 0:1"}, "0:0, 0:1, 0:2, 0:1", options);
}

TEST(LaxPolylineLayer, EarlyWalkTerminationWithEndLoop2) {
  // This tests a different code path where the walk is terminated early
  // (yield a polyline with one edge), and then the walk is "maximimzed" by
  // appending a two-edge loop to the end.
  TestS2LaxPolylineShape({"0:0, 0:1", "0:2, 0:1", "0:1, 0:2"},
                         "0:0, 0:1, 0:2, 0:1");
}

TEST(LaxPolylineLayer, SimpleLoop) {
  TestS2LaxPolylineShapeUnchanged("0:0, 0:5, 5:5, 5:0, 0:0");
}

TEST(LaxPolylineLayer, ManyLoops) {
  // This polyline consists of many overlapping loops that keep returning to
  // the same starting vertex (2:2).  This tests whether the implementation is
  // able to assemble the polyline in the original order.
  TestS2LaxPolylineShapeUnchanged(
      "0:0, 2:2, 2:4, 2:2, 2:4, 4:4, 4:2, 2:2, 4:4, 4:2, 2:2, 2:0, 2:2, "
      "2:0, 4:0, 2:2, 4:2, 2:2, 0:2, 0:4, 2:2, 0:4, 0:2, 2:2, 0:4, 2:2, "
      "0:2, 2:2, 0:0, 0:2, 2:2, 0:0");
}

TEST(LaxPolylineLayer, UnorderedLoops) {
  // This test consists of 5 squares that touch diagonally, similar to the 5
  // white squares of a 3x3 chessboard.  The edges of these squares need to be
  // reordered to assemble them into a single unbroken polyline.
  TestS2LaxPolylineShape({
      "3:3, 3:2, 2:2, 2:3, 3:3",
      "1:0, 0:0, 0:1, 1:1, 1:0",
      "3:1, 3:0, 2:0, 2:1, 3:1",
      "1:3, 1:2, 0:2, 0:1, 1:3",
      "1:1, 1:2, 2:2, 2:1, 1:1",  // Central square
      },
    "3:3, 3:2, 2:2, 2:1, 3:1, 3:0, 2:0, 2:1, 1:1, 1:0, 0:0, "
    "0:1, 1:1, 1:2, 0:2, 0:1, 1:3, 1:2, 2:2, 2:3, 3:3");
}

TEST(LaxPolylineLayer, SplitEdges) {
  // Test reconstruction of a polyline where two edges have been split into
  // many pieces by crossing edges.  This example is particularly challenging
  // because (1) the edges form a loop, and (2) the first and last edges are
  // identical (but reversed).  This is designed to test the heuristics that
  // attempt to find the first edge of the input polyline.
  S2Builder::Options options;
  options.set_split_crossing_edges(true);
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(7));
  TestS2LaxPolylineShape(
      {"0:10, 0:0, 1:0, -1:2, 1:4, -1:6, 1:8, -1:10, -5:0, 0:0, 0:10"},
      "0:10, 0:9, 0:7, 0:5, 0:3, 0:1, 0:0, 1:0, 0:1, -1:2, 0:3, 1:4, 0:5, "
      "-1:6, 0:7, 1:8, 0:9, -1:10, -5:0, 0:0, 0:1, 0:3, 0:5, 0:7, 0:9, 0:10",
      options);
}

TEST(LaxPolylineLayer, SimpleEdgeLabels) {
  S2Builder builder{S2Builder::Options()};
  S2LaxPolylineShape output;
  LaxPolylineLayer::LabelSetIds label_set_ids;
  IdSetLexicon label_set_lexicon;
  builder.StartLayer(make_unique<LaxPolylineLayer>(
      &output, &label_set_ids, &label_set_lexicon,
      LaxPolylineLayer::Options(EdgeType::UNDIRECTED)));
  builder.set_label(5);
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:1, 0:2"));
  builder.push_label(7);
  builder.AddShape(*MakeLaxPolylineOrDie("0:3, 0:2"));
  builder.clear_labels();
  builder.AddShape(*MakeLaxPolylineOrDie("0:3, 0:4, 0:5"));
  builder.set_label(11);
  builder.AddShape(*MakeLaxPolylineOrDie("0:6, 0:5"));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  vector<vector<int32>> expected = {{5}, {5}, {5, 7}, {}, {}, {11}};
  ASSERT_EQ(expected.size(), label_set_ids.size());
  for (int i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(expected[i].size(),
              label_set_lexicon.id_set(label_set_ids[i]).size());
    int j = 0;
    for (int32 label : label_set_lexicon.id_set(label_set_ids[i])) {
      EXPECT_EQ(expected[i][j++], label);
    }
  }
}

TEST(LaxPolylineLayer, AntipodalVertices) {
  S2Builder builder{S2Builder::Options()};
  S2LaxPolylineShape output;
  builder.StartLayer(make_unique<LaxPolylineLayer>(&output));
  builder.AddEdge(S2Point(1, 0, 0), S2Point(-1, 0, 0));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  EXPECT_EQ(output.num_vertices(), 2);
  EXPECT_EQ(output.vertex(0), S2Point(1, 0, 0));
  EXPECT_EQ(output.vertex(1), S2Point(-1, 0, 0));
}


TEST(IndexedLaxPolylineLayer, AddsShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedLaxPolylineLayer>(&index));
  const string& polyline_str = "0:0, 0:10";
  builder.AddShape(*MakeLaxPolylineOrDie(polyline_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(1, index.num_shape_ids());
  const S2LaxPolylineShape* polyline = down_cast<const S2LaxPolylineShape*>(
      index.shape(0));
  EXPECT_EQ(polyline_str, s2textformat::ToString(*polyline));
}

TEST(IndexedLaxPolylineLayer, AddsEmptyShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedLaxPolylineLayer>(&index));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(0, index.num_shape_ids());
}

}  // namespace
