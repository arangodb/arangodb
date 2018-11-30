// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "s2/s2builderutil_s2polyline_layer.h"

#include <string>
#include "s2/base/casts.h"
#include "s2/base/integral_types.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2debug.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IndexedS2PolylineLayer;
using s2builderutil::S2PolylineLayer;
using s2textformat::MakePolylineOrDie;
using std::vector;

using EdgeType = S2Builder::EdgeType;

namespace {

void TestS2Polyline(
    const vector<const char*>& input_strs,
    const char* expected_str, EdgeType edge_type,
    const S2Builder::Options& options = S2Builder::Options()) {
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  S2Builder builder(options);
  S2Polyline output;
  builder.StartLayer(make_unique<S2PolylineLayer>(
      &output, S2PolylineLayer::Options(edge_type)));
  for (auto input_str : input_strs) {
    builder.AddPolyline(*MakePolylineOrDie(input_str));
  }
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(expected_str, s2textformat::ToString(output));
}

// Convenience function that tests both directed and undirected edges.
void TestS2Polyline(
    const vector<const char*>& input_strs, const char* expected_str,
    const S2Builder::Options& options = S2Builder::Options()) {
  TestS2Polyline(input_strs, expected_str, EdgeType::DIRECTED, options);
  TestS2Polyline(input_strs, expected_str, EdgeType::UNDIRECTED, options);
}

void TestS2PolylineUnchanged(const char* input_str) {
  TestS2Polyline(vector<const char*>{input_str}, input_str);
}

TEST(S2PolylineLayer, NoEdges) {
  TestS2Polyline({}, "");
}

TEST(S2PolylineLayer, OneEdge) {
  // Even with undirected edges, S2PolylineLayer prefers to reconstruct edges
  // in their original direction.
  TestS2PolylineUnchanged("3:4, 1:1");
  TestS2PolylineUnchanged("1:1, 3:4");
}

TEST(S2PolylineLayer, StraightLineWithBacktracking) {
  TestS2PolylineUnchanged("0:0, 1:0, 2:0, 3:0, 2:0, 1:0, 2:0, 3:0, 4:0");
}

TEST(S2PolylineLayer, EarlyWalkTerminationWithEndLoop1) {
  // Test that the "early walk termination" code (which is needed by
  // S2PolylineVectorLayer in order to implement idempotency) does not create
  // two polylines when it is possible to assemble the edges into one.
  //
  // This example tests a code path where the early walk termination code
  // should not be triggered at all (but was at one point due to a bug).
  S2Builder::Options options;
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(2));
  TestS2Polyline({"0:0, 0:2, 0:1"}, "0:0, 0:1, 0:2, 0:1", options);
}

TEST(S2PolylineLayer, EarlyWalkTerminationWithEndLoop2) {
  // This tests a different code path where the walk is terminated early
  // (yield a polyline with one edge), and then the walk is "maximimzed" by
  // appending a two-edge loop to the end.
  TestS2Polyline({"0:0, 0:1", "0:2, 0:1", "0:1, 0:2"},
                 "0:0, 0:1, 0:2, 0:1");
}

TEST(S2PolylineLayer, SimpleLoop) {
  TestS2PolylineUnchanged("0:0, 0:5, 5:5, 5:0, 0:0");
}

TEST(S2PolylineLayer, ManyLoops) {
  // This polyline consists of many overlapping loops that keep returning to
  // the same starting vertex (2:2).  This tests whether the implementation is
  // able to assemble the polyline in the original order.
  TestS2PolylineUnchanged(
      "0:0, 2:2, 2:4, 2:2, 2:4, 4:4, 4:2, 2:2, 4:4, 4:2, 2:2, 2:0, 2:2, "
      "2:0, 4:0, 2:2, 4:2, 2:2, 0:2, 0:4, 2:2, 0:4, 0:2, 2:2, 0:4, 2:2, "
      "0:2, 2:2, 0:0, 0:2, 2:2, 0:0");
}

TEST(S2PolylineLayer, UnorderedLoops) {
  // This test consists of 5 squares that touch diagonally, similar to the 5
  // white squares of a 3x3 chessboard.  The edges of these squares need to be
  // reordered to assemble them into a single unbroken polyline.
  TestS2Polyline({
      "3:3, 3:2, 2:2, 2:3, 3:3",
      "1:0, 0:0, 0:1, 1:1, 1:0",
      "3:1, 3:0, 2:0, 2:1, 3:1",
      "1:3, 1:2, 0:2, 0:1, 1:3",
      "1:1, 1:2, 2:2, 2:1, 1:1",  // Central square
      },
    "3:3, 3:2, 2:2, 2:1, 3:1, 3:0, 2:0, 2:1, 1:1, 1:0, 0:0, "
    "0:1, 1:1, 1:2, 0:2, 0:1, 1:3, 1:2, 2:2, 2:3, 3:3");
}

TEST(S2PolylineLayer, SplitEdges) {
  // Test reconstruction of a polyline where two edges have been split into
  // many pieces by crossing edges.  This example is particularly challenging
  // because (1) the edges form a loop, and (2) the first and last edges are
  // identical (but reversed).  This is designed to test the heuristics that
  // attempt to find the first edge of the input polyline.
  S2Builder::Options options;
  options.set_split_crossing_edges(true);
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(7));
  TestS2Polyline(
      {"0:10, 0:0, 1:0, -1:2, 1:4, -1:6, 1:8, -1:10, -5:0, 0:0, 0:10"},
      "0:10, 0:9, 0:7, 0:5, 0:3, 0:1, 0:0, 1:0, 0:1, -1:2, 0:3, 1:4, 0:5, "
      "-1:6, 0:7, 1:8, 0:9, -1:10, -5:0, 0:0, 0:1, 0:3, 0:5, 0:7, 0:9, 0:10",
      options);
}

TEST(S2PolylineLayer, SimpleEdgeLabels) {
  S2Builder builder{S2Builder::Options()};
  S2Polyline output;
  S2PolylineLayer::LabelSetIds label_set_ids;
  IdSetLexicon label_set_lexicon;
  builder.StartLayer(make_unique<S2PolylineLayer>(
      &output, &label_set_ids, &label_set_lexicon,
      S2PolylineLayer::Options(EdgeType::UNDIRECTED)));
  builder.set_label(5);
  builder.AddPolyline(*MakePolylineOrDie("0:0, 0:1, 0:2"));
  builder.push_label(7);
  builder.AddPolyline(*MakePolylineOrDie("0:3, 0:2"));
  builder.clear_labels();
  builder.AddPolyline(*MakePolylineOrDie("0:3, 0:4, 0:5"));
  builder.set_label(11);
  builder.AddPolyline(*MakePolylineOrDie("0:6, 0:5"));
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

TEST(S2PolylineLayer, InvalidPolyline) {
  S2Builder builder{S2Builder::Options()};
  S2Polyline output;
  S2PolylineLayer::Options options;
  options.set_validate(true);
  builder.StartLayer(make_unique<S2PolylineLayer>(&output, options));
  vector<S2Point> vertices;
  vertices.push_back(S2Point(1, 0, 0));
  vertices.push_back(S2Point(-1, 0, 0));
  S2Polyline input(vertices, S2Debug::DISABLE);
  builder.AddPolyline(input);
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_EQ(S2Error::ANTIPODAL_VERTICES, error.code());
}


TEST(IndexedS2PolylineLayer, AddsShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PolylineLayer>(&index));
  const string& polyline_str = "0:0, 0:10";
  builder.AddPolyline(*MakePolylineOrDie(polyline_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(1, index.num_shape_ids());
  const S2Polyline* polyline = down_cast<const S2Polyline::Shape*>(
      index.shape(0))->polyline();
  EXPECT_EQ(polyline_str, s2textformat::ToString(*polyline));
}

TEST(IndexedS2PolylineLayer, AddsEmptyShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PolylineLayer>(&index));
  S2Polyline line;
  builder.AddPolyline(line);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(0, index.num_shape_ids());
}

}  // namespace
