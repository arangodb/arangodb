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

#include "s2/s2builderutil_s2polyline_vector_layer.h"

#include <memory>
#include <string>
#include "s2/base/casts.h"
#include "s2/base/integral_types.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/strings/str_join.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IndexedS2PolylineVectorLayer;
using s2builderutil::S2PolylineVectorLayer;
using s2textformat::MakePolylineOrDie;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using PolylineType = S2PolylineVectorLayer::Options::PolylineType;

namespace {

void TestS2PolylineVector(
    const vector<const char*>& input_strs,
    const vector<const char*>& expected_strs,
    EdgeType edge_type,
    S2PolylineVectorLayer::Options layer_options =  // by value
    S2PolylineVectorLayer::Options(),
    const S2Builder::Options& builder_options = S2Builder::Options()) {
  layer_options.set_edge_type(edge_type);
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  S2Builder builder(builder_options);
  vector<unique_ptr<S2Polyline>> output;
  builder.StartLayer(
      make_unique<S2PolylineVectorLayer>(&output, layer_options));
  for (auto input_str : input_strs) {
    builder.AddPolyline(*MakePolylineOrDie(input_str));
  }
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  vector<string> output_strs;
  for (const auto& polyline : output) {
    output_strs.push_back(s2textformat::ToString(*polyline));
  }
  EXPECT_EQ(absl::StrJoin(expected_strs, "; "),
            absl::StrJoin(output_strs, "; "));
}

// Convenience function that tests both directed and undirected edges.
void TestS2PolylineVector(
    const vector<const char*>& input_strs,
    const vector<const char*>& expected_strs,
    const S2PolylineVectorLayer::Options& layer_options =
    S2PolylineVectorLayer::Options(),
    const S2Builder::Options& builder_options = S2Builder::Options()) {
  TestS2PolylineVector(input_strs, expected_strs, EdgeType::DIRECTED,
                       layer_options, builder_options);
  TestS2PolylineVector(input_strs, expected_strs, EdgeType::UNDIRECTED,
                       layer_options, builder_options);
}

void TestS2PolylineVectorUnchanged(const vector<const char*>& input_strs) {
  TestS2PolylineVector(input_strs, input_strs);
}

TEST(S2PolylineVectorLayer, NoEdges) {
  TestS2PolylineVectorUnchanged({});
}

TEST(S2PolylineVectorLayer, TwoPolylines) {
  TestS2PolylineVectorUnchanged({"0:0, 1:1, 2:2", "4:4, 3:3"});
}

TEST(S2PolylineVectorLayer, JoiningPolylines) {
  // Check that polylines are joined together when possible, even if they were
  // not adjacent in the input.  For undirected edges, the polyline direction
  // should be chosen such that the first edge of the polyline was added to
  // S2Builder before the last edge of the polyline.
  TestS2PolylineVector({"1:1, 2:2", "3:3, 2:2", "0:0, 1:1"},
                       {"3:3, 2:2", "0:0, 1:1, 2:2"}, EdgeType::DIRECTED);
  TestS2PolylineVector({"1:1, 2:2", "3:3, 2:2", "0:0, 1:1"},
                       {"3:3, 2:2, 1:1, 0:0"}, EdgeType::UNDIRECTED);
}

TEST(S2PolylineVectorLayer, SegmentNetwork) {
  // Test a complex network of polylines that meet at shared vertices.
  TestS2PolylineVectorUnchanged({
      "0:0, 1:1, 2:2",
      "2:2, 2:3, 2:4",
      "2:4, 3:4, 4:4",
      "2:2, 3:2, 4:2",
      "4:2, 4:3, 4:4",
      "1:0, 2:2",
      "0:1, 2:2",
      "5:4, 4:4",
      "4:5, 4:4",
      "2:4, 2:5, 1:5, 1:4, 2:4",
      "4:2, 6:1, 5:0",  // Two nested loops
      "4:2, 7:0, 6:-1",
      "11:1, 11:0, 10:0, 10:1, 11:1"  // Isolated loop
    });
}

TEST(S2PolylineVectorLayer, MultipleIntersectingWalks) {
  // This checks idempotency for directed edges in the case of several
  // polylines that share edges (and that even share loops).  The test
  // happens to pass for undirected edges as well.
  S2PolylineVectorLayer::Options layer_options;
  layer_options.set_polyline_type(PolylineType::WALK);
  vector<const char*> input = {
    "5:5, 5:6, 6:5, 5:5, 5:4, 5:3",
    "4:4, 5:5, 6:5, 5:6, 5:5, 5:6, 6:5, 5:5, 4:5",
    "3:5, 5:5, 5:6, 6:5, 5:5, 5:6, 6:6, 7:7",
  };
  TestS2PolylineVector(input, input, layer_options);
}

TEST(S2PolylineVectorLayer, EarlyWalkTermination) {
  // This checks idempotency for cases where earlier polylines in the input
  // happen to terminate in the middle of later polylines.  This requires
  // building non-maximal polylines.
  S2PolylineVectorLayer::Options layer_options;
  layer_options.set_polyline_type(PolylineType::WALK);
  vector<const char*> input = {
    "0:1, 1:1",
    "1:0, 1:1, 1:2",
    "0:2, 1:2, 2:2",
    "2:1, 2:2, 2:3"
  };
  TestS2PolylineVector(input, input, layer_options);
}

TEST(S2PolylineVectorLayer, InputEdgeStartsMultipleLoops) {
  // A single input edge is split into several segments by removing portions
  // of it, and then each of those segments becomes one edge of a loop.
  S2PolylineVectorLayer::Options layer_options;
  layer_options.set_polyline_type(PolylineType::WALK);
  layer_options.set_sibling_pairs(
      S2PolylineVectorLayer::Options::SiblingPairs::DISCARD);
  S2Builder::Options builder_options;
  builder_options.set_snap_function(s2builderutil::IntLatLngSnapFunction(7));
  vector<const char*> input = {
    "0:10, 0:0",
    "0:6, 1:6, 1:7, 0:7, 0:8",
    "0:8, 1:8, 1:9, 0:9, 0:10",
    "0:2, 1:2, 1:3, 0:3, 0:4",
    "0:0, 1:0, 1:1, 0:1, 0:2",
    "0:4, 1:4, 1:5, 0:5, 0:6",
  };
  vector<const char*> expected = {
    "0:1, 0:0, 1:0, 1:1, 0:1",
    "0:3, 0:2, 1:2, 1:3, 0:3",
    "0:5, 0:4, 1:4, 1:5, 0:5",
    "0:7, 0:6, 1:6, 1:7, 0:7",
    "0:9, 0:8, 1:8, 1:9, 0:9",
  };
  TestS2PolylineVector(input, expected, layer_options, builder_options);
}

TEST(S2PolylineVectorLayer, SimpleEdgeLabels) {
  S2Builder builder{S2Builder::Options()};
  vector<unique_ptr<S2Polyline>> output;
  S2PolylineVectorLayer::LabelSetIds label_set_ids;
  IdSetLexicon label_set_lexicon;
  S2PolylineVectorLayer::Options layer_options;
  layer_options.set_edge_type(EdgeType::UNDIRECTED);
  layer_options.set_duplicate_edges(
      S2PolylineVectorLayer::Options::DuplicateEdges::MERGE);
  builder.StartLayer(make_unique<S2PolylineVectorLayer>(
      &output, &label_set_ids, &label_set_lexicon, layer_options));
  builder.set_label(1);
  builder.AddPolyline(*MakePolylineOrDie("0:0, 0:1, 0:2"));
  builder.set_label(2);
  builder.AddPolyline(*MakePolylineOrDie("0:3, 0:2, 0:1"));
  builder.clear_labels();
  builder.AddPolyline(*MakePolylineOrDie("0:4, 0:5"));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  vector<vector<vector<int32>>> expected = {{{1}, {1, 2}, {2}}, {{}}};
  ASSERT_EQ(expected.size(), label_set_ids.size());
  for (int i = 0; i < expected.size(); ++i) {
    ASSERT_EQ(expected[i].size(), label_set_ids[i].size());
    for (int j = 0; j < expected[i].size(); ++j) {
      ASSERT_EQ(expected[i][j].size(),
                label_set_lexicon.id_set(label_set_ids[i][j]).size());
      int k = 0;
      for (int32 label : label_set_lexicon.id_set(label_set_ids[i][j])) {
        EXPECT_EQ(expected[i][j][k++], label);
      }
    }
  }
}

TEST(IndexedS2PolylineVectorLayer, AddsShapes) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PolylineVectorLayer>(&index));
  string polyline0_str = "0:0, 1:1";
  string polyline1_str = "2:2, 3:3";
  builder.AddPolyline(*s2textformat::MakePolylineOrDie(polyline0_str));
  builder.AddPolyline(*s2textformat::MakePolylineOrDie(polyline1_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(2, index.num_shape_ids());
  const S2Polyline* polyline0 = down_cast<const S2Polyline::Shape*>(
      index.shape(0))->polyline();
  const S2Polyline* polyline1 = down_cast<const S2Polyline::Shape*>(
      index.shape(1))->polyline();
  EXPECT_EQ(polyline0_str, s2textformat::ToString(*polyline0));
  EXPECT_EQ(polyline1_str, s2textformat::ToString(*polyline1));
}

}  // namespace
