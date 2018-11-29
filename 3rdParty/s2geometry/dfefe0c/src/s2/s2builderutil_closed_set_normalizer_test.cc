// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "s2/s2builderutil_closed_set_normalizer.h"

#include <memory>
#include <vector>

#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2boolean_operation.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_s2point_vector_layer.h"
#include "s2/s2builderutil_s2polygon_layer.h"
#include "s2/s2builderutil_s2polyline_vector_layer.h"
#include "s2/s2builderutil_testing.h"
#include "s2/s2text_format.h"
#include <gtest/gtest.h>

using absl::make_unique;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using Edge = Graph::Edge;

namespace s2builderutil {

// A test harness that sets default values for ClosedSetNormalizer::Options
// and the S2Builder::GraphOptions for each of the three output layers.
class NormalizeTest : public testing::Test {
 public:
  NormalizeTest() : suppress_lower_dimensions_(true) {
    // Set the default GraphOptions for building S2Points, S2Polylines, and
    // S2Polygons.  Tests can modify these options as necessary.  Most of the
    // defaults are KEEP so that we can verify edge counts in some cases.
    //
    // Polyline edges are undirected by default because (1) this case is
    // slightly more challenging and (2) it is expected to be common.
    graph_options_out_.push_back(  // Points
        GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                     DuplicateEdges::KEEP, SiblingPairs::KEEP));
    graph_options_out_.push_back(  // Polylines
        GraphOptions(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                     DuplicateEdges::KEEP, SiblingPairs::KEEP));
    graph_options_out_.push_back(  // Polygons
        GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                     DuplicateEdges::KEEP, SiblingPairs::KEEP));
  }

  void Run(const string& input_str, const string& expected_str);

 protected:
  bool suppress_lower_dimensions_;
  vector<GraphOptions> graph_options_out_;

 private:
  static string ToString(const Graph& g);
  void AddLayers(const string& str, const vector<GraphOptions>& graph_options,
                 vector<Graph>* graphs_out, S2Builder* builder);

  vector<unique_ptr<GraphClone>> graph_clones_;
};

void NormalizeTest::Run(const string& input_str,
                        const string& expected_str) {
  ClosedSetNormalizer::Options options;
  options.set_suppress_lower_dimensions(suppress_lower_dimensions_);
  ClosedSetNormalizer normalizer(options, graph_options_out_);

  S2Builder builder{S2Builder::Options()};
  vector<Graph> input, expected;
  AddLayers(input_str, normalizer.graph_options(), &input, &builder);
  AddLayers(expected_str, graph_options_out_, &expected, &builder);
  S2Error error;
  // Populate the "input" and "expected" vectors.
  EXPECT_TRUE(builder.Build(&error)) << error;

  const vector<Graph>& actual = normalizer.Run(input, &error);
  for (int dim = 0; dim < 3; ++dim) {
    EXPECT_TRUE(expected[dim].options() == actual[dim].options());
    EXPECT_EQ(ToString(expected[dim]), ToString(actual[dim])) << "dim=" << dim;
  }
}

void NormalizeTest::AddLayers(
    const string& str, const vector<GraphOptions>& graph_options,
    vector<Graph>* graphs_out, S2Builder* builder) {
  auto index = s2textformat::MakeIndex(str);
  for (int dim = 0; dim < 3; ++dim) {
    builder->StartLayer(make_unique<GraphAppendingLayer>(
        graph_options[dim], graphs_out, &graph_clones_));
    for (S2Shape* shape : *index) {
      if (shape->dimension() != dim) continue;
      int n = shape->num_edges();
      for (int e = 0; e < n; ++e) {
        S2Shape::Edge edge = shape->edge(e);
        builder->AddEdge(edge.v0, edge.v1);
      }
    }
  }
}

string NormalizeTest::ToString(const Graph& g) {
  string msg;
  for (const auto& edge : g.edges()) {
    vector<S2Point> vertices = { g.vertex(edge.first),
                                 g.vertex(edge.second) };
    msg += s2textformat::ToString(vertices);
    msg += "; ";
  }
  return msg;
}

TEST_F(NormalizeTest, EmptyGraphs) {
  Run("# #", "# #");
}

TEST_F(NormalizeTest, NonDegenerateInputs) {
  Run("0:0 # 1:0, 1:1 | 1:2, 1:3 # 2:2, 2:3, 3:2",
      "0:0 # 1:0, 1:1 | 1:2, 1:3 # 2:2, 2:3, 3:2");
}

TEST_F(NormalizeTest, PointShell) {
  Run("# # 0:0", "0:0 # #");
}

TEST_F(NormalizeTest, PointHole) {
  Run("# # 0:0, 0:3, 3:0 | 1:1", "# # 0:0, 0:3, 3:0");
}

TEST_F(NormalizeTest, PointPolyline) {
  // Verify that a single degenerate polyline edge is transformed into a
  // single point.  Note that since the polyline layer is undirected while the
  // point layer is not, this tests that the edge count is halved when the
  // edge is demoted.
  Run("# 0:0, 0:0 #", "0:0 # #");
}

TEST_F(NormalizeTest, SiblingPairShell) {
  Run("# # 0:0, 1:0 ", "# 0:0, 1:0 #");
}

TEST_F(NormalizeTest, SiblingPairHole) {
  Run("# # 0:0, 0:3, 3:0; 0:0, 1:1",
      "# # 0:0, 0:3, 3:0");
}

TEST_F(NormalizeTest, PointSuppressedByPolygonVertex) {
  Run("0:0 | 0:1 | 1:0 # # 0:0, 0:1, 1:0",
      "# # 0:0, 0:1, 1:0");
  suppress_lower_dimensions_ = false;
  Run("0:0 | 0:1 | 1:0 # # 0:0, 0:1, 1:0",
      "0:0 | 0:1 | 1:0 # # 0:0, 0:1, 1:0");
}

TEST_F(NormalizeTest, PointSuppressedByPolylineVertex) {
  Run("0:0 | 0:1 # 0:0, 0:1 #", "# 0:0, 0:1 #");
  suppress_lower_dimensions_ = false;
  Run("0:0 | 0:1 # 0:0, 0:1 #", "0:0 | 0:1 # 0:0, 0:1 #");
}

TEST_F(NormalizeTest, PointShellSuppressedByPolylineEdge) {
  // This tests the case where a single-point shell is demoted to a point
  // which is then suppressed by a matching polyline vertex.
  Run("# 0:0, 1:0 # 0:0; 1:0", "# 0:0, 1:0 #");
  suppress_lower_dimensions_ = false;
  Run("# 0:0, 1:0 # 0:0; 1:0", "0:0 | 1:0 # 0:0, 1:0 #");
}

TEST_F(NormalizeTest, PolylineEdgeSuppressedByPolygonEdge) {
  Run("# 0:0, 0:1 # 0:0, 0:1, 1:0", "# # 0:0, 0:1, 1:0");
  suppress_lower_dimensions_ = false;
  Run("# 0:0, 0:1 # 0:0, 0:1, 1:0", "# 0:0, 0:1 # 0:0, 0:1, 1:0");
}

TEST_F(NormalizeTest, PolylineEdgeSuppressedByReversePolygonEdge) {
  graph_options_out_[1].set_edge_type(EdgeType::DIRECTED);
  Run("# 1:0, 0:0 # 0:0, 0:1, 1:0", "# # 0:0, 0:1, 1:0");
  suppress_lower_dimensions_ = false;
  Run("# 1:0, 0:0 # 0:0, 0:1, 1:0", "# 1:0, 0:0 # 0:0, 0:1, 1:0");
}

TEST_F(NormalizeTest, DuplicateEdgeMerging) {
  // Verify that when DuplicateEdges::KEEP is specified, demoted edges are
  // added as new edges rather than being merged with existing ones.
  // (Note that NormalizeTest specifies DuplicateEdges::KEEP by default.)
  Run("0:0 | 0:0 # 0:0, 0:0 | 0:1, 0:2 # 0:0; 0:1, 0:2",
      "0:0 | 0:0 | 0:0 | 0:0 # 0:1, 0:2 | 0:1, 0:2 #");
  // Now verify that the duplicate edges are merged if requested.
  graph_options_out_[0].set_duplicate_edges(DuplicateEdges::MERGE);
  graph_options_out_[1].set_duplicate_edges(DuplicateEdges::MERGE);
  Run("0:0 | 0:0 # 0:0, 0:0 | 0:1, 0:2 # 0:0; 0:1, 0:2",
      "0:0 # 0:1, 0:2 #");
}

// If this code changes, please update the header file comments to match.
bool ComputeUnion(const S2ShapeIndex& a, const S2ShapeIndex& b,
                  MutableS2ShapeIndex* index, S2Error* error) {
  IndexedS2PolylineVectorLayer::Options polyline_options;
  polyline_options.set_edge_type(EdgeType::UNDIRECTED);
  polyline_options.set_polyline_type(Graph::PolylineType::WALK);
  polyline_options.set_duplicate_edges(DuplicateEdges::MERGE);
  LayerVector layers(3);
  layers[0] = make_unique<IndexedS2PointVectorLayer>(index);
  layers[1] = make_unique<IndexedS2PolylineVectorLayer>(
      index, polyline_options);
  layers[2] = make_unique<IndexedS2PolygonLayer>(index);
  S2BooleanOperation op(S2BooleanOperation::OpType::UNION,
                        NormalizeClosedSet(std::move(layers)));
  return op.Build(a, b, error);
}

TEST(ComputeUnion, MixedGeometry) {
  // Verifies that the code above works.  Features tested include:
  //  - Points and polylines in the interior of the other polygon are removed
  //  - Degenerate polygon shells are converted to points/polylines
  //  - Degenerate polygon holes are removed
  //  - Points coincident with polyline or polygon edges are removed
  //  - Polyline edges coincident with polygon edges are removed
  auto a = s2textformat::MakeIndex(
      "0:0 | 10:10 | 20:20 # "
      "0:0, 0:10 | 0:0, 10:0 | 15:15, 16:16 # "
      "0:0, 0:10, 10:10, 10:0; 0:0, 1:1; 2:2; 10:10, 11:11; 12:12");
  auto b = s2textformat::MakeIndex(
      "0:10 | 10:0 | 3:3 | 16:16 # "
      "10:10, 0:10 | 10:10, 10:0 | 5:5, 6:6 # "
      "19:19, 19:21, 21:21, 21:19");
  MutableS2ShapeIndex result;
  S2Error error;
  ASSERT_TRUE(ComputeUnion(*a, *b, &result, &error));
  EXPECT_EQ("12:12 # "
            "15:15, 16:16 | 10:10, 11:11 # "
            "0:0, 0:10, 10:10, 10:0; 19:19, 19:21, 21:21, 21:19",
            s2textformat::ToString(result));
}

}  // namespace s2builderutil
