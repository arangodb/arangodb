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

#include "s2/s2builderutil_s2polygon_layer.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include "s2/base/casts.h"
#include "s2/base/integral_types.h"
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2debug.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::IndexedS2PolygonLayer;
using s2builderutil::S2PolygonLayer;
using s2textformat::MakePolylineOrDie;
using std::map;
using std::set;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;

namespace {

void TestS2Polygon(const vector<const char*>& input_strs,
                   const char* expected_str, EdgeType edge_type) {
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  S2Builder builder{S2Builder::Options()};
  S2Polygon output;
  builder.StartLayer(make_unique<S2PolygonLayer>(
      &output, S2PolygonLayer::Options(edge_type)));
  bool is_full = false;
  for (auto input_str : input_strs) {
    if (string(input_str) == "full") is_full = true;
    builder.AddPolygon(*s2textformat::MakeVerbatimPolygonOrDie(input_str));
  }
  builder.AddIsFullPolygonPredicate(S2Builder::IsFullPolygon(is_full));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  // The input strings in tests may not be in normalized form, so we build an
  // S2Polygon and convert it back to a string.
  unique_ptr<S2Polygon> expected(s2textformat::MakePolygon(expected_str));
  EXPECT_EQ(s2textformat::ToString(*expected),
            s2textformat::ToString(output));
}

void TestS2Polygon(const vector<const char*>& input_strs,
                   const char* expected_str) {
  TestS2Polygon(input_strs, expected_str, EdgeType::DIRECTED);
  TestS2Polygon(input_strs, expected_str, EdgeType::UNDIRECTED);
}

void TestS2PolygonUnchanged(const char* input_str) {
  TestS2Polygon(vector<const char*>{input_str}, input_str);
}

// Unlike the methods above, the input consists of a set of *polylines*.
void TestS2PolygonError(const vector<const char*>& input_strs,
                        S2Error::Code expected_error, EdgeType edge_type) {
  SCOPED_TRACE(edge_type == EdgeType::DIRECTED ? "DIRECTED" : "UNDIRECTED");
  S2Builder builder{S2Builder::Options()};
  S2Polygon output;
  S2PolygonLayer::Options options(edge_type);
  options.set_validate(true);
  builder.StartLayer(make_unique<S2PolygonLayer>(&output, options));
  for (auto input_str : input_strs) {
    builder.AddPolyline(*s2textformat::MakePolyline(input_str));
  }
  S2Error error;
  ASSERT_FALSE(builder.Build(&error));
  EXPECT_EQ(expected_error, error.code());
}

void TestS2PolygonError(const vector<const char*>& input_strs,
                        S2Error::Code expected_error) {
  TestS2PolygonError(input_strs, expected_error, EdgeType::DIRECTED);
  TestS2PolygonError(input_strs, expected_error, EdgeType::UNDIRECTED);
}

TEST(S2PolygonLayer, Empty) {
  TestS2PolygonUnchanged("");
}

TEST(S2PolygonLayer, Full) {
  TestS2PolygonUnchanged("full");
}

TEST(S2PolygonLayer, SmallLoop) {
  TestS2PolygonUnchanged("0:0, 0:1, 1:1");
}

TEST(S2PolygonLayer, ThreeLoops) {
  // The second two loops are nested.
  TestS2PolygonUnchanged("0:1, 1:1, 0:0; "
                         "3:3, 3:6, 6:6, 6:3; "
                         "4:4, 4:5, 5:5, 5:4");
}

TEST(S2PolygonLayer, PartialLoop) {
  TestS2PolygonError({"0:1, 2:3, 4:5"},
                     S2Error::BUILDER_EDGES_DO_NOT_FORM_LOOPS);
}

TEST(S2PolygonLayer, InvalidPolygon) {
  TestS2PolygonError({"0:0, 0:10, 10:0, 10:10, 0:0"},
                     S2Error::LOOP_SELF_INTERSECTION);
}

TEST(S2PolygonLayer, DuplicateInputEdges) {
  // Check that S2PolygonLayer can assemble polygons even when there are
  // duplicate edges (after sibling pairs are removed), and then report the
  // duplicate edges as an error.
  S2Builder builder{S2Builder::Options()};
  S2Polygon output;
  S2PolygonLayer::Options options;
  options.set_validate(true);
  builder.StartLayer(make_unique<S2PolygonLayer>(&output, options));
  builder.AddPolyline(*MakePolylineOrDie(
      "0:0, 0:2, 2:2, 1:1, 0:2, 2:2, 2:0, 0:0"));
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_EQ(S2Error::POLYGON_LOOPS_SHARE_EDGE, error.code());
  ASSERT_EQ(2, output.num_loops());
  unique_ptr<S2Loop> loop0(s2textformat::MakeLoop("0:0, 0:2, 2:2, 2:0"));
  unique_ptr<S2Loop> loop1(s2textformat::MakeLoop("0:2, 2:2, 1:1"));
  EXPECT_TRUE(loop0->Equals(output.loop(0)));
  EXPECT_TRUE(loop1->Equals(output.loop(1)));
}

// Since we don't expect to have any crossing edges, the key for each edge is
// simply the sum of its endpoints.  This key has the advantage of being
// unchanged when the endpoints of an edge are swapped.
using EdgeLabelMap = map<S2Point, set<int32>>;

void AddPolylineWithLabels(const S2Polyline& polyline, EdgeType edge_type,
                           int32 label_begin, S2Builder* builder,
                           EdgeLabelMap *edge_label_map) {
  for (int i = 0; i + 1 < polyline.num_vertices(); ++i) {
    int32 label = label_begin + i;
    builder->set_label(label);
    // With undirected edges, reverse the direction of every other input edge.
    int dir = edge_type == EdgeType::DIRECTED ? 1 : (i & 1);
    builder->AddEdge(polyline.vertex(i + (1 - dir)), polyline.vertex(i + dir));
    S2Point key = polyline.vertex(i) + polyline.vertex(i + 1);
    (*edge_label_map)[key].insert(label);
  }
}

static void TestEdgeLabels(EdgeType edge_type) {
  S2Builder builder{S2Builder::Options()};
  S2Polygon output;
  S2PolygonLayer::LabelSetIds label_set_ids;
  IdSetLexicon label_set_lexicon;
  builder.StartLayer(make_unique<S2PolygonLayer>(
      &output, &label_set_ids, &label_set_lexicon,
      S2PolygonLayer::Options(edge_type)));

  // We use a polygon consisting of 3 loops.  The loops are reordered and
  // some of the loops are inverted during S2Polygon construction.
  EdgeLabelMap edge_label_map;
  AddPolylineWithLabels(*MakePolylineOrDie("0:0, 9:1, 1:9, 0:0, 2:8, 8:2, "
                                           "0:0, 0:10, 10:10, 10:0, 0:0"),
                        edge_type, 0, &builder, &edge_label_map);
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  vector<int> expected_loop_sizes = { 4, 3, 3 };
  ASSERT_EQ(expected_loop_sizes.size(), label_set_ids.size());
  for (int i = 0; i < expected_loop_sizes.size(); ++i) {
    ASSERT_EQ(expected_loop_sizes[i], label_set_ids[i].size());
    for (int j = 0; j < label_set_ids[i].size(); ++j) {
      S2Point key = output.loop(i)->vertex(j) + output.loop(i)->vertex(j + 1);
      const set<int32>& expected_labels = edge_label_map[key];
      ASSERT_EQ(expected_labels.size(),
                label_set_lexicon.id_set(label_set_ids[i][j]).size());
      EXPECT_TRUE(std::equal(
          expected_labels.begin(), expected_labels.end(),
          label_set_lexicon.id_set(label_set_ids[i][j]).begin()));
    }
  }
}

TEST(S2PolygonLayer, DirectedEdgeLabels) {
  TestEdgeLabels(EdgeType::DIRECTED);
}

TEST(S2PolygonLayer, UndirectedEdgeLabels) {
  TestEdgeLabels(EdgeType::UNDIRECTED);
}

TEST(S2PolygonLayer, ThreeLoopsIntoOne) {
  // Three loops (two shells and one hole) that combine into one.
  TestS2Polygon(
      {"10:0, 0:0, 0:10, 5:10, 10:10, 10:5",
       "0:10, 0:15, 5:15, 5:10",
       "10:10, 5:10, 5:5, 10:5"},
      "10:5, 10:0, 0:0, 0:10, 0:15, 5:15, 5:10, 5:5");
}

TEST(S2PolygonLayer, TrianglePyramid) {
  // A big CCW triangle containing 3 CW triangular holes.  The whole thing
  // looks like a pyramid of nine triangles.  The output consists of 6
  // positive triangles with no holes.
  TestS2Polygon(
      {"0:0, 0:2, 0:4, 0:6, 1:5, 2:4, 3:3, 2:2, 1:1",
       "0:2, 1:1, 1:3",
       "0:4, 1:3, 1:5",
       "1:3, 2:2, 2:4"},
      "0:4, 0:6, 1:5; 2:4, 3:3, 2:2; 2:2, 1:1, 1:3; "
      "1:1, 0:0, 0:2; 1:3, 0:2, 0:4; 1:3, 1:5, 2:4");
}

TEST(S2PolygonLayer, ComplexNesting) {
  // A complex set of nested polygons, with the loops in random order and the
  // vertices in random cyclic order within each loop.  This test checks that
  // the order (after S2Polygon::InitNested is called) is preserved exactly,
  // whether directed or undirected edges are used.
  TestS2PolygonUnchanged(
      "47:15, 47:5, 5:5, 5:15; "
      "35:12, 35:7, 27:7, 27:12; "
      "1:50, 50:50, 50:1, 1:1; "
      "42:22, 10:22, 10:25, 42:25; "
      "47:30, 47:17, 5:17, 5:30; "
      "7:27, 45:27, 45:20, 7:20; "
      "37:7, 37:12, 45:12, 45:7; "
      "47:47, 47:32, 5:32, 5:47; "
      "50:60, 50:55, 1:55, 1:60; "
      "25:7, 17:7, 17:12, 25:12; "
      "7:7, 7:12, 15:12, 15:7");
}

TEST(S2PolygonLayer, FiveLoopsTouchingAtOneCommonPoint) {
  // Five nested loops that touch at one common point.
  TestS2PolygonUnchanged("0:0, 0:10, 10:10, 10:0; "
                         "0:0, 1:9, 9:9, 9:1; "
                         "0:0, 2:8, 8:8, 8:2; "
                         "0:0, 3:7, 7:7, 7:3; "
                         "0:0, 4:6, 6:6, 6:4");
}

TEST(S2PolygonLayer, FourNestedDiamondsTouchingAtTwoPointsPerPair) {
  // Four diamonds nested inside each other, where each diamond shares two
  // vertices with the diamond inside it and shares its other two vertices
  // with the diamond that contains it.  The resulting shape looks vaguely
  // like an eye made out of chevrons.
  TestS2Polygon(
      {"0:10, -10:0, 0:-10, 10:0",
       "0:-20, -10:0, 0:20, 10:0",
       "0:-10, -5:0, 0:10, 5:0",
       "0:5, -5:0, 0:-5, 5:0"},
      "10:0, 0:10, -10:0, 0:20; "
      "0:-20, -10:0, 0:-10, 10:0; "
      "5:0, 0:-10, -5:0, 0:-5; "
      "0:5, -5:0, 0:10, 5:0");
}

TEST(S2PolygonLayer, SevenDiamondsTouchingAtOnePointPerPair) {
  // Seven diamonds nested within each other touching at one
  // point between each nested pair.
  TestS2PolygonUnchanged("0:-70, -70:0, 0:70, 70:0; "
                         "0:-70, -60:0, 0:60, 60:0; "
                         "0:-50, -60:0, 0:50, 50:0; "
                         "0:-40, -40:0, 0:50, 40:0; "
                         "0:-30, -30:0, 0:30, 40:0; "
                         "0:-20, -20:0, 0:30, 20:0; "
                         "0:-10, -20:0, 0:10, 10:0");
}

TEST(IndexedS2PolygonLayer, AddsShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PolygonLayer>(&index));
  const string& polygon_str = "0:0, 0:10, 10:0";
  builder.AddPolygon(*s2textformat::MakePolygon(polygon_str));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(1, index.num_shape_ids());
  const S2Polygon* polygon = down_cast<const S2Polygon::Shape*>(
      index.shape(0))->polygon();
  EXPECT_EQ(polygon_str, s2textformat::ToString(*polygon));
}

TEST(IndexedS2PolygonLayer, IgnoresEmptyShape) {
  S2Builder builder{S2Builder::Options()};
  MutableS2ShapeIndex index;
  builder.StartLayer(make_unique<IndexedS2PolygonLayer>(&index));
  S2Error error;
  ASSERT_TRUE(builder.Build(&error));
  EXPECT_EQ(0, index.num_shape_ids());
}

}  // namespace
