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

#include "s2/s2boolean_operation.h"

#include <memory>
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/strings/str_split.h"
#include "s2/third_party/absl/strings/strip.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2polygon.h"
#include "s2/s2text_format.h"

namespace {

using absl::make_unique;
using std::make_pair;
using std::pair;
using std::unique_ptr;
using std::vector;

using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using OpType = S2BooleanOperation::OpType;
using PolygonModel = S2BooleanOperation::PolygonModel;
using PolylineModel = S2BooleanOperation::PolylineModel;

S2Error::Code INDEXES_DO_NOT_MATCH = S2Error::USER_DEFINED_START;

class IndexMatchingLayer : public S2Builder::Layer {
 public:
  explicit IndexMatchingLayer(const S2ShapeIndex* index, int dimension)
      : index_(*index), dimension_(dimension) {
  }
  GraphOptions graph_options() const override {
    return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                        DuplicateEdges::KEEP, SiblingPairs::KEEP);
  }

  void Build(const Graph& g, S2Error* error) override;

 private:
  using EdgeVector = vector<S2Shape::Edge>;
  static string ToString(const EdgeVector& edges);

  const S2ShapeIndex& index_;
  int dimension_;
};

string IndexMatchingLayer::ToString(const EdgeVector& edges) {
  string msg;
  for (const auto& edge : edges) {
    vector<S2Point> vertices = { edge.v0, edge.v1 };
    msg += s2textformat::ToString(vertices);
    msg += "; ";
  }
  return msg;
}

void IndexMatchingLayer::Build(const Graph& g, S2Error* error) {
  vector<S2Shape::Edge> actual, expected;
  for (int e = 0; e < g.num_edges(); ++e) {
    const Graph::Edge& edge = g.edge(e);
    actual.push_back(S2Shape::Edge(g.vertex(edge.first),
                                   g.vertex(edge.second)));
  }
  for (S2Shape* shape : index_) {
    if (shape == nullptr || shape->dimension() != dimension_) {
      continue;
    }
    for (int e = shape->num_edges(); --e >= 0; ) {
      expected.push_back(shape->edge(e));
    }
  }
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());

  // The edges are a multiset, so we can't use std::set_difference.
  vector<S2Shape::Edge> missing, extra;
  for (auto ai = actual.begin(), ei = expected.begin();
       ai != actual.end() || ei != expected.end(); ) {
    if (ei == expected.end() || (ai != actual.end() && *ai < *ei)) {
      extra.push_back(*ai++);
    } else if (ai == actual.end() || *ei < *ai) {
      missing.push_back(*ei++);
    } else {
      ++ai;
      ++ei;
    }
  }
  if (!missing.empty() || !extra.empty()) {
    // There may be errors in more than one dimension, so we append to the
    // existing error text.
    error->Init(INDEXES_DO_NOT_MATCH,
                "%sDimension %d: Missing edges: %s Extra edges: %s\n",
                error->text().c_str(), dimension_, ToString(missing).c_str(),
                ToString(extra).c_str());
  }
}

void ExpectResult(S2BooleanOperation::OpType op_type,
                  const S2BooleanOperation::Options& options,
                  const string& a_str, const string& b_str,
                  const string& expected_str) {
  auto a = s2textformat::MakeIndex(a_str);
  auto b = s2textformat::MakeIndex(b_str);
  auto expected = s2textformat::MakeIndex(expected_str);
  vector<unique_ptr<S2Builder::Layer>> layers;
  for (int dim = 0; dim < 3; ++dim) {
    layers.push_back(make_unique<IndexMatchingLayer>(expected.get(), dim));
  }
  S2BooleanOperation op(op_type, std::move(layers), options);
  S2Error error;
  EXPECT_TRUE(op.Build(*a, *b, &error))
      << S2BooleanOperation::OpTypeToString(op_type) << " failed:\n"
      << "Expected result: " << expected_str << "\n" << error;

  // Now try the same thing with boolean output.
  EXPECT_EQ(expected->num_shape_ids() == 0,
            S2BooleanOperation::IsEmpty(op_type, *a, *b, options));
}

}  // namespace

// The intersections in the "expected" data below were computed in lat-lng
// space (i.e., the rectangular projection), while the actual intersections
// are computed using geodesics.  We can compensate for this by rounding the
// intersection points to a fixed precision in degrees (e.g., 2 decimals).
static S2BooleanOperation::Options RoundToE(int exp) {
  S2BooleanOperation::Options options;
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(exp));
  return options;
}

// TODO(ericv): Clean up or remove these notes.
//
// Options to test:
//   polygon_model:                   OPEN, SEMI_OPEN, CLOSED
//   polyline_model:                  OPEN, SEMI_OPEN, CLOSED
//   polyline_loops_have_boundaries:  true, false
//   conservative:                    true, false
//
// Geometry combinations to test:
//
// Point/point:
//  - disjoint, coincident
// Point/polyline:
//  - Start vertex, end vertex, interior vertex, degenerate polyline
//  - With polyline_loops_have_boundaries: start/end vertex, degenerate polyline
// Point/polygon:
//  - Polygon interior, exterior, vertex
//  - Vertex of degenerate sibling pair shell, hole
//  - Vertex of degenerate single point shell, hole
// Polyline/polyline:
//  - Vertex intersection:
//    - Start, end, interior, degenerate, loop start/end, degenerate loop
//    - Test cases where vertex is not emitted because an incident edge is.
//  - Edge/edge: interior crossing, duplicate, reversed, degenerate
//  - Test that degenerate edges are ignored unless polyline has a single edge.
//    (For example, AA has one edge but AAA has no edges.)
// Polyline/polygon:
//  - Vertex intersection: polyline vertex cases already covered, but test
//    polygon normal vertex, sibling pair shell/hole, single vertex shell/hole
//    - Also test cases where vertex is not emitted because an edge is.
//  - Edge/edge: interior crossing, duplicate, reversed
//  - Edge/interior: polyline edge in polygon interior, exterior
// Polygon/polygon:
//  - Vertex intersection:
//    - normal vertex, sibling pair shell/hole, single vertex shell/hole
//    - Also test cases where vertex is not emitted because an edge is.
//    - Test that polygons take priority when there is a polygon vertex and
//      also isolated polyline vertices.  (There should not be any points.)
//  - Edge/edge: interior crossing, duplicate, reversed
//  - Interior/interior: polygons in interior/exterior of other polygons

TEST(S2BooleanOperation, DegeneratePolylines) {
  // Verify that degenerate polylines are preserved under all boundary models.
  S2BooleanOperation::Options options;
  auto a = "# 0:0, 0:0 #";
  auto b = "# #";
  options.set_polyline_model(PolylineModel::OPEN);
  ExpectResult(OpType::UNION, options, a, b, a);
  options.set_polyline_model(PolylineModel::SEMI_OPEN);
  ExpectResult(OpType::UNION, options, a, b, a);
  options.set_polyline_model(PolylineModel::CLOSED);
  ExpectResult(OpType::UNION, options, a, b, a);
}

TEST(S2BooleanOperation, DegeneratePolygons) {
  // Verify that degenerate polygon features (single-vertex and sibling pair
  // shells and holes) are preserved under all boundary models.
  S2BooleanOperation::Options options;
  auto a = "# # 0:0, 0:5, 5:5, 5:0; 1:1; 2:2, 3:3; 6:6; 7:7, 8:8";
  auto b = "# #";
  options.set_polygon_model(PolygonModel::OPEN);
  ExpectResult(OpType::UNION, options, a, b, a);
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  ExpectResult(OpType::UNION, options, a, b, a);
  options.set_polygon_model(PolygonModel::CLOSED);
  ExpectResult(OpType::UNION, options, a, b, a);
}

TEST(S2BooleanOperation, PointPoint) {
  S2BooleanOperation::Options options;
  auto a = "0:0 | 1:0 # #";
  auto b = "0:0 | 2:0 # #";
  // Note that these results have duplicates, which is correct.  Clients can
  // eliminated the duplicates with the appropriate GraphOptions.
  ExpectResult(OpType::UNION, options, a, b,
               "0:0 | 0:0 | 1:0 | 2:0 # #");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "0:0 | 0:0 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "1:0 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "1:0 | 2:0 # #");
}

TEST(S2BooleanOperation, PointOpenPolyline) {
  // Tests operations between an open polyline and its vertices.
  //
  // The polyline "3:0, 3:0" consists of a single degenerate edge and contains
  // no points (since polyline_model() is OPEN).  Since S2BooleanOperation
  // preserves degeneracies, this means that the union includes *both* the
  // point 3:0 and the degenerate polyline 3:0, since they do not intersect.
  //
  // This test uses Options::polyline_loops_have_boundaries() == true, which
  // means that the loop "4:0, 5:0, 4:0" does not contain the vertex "4:0".
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::OPEN);
  auto a = "0:0 | 1:0 | 2:0 | 3:0 | 4:0 | 5:0 # #";
  auto b = "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #";
  ExpectResult(OpType::UNION, options, a, b,
               "0:0 | 2:0 | 3:0 | 4:0 "
               "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "1:0 | 5:0 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "0:0 | 2:0 | 3:0 | 4:0 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "0:0 | 2:0 | 3:0 | 4:0"
               "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
}

TEST(S2BooleanOperation, PointOpenPolylineLoopBoundariesFalse) {
  // With Options::polyline_loops_have_boundaries() == false, the loop
  // "4:0, 5:0, 4:0" has two vertices, both of which are contained.
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::OPEN);
  options.set_polyline_loops_have_boundaries(false);
  auto a = "0:0 | 1:0 | 2:0 | 3:0 | 4:0 | 5:0 # #";
  auto b = "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #";
  ExpectResult(OpType::UNION, options, a, b,
               "0:0 | 2:0 | 3:0 "
               "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "1:0 | 4:0 | 5:0 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "0:0 | 2:0 | 3:0 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "0:0 | 2:0 | 3:0 "
               "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
}

TEST(S2BooleanOperation, PointSemiOpenPolyline) {
  // Degenerate polylines are defined not contain any points under the
  // SEMI_OPEN model either, so again the point 3:0 and the degenerate
  // polyline "3:0, 3:0" do not intersect.
  //
  // The result does not depend on Options::polyline_loops_have_boundaries().
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::SEMI_OPEN);
  for (bool bool_value : {false, true}) {
    options.set_polyline_loops_have_boundaries(bool_value);
    auto a = "0:0 | 1:0 | 2:0 | 3:0 | 4:0 | 5:0 # #";
    auto b = "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #";
    ExpectResult(OpType::UNION, options, a, b,
                 "2:0 | 3:0 # 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
    ExpectResult(OpType::INTERSECTION, options, a, b,
                 "0:0 | 1:0 | 4:0 | 5:0 # #");
    ExpectResult(OpType::DIFFERENCE, options, a, b,
                 "2:0 | 3:0 # #");
    ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
                 "2:0 | 3:0 # 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
  }
}

TEST(S2BooleanOperation, PointClosedPolyline) {
  // Under the CLOSED model, the degenerate polyline 3:0 does contain its
  // vertex.  Since polylines take precedence over points, the union of the
  // point 3:0 and the polyline 3:0 is the polyline only.  Similarly, since
  // subtracting a point from a polyline has no effect, the symmetric
  // difference includes only the polyline objects.
  //
  // The result does not depend on Options::polyline_loops_have_boundaries().
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::CLOSED);
  for (bool bool_value : {false, true}) {
    options.set_polyline_loops_have_boundaries(bool_value);
    auto a = "0:0 | 1:0 | 2:0 | 3:0 | 4:0 | 5:0 # #";
    auto b = "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #";
    ExpectResult(OpType::UNION, options, a, b,
                 "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
    ExpectResult(OpType::INTERSECTION, options, a, b,
                 "0:0 | 1:0 | 2:0 | 3:0 | 4:0 | 5:0 # #");
    ExpectResult(OpType::DIFFERENCE, options, a, b,
                 "# #");
    ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
                 "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0, 4:0 #");
  }
}

TEST(S2BooleanOperation, PointPolygonInterior) {
  S2BooleanOperation::Options options;  // PolygonModel is irrelevant.
  // One interior point and one exterior point.
  auto a = "1:1 | 4:4 # #";
  auto b = "# # 0:0, 0:3, 3:0";
  ExpectResult(OpType::UNION, options, a, b,
               "4:4 # # 0:0, 0:3, 3:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "1:1 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "4:4 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "4:4 # # 0:0, 0:3, 3:0");
}

TEST(S2BooleanOperation, PointOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);
  // See notes about the two vertices below.
  auto a = "0:1 | 1:0 # #";
  auto b = "# # 0:0, 0:1, 1:0";
  ExpectResult(OpType::UNION, options, a, b,
               "0:1 | 1:0 # # 0:0, 0:1, 1:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "0:1 | 1:0 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "0:1 | 1:0 # # 0:0, 0:1, 1:0");
}

TEST(S2BooleanOperation, PointSemiOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  // The two vertices are chosen such that the polygon contains one vertex but
  // not the other under PolygonModel::SEMI_OPEN.  (The same vertices are used
  // for all three PolygonModel options.)
  auto polygon = s2textformat::MakePolygonOrDie("0:0, 0:1, 1:0");
  ASSERT_TRUE(polygon->Contains(s2textformat::MakePoint("0:1")));
  ASSERT_FALSE(polygon->Contains(s2textformat::MakePoint("1:0")));
  auto a = "0:1 | 1:0 # #";
  auto b = "# # 0:0, 0:1, 1:0";
  ExpectResult(OpType::UNION, options, a, b,
               "1:0 # # 0:0, 0:1, 1:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "0:1 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "1:0 # #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "1:0 # # 0:0, 0:1, 1:0");
}

TEST(S2BooleanOperation, PointClosedPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::CLOSED);
// See notes about the two vertices above.
  auto a = "0:1 | 1:0 # #";
  auto b = "# # 0:0, 0:1, 1:0";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:1, 1:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "0:1 | 1:0 # #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:1, 1:0");
}

TEST(S2BooleanOperation, PolylineVertexOpenPolylineVertex) {
  // Test first, last, and middle vertices of both polylines.  Also test
  // first/last and middle vertices of two polyline loops.
  //
  // Degenerate polylines are tested in PolylineEdgePolylineEdgeOverlap below.
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::OPEN);
  auto a = "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #";
  auto b = "# 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
           "| 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");

  // The output consists of the portion of each input polyline that intersects
  // the opposite region, so the intersection vertex is present twice.  This
  // allows reassembling the individual polylins that intersect, if desired.
  // (Otherwise duplicates can be removed using DuplicateEdges::MERGE.)
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 0:1, 0:1 | 0:1, 0:1 #");

  // Note that all operations are defined such that subtracting a
  // lower-dimensional subset of an object has no effect.  In this case,
  // subtracting the middle vertex of a polyline has no effect.
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
}

TEST(S2BooleanOperation, PolylineVertexOpenPolylineVertexLoopBoundariesFalse) {
  // With Options::polyline_loops_have_boundaries() == false, the 3 polyline
  // loops each have two vertices, both of which are contained.
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::OPEN);
  options.set_polyline_loops_have_boundaries(false);
  auto a = "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #";
  auto b = "# 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
           "| 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");

  // Note that the polyline "0:3, 0:4, 0:3" only has two vertices, not three.
  // This means that 0:3 is emitted only once for that polyline, plus once for
  // the other polyline, for a total of twice.
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 0:1, 0:1 | 0:1, 0:1 "
               "| 0:3, 0:3 | 0:3, 0:3 | 0:4, 0:4 | 0:4, 0:4 #");

  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
}

TEST(S2BooleanOperation, PolylineVertexSemiOpenPolylineVertex) {
  // The result does not depend on Options::polyline_loops_have_boundaries().
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::SEMI_OPEN);
  for (bool bool_value : {false, true}) {
    options.set_polyline_loops_have_boundaries(bool_value);
    auto a = "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #";
    auto b = "# 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
             "| 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #";
    ExpectResult(OpType::UNION, options, a, b,
                 "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
                 "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
    ExpectResult(OpType::INTERSECTION, options, a, b,
                 "# 0:0, 0:0 | 0:0, 0:0 | 0:1, 0:1 | 0:1, 0:1 "
                 "| 0:3, 0:3 | 0:3, 0:3 | 0:4, 0:4 | 0:4, 0:4 #");
    ExpectResult(OpType::DIFFERENCE, options, a, b,
                 "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #");
    ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
                 "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
                 "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
  }
}

TEST(S2BooleanOperation, PolylineVertexClosedPolylineVertex) {
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::CLOSED);
  auto a = "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #";
  auto b = "# 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
           "| 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");

  // Since Options::polyline_loops_have_boundaries() == true, the polyline
  // "0:3, 0:4, 0:3" has three vertices.  Therefore 0:3 is emitted twice for
  // that polyline, plus once for the other polyline, for a total of thrice.
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 0:0, 0:0 | 0:0, 0:0 | 0:1, 0:1 | 0:1, 0:1 "
               "| 0:2, 0:2 | 0:2, 0:2 "
               "| 0:3, 0:3 | 0:3, 0:3 | 0:3, 0:3 "
               "| 0:4, 0:4 | 0:4, 0:4 | 0:4, 0:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
}

TEST(S2BooleanOperation,
     PolylineVertexClosedPolylineVertexLoopBoundariesFalse) {
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::CLOSED);
  options.set_polyline_loops_have_boundaries(false);
  auto a = "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #";
  auto b = "# 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
           "| 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");

  // Since Options::polyline_loops_have_boundaries() == false, the polyline
  // "0:3, 0:4, 0:3" has two vertices.  Therefore 0:3 is emitted once for
  // that polyline, plus once for the other polyline, for a total of twice.
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 0:0, 0:0 | 0:0, 0:0 | 0:1, 0:1 | 0:1, 0:1 "
               "| 0:2, 0:2 | 0:2, 0:2 "
               "| 0:3, 0:3 | 0:3, 0:3 | 0:4, 0:4 | 0:4, 0:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:3, 0:4, 0:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 0:0, 0:1, 0:2 | 0:0, 1:0 | -1:1, 0:1, 1:1 | -1:2, 0:2 "
               "| 0:3, 0:4, 0:3 | 1:3, 0:3, 1:3 | 0:4, 1:4, 0:4 #");
}

// The polygon used in the polyline/polygon vertex tests below.
static string kVertexTestPolygonStr() {
  return "0:0, 0:1, 0:2, 0:3, 0:4, 0:5, 5:5, 5:4, 5:3, 5:2, 5:1, 5:0";
}

TEST(S2BooleanOperation, TestSemiOpenPolygonVerticesContained) {
  // Verify whether certain vertices of the test polygon are contained under
  // the semi-open boundary model (for use in the tests below).
  auto polygon = s2textformat::MakePolygonOrDie(kVertexTestPolygonStr());
  EXPECT_TRUE(polygon->Contains(s2textformat::MakePoint("0:1")));
  EXPECT_TRUE(polygon->Contains(s2textformat::MakePoint("0:2")));
  EXPECT_TRUE(polygon->Contains(s2textformat::MakePoint("0:3")));
  EXPECT_TRUE(polygon->Contains(s2textformat::MakePoint("0:4")));
  EXPECT_FALSE(polygon->Contains(s2textformat::MakePoint("5:1")));
  EXPECT_FALSE(polygon->Contains(s2textformat::MakePoint("5:2")));
  EXPECT_FALSE(polygon->Contains(s2textformat::MakePoint("5:3")));
  EXPECT_FALSE(polygon->Contains(s2textformat::MakePoint("5:4")));
}

// Don't bother testing every PolylineModel with every PolygonModel for vertex
// intersection, since we have already tested the PolylineModels individually
// above.  It is sufficient to use PolylineModel::CLOSED with the various
// PolygonModel options.
TEST(S2BooleanOperation, PolylineVertexOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);

  // Define some constants to reduce code duplication.
  // Test all combinations of polylines that start or end on a polygon vertex,
  // where the polygon vertex is open or closed using semi-open boundaries,
  // and where the incident edge is inside or outside the polygon.
  auto a = ("# 1:1, 0:1 | 0:2, 1:2 | -1:3, 0:3 | 0:4, -1:4 "
            "| 6:1, 5:1 | 5:2, 6:2 | 4:3, 5:3 | 5:4, 4:4 #");
  auto b = "# # " + kVertexTestPolygonStr();

  const string kDifferenceResult =
      "# 0:1, 0:1 | 0:2, 0:2 | -1:3, 0:3 | 0:4, -1:4"
      "| 6:1, 5:1 | 5:2, 6:2 | 5:3, 5:3 | 5:4, 5:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 0:1 | 0:2, 1:2 | 4:3, 5:3 | 5:4, 4:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b, kDifferenceResult);
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
}

// Like the test above, except that every polygon vertex is also incident to a
// closed polyline vertex.  This tests that when an open vertex and a closed
// vertex coincide with each other, the result is considered closed.
TEST(S2BooleanOperation, PolylineVertexOpenPolygonClosedPolylineVertex) {
  const string kTestGeometrySuffix =
      "-2:0, 0:1 | -2:1, 0:2 | -2:2, 0:3 | -2:3, 0:4 | "
      "7:0, 5:1 | 7:1, 5:2 | 7:2, 5:3 | 7:3, 5:4 # " + kVertexTestPolygonStr();

  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);
  auto a = ("# 1:1, 0:1 | 0:2, 1:2 | -1:3, 0:3 | 0:4, -1:4 "
            "| 6:1, 5:1 | 5:2, 6:2 | 4:3, 5:3 | 5:4, 4:4 #");
  auto b = ("# " + kTestGeometrySuffix);

  const string kDifferencePrefix =
      "# -1:3, 0:3 | 0:4, -1:4 | 6:1, 5:1 | 5:2, 6:2";
  ExpectResult(OpType::UNION, options, a, b,
               kDifferencePrefix +
               " | 0:1, 0:1 | 0:2, 0:2 | 5:3, 5:3 | 5:4, 5:4 | " +
               kTestGeometrySuffix);
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 0:1 | 0:2, 1:2 | 0:3, 0:3 | 0:4, 0:4"
               "| 5:1, 5:1 | 5:2, 5:2 | 4:3, 5:3 | 5:4, 4:4"
               "| 0:1, 0:1 | 0:2, 0:2 | 0:3, 0:3 | 0:4, 0:4"
               "| 5:1, 5:1 | 5:2, 5:2 | 5:3, 5:3 | 5:4, 5:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b, kDifferencePrefix + " #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               kDifferencePrefix + " | " + kTestGeometrySuffix);
}

TEST(S2BooleanOperation, PolylineVertexSemiOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  // Test all combinations of polylines that start or end on a polygon vertex,
  // where the polygon vertex is open or closed using semi-open boundaries,
  // and where the incident edge is inside or outside the polygon.
  //
  // The vertices at latitude 0 used below are all closed while the vertices
  // at latitude 5 are all open (see TestSemiOpenPolygonVerticesContained).
  auto a = ("# 1:1, 0:1 | 0:2, 1:2 | -1:3, 0:3 | 0:4, -1:4 "
            "| 6:1, 5:1 | 5:2, 6:2 | 4:3, 5:3 | 5:4, 4:4 #");
  auto b = "# # " + kVertexTestPolygonStr();
  const string kDifferenceResult =
      "# -1:3, 0:3 | 0:4, -1:4 | 6:1, 5:1 | 5:2, 6:2 | 5:3, 5:3 | 5:4, 5:4 #";
  ExpectResult(OpType::UNION, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 0:1 | 0:2, 1:2 | 0:3, 0:3 | 0:4, 0:4 "
               "| 4:3, 5:3 | 5:4, 4:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b, kDifferenceResult);
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
}

TEST(S2BooleanOperation, PolylineVertexClosedPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::CLOSED);
  // Test all combinations of polylines that start or end on a polygon vertex,
  // where the polygon vertex is open or closed using semi-open boundaries,
  // and where the incident edge is inside or outside the polygon.
  auto a = ("# 1:1, 0:1 | 0:2, 1:2 | -1:3, 0:3 | 0:4, -1:4 "
            "| 6:1, 5:1 | 5:2, 6:2 | 4:3, 5:3 | 5:4, 4:4 #");
  auto b = "# # " + kVertexTestPolygonStr();
  const string kDifferenceResult =
      "# -1:3, 0:3 | 0:4, -1:4 | 6:1, 5:1 | 5:2, 6:2 #";
  ExpectResult(OpType::UNION, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 0:1 | 0:2, 1:2 | 0:3, 0:3 | 0:4, 0:4"
               "| 5:1, 5:1 | 5:2, 5:2 | 4:3, 5:3 | 5:4, 4:4 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b, kDifferenceResult);
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               kDifferenceResult + kVertexTestPolygonStr());
}

TEST(S2BooleanOperation, PolylineEdgePolylineEdgeCrossing) {
  // Two polyline edges that cross at a point interior to both edges.
  S2BooleanOperation::Options options = RoundToE(1);
  auto a = "# 0:0, 2:2 #";
  auto b = "# 2:0, 0:2 #";
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 1:1, 2:2 | 2:0, 1:1, 0:2 #");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 1:1 | 1:1, 1:1 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 0:0, 2:2 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 0:0, 1:1, 2:2 | 2:0, 1:1, 0:2 #");
}

TEST(S2BooleanOperation, PolylineEdgePolylineEdgeOverlap) {
  // The PolylineModel does not affect this calculation.  In particular the
  // intersection of a degenerate polyline edge with itself is non-empty, even
  // though the edge contains no points in the OPEN and SEMI_OPEN models.
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);
  // Test edges in the same and reverse directions, and degenerate edges.
  auto a = "# 0:0, 1:0, 2:0, 2:5 | 3:0, 3:0 | 6:0, 5:0, 4:0 #";
  auto b = "# 0:0, 1:0, 2:0 | 3:0, 3:0 | 4:0, 5:0 #";
  // As usual, the expected output includes the relevant portions of *both*
  // input polylines.  Duplicates can be removed using GraphOptions.
  ExpectResult(OpType::UNION, options, a, b,
               "# 0:0, 1:0, 2:0, 2:5 | 0:0, 1:0, 2:0 | 3:0, 3:0 | 3:0, 3:0 "
               "| 6:0, 5:0, 4:0 | 4:0, 5:0 #");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 0:0, 1:0, 2:0 | 0:0, 1:0, 2:0 | 3:0, 3:0 | 3:0, 3:0 "
               "| 5:0, 4:0 | 4:0, 5:0 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 2:0, 2:5 | 6:0, 5:0 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 2:0, 2:5 | 6:0, 5:0 #");
}

TEST(S2BooleanOperation, PolylineEdgeOpenPolygonEdgeOverlap) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);
  // A polygon and two polyline edges that coincide with the polygon boundary,
  // one in the same direction and one in the reverse direction.
  auto a = "# 1:1, 1:3, 3:3 | 3:3, 1:3 # ";
  auto b = "# # 1:1, 1:3, 3:3, 3:1";
  ExpectResult(OpType::UNION, options, a, b,
               "# 1:1, 1:3, 3:3 | 3:3, 1:3 # 1:1, 1:3, 3:3, 3:1");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 1:1, 1:3, 3:3 | 3:3, 1:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 1:1, 1:3, 3:3 | 3:3, 1:3 # 1:1, 1:3, 3:3, 3:1");
}

TEST(S2BooleanOperation, PolylineEdgeSemiOpenPolygonEdgeOverlap) {
  auto polygon = s2textformat::MakePolygonOrDie("1:1, 1:3, 3:3, 3:1");
  ASSERT_FALSE(polygon->Contains(s2textformat::MakePoint("1:1")));
  ASSERT_TRUE(polygon->Contains(s2textformat::MakePoint("1:3")));
  ASSERT_FALSE(polygon->Contains(s2textformat::MakePoint("3:3")));
  ASSERT_FALSE(polygon->Contains(s2textformat::MakePoint("3:1")));
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  auto a = "# 1:1, 1:3, 3:3 | 3:3, 1:3 # ";
  auto b = "# # 1:1, 1:3, 3:3, 3:1";
  ExpectResult(OpType::UNION, options, a, b,
               "# 1:1, 1:1 | 3:3, 3:3 | 3:3, 1:3 # 1:1, 1:3, 3:3, 3:1");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:3, 1:3 | 1:1, 1:3, 3:3 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 1:1, 1:1 | 3:3, 3:3 | 3:3, 1:3 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 1:1, 1:1 | 3:3, 3:3 | 3:3, 1:3 # 1:1, 1:3, 3:3, 3:1");
}

TEST(S2BooleanOperation, PolylineEdgeClosedPolygonEdgeOverlap) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::CLOSED);
  auto a = "# 1:1, 1:3, 3:3 | 3:3, 1:3 # ";
  auto b = "# # 1:1, 1:3, 3:3, 3:1";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 1:1, 1:3, 3:3, 3:1");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 1:3, 3:3 | 3:3, 1:3 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 1:1, 1:3, 3:3, 3:1");
}

TEST(S2BooleanOperation, PolygonVertexMatching) {
  // This test shows that CrossingProcessor::ProcessEdgeCrossings() must set
  // a0_matches_polygon and a1_matches_polygon correctly even when (a0, a1)
  // itself is a polygon edge (or its sibling).  (It requires degenerate
  // polygon geometry to demonstrate this.)
  S2BooleanOperation::Options options;
  options.set_polyline_model(PolylineModel::CLOSED);
  options.set_polygon_model(PolygonModel::CLOSED);
  auto a = "# 0:0, 1:1 # ";
  auto b = "# # 0:0, 1:1";
  ExpectResult(OpType::UNION, options, a, b, "# # 0:0, 1:1");
}

TEST(S2BooleanOperation, PolylineEdgePolygonInterior) {
  S2BooleanOperation::Options options;  // PolygonModel is irrelevant.
  // One normal and one degenerate polyline edge in the polygon interior, and
  // similarly for the polygon exterior.
  auto a = "# 1:1, 2:2 | 3:3, 3:3 | 6:6, 7:7 | 8:8, 8:8 # ";
  auto b = "# # 0:0, 0:5, 5:5, 5:0";
  ExpectResult(OpType::UNION, options, a, b,
               "# 6:6, 7:7 | 8:8, 8:8 # 0:0, 0:5, 5:5, 5:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# 1:1, 2:2 | 3:3, 3:3 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# 6:6, 7:7 | 8:8, 8:8 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# 6:6, 7:7 | 8:8, 8:8 # 0:0, 0:5, 5:5, 5:0");
}

TEST(S2BooleanOperation, PolygonVertexOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::OPEN);
  auto a = "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5";
  auto b = "# # 0:0, 5:3, 5:2";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
}

TEST(S2BooleanOperation, PolygonVertexSemiOpenPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  auto a = "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5";
  auto b = "# # 0:0, 5:3, 5:2";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
}

TEST(S2BooleanOperation, PolygonVertexClosedPolygonVertex) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::CLOSED);
  auto a = "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5";
  auto b = "# # 0:0, 5:3, 5:2";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 0:0");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5");
  ExpectResult(OpType::DIFFERENCE, options, b, a,
               "# # 0:0, 5:3, 5:2");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:5, 1:5, 0:0, 2:5, 3:5, 0:0, 5:3, 5:2");
}

TEST(S2BooleanOperation, PolygonEdgePolygonEdgeCrossing) {
  // Two polygons whose edges cross at points interior to both edges.
  S2BooleanOperation::Options options = RoundToE(2);
  auto a = "# # 0:0, 0:2, 2:2, 2:0";
  auto b = "# # 1:1, 1:3, 3:3, 3:1";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:2, 1:2, 1:3, 3:3, 3:1, 2:1, 2:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 1:1, 1:2, 2:2, 2:1");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:2, 1:2, 1:1, 2:1, 2:0");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:2, 1:2, 1:1, 2:1, 2:0; "
               "1:2, 1:3, 3:3, 3:1, 2:1, 2:2");
}

TEST(S2BooleanOperation, PolygonEdgeOpenPolygonEdgeOverlap) {
  S2BooleanOperation::Options options;
  // One shape is a rectangle, the other consists of one triangle inside the
  // rectangle and one triangle outside the rectangle, where each triangle
  // shares one edge with the rectangle.  This implies that the edges are in
  // the same direction in one case and opposite directions in the other case.
  options.set_polygon_model(PolygonModel::OPEN);
  auto a = "# # 0:0, 0:4, 2:4, 2:0";
  auto b = "# # 0:0, 1:1, 2:0; 0:4, 1:5, 2:4";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0; 0:4, 1:5, 2:4");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 0:0, 1:1, 2:0");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1; 0:4, 1:5, 2:4");
}

TEST(S2BooleanOperation, PolygonEdgeSemiOpenPolygonEdgeOverlap) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::SEMI_OPEN);
  auto a = "# # 0:0, 0:4, 2:4, 2:0";
  auto b = "# # 0:0, 1:1, 2:0; 0:4, 1:5, 2:4";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:4, 1:5, 2:4, 2:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 0:0, 1:1, 2:0");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1");
  // Note that SYMMETRIC_DIFFERENCE does not guarantee that results are
  // normalized, i.e. the output could contain siblings pairs (which can be
  // discarded using S2Builder::GraphOptions).
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1; 0:4, 1:5, 2:4");
}

TEST(S2BooleanOperation, PolygonEdgeClosedPolygonEdgeOverlap) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(PolygonModel::CLOSED);
  auto a = "# # 0:0, 0:4, 2:4, 2:0";
  auto b = "# # 0:0, 1:1, 2:0; 0:4, 1:5, 2:4";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:4, 1:5, 2:4, 2:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 0:0, 1:1, 2:0; 0:4, 2:4");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1");
  // Note that SYMMETRIC_DIFFERENCE does not guarantee that results are
  // normalized, i.e. the output could contain siblings pairs.
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 2:4, 2:0, 1:1; 0:4, 1:5, 2:4");
}

TEST(S2BooleanOperation, PolygonPolygonInterior) {
  S2BooleanOperation::Options options;  // PolygonModel is irrelevant.
  // One loop in the interior of another polygon and one loop in the exterior.
  auto a = "# # 0:0, 0:4, 4:4, 4:0";
  auto b = "# # 1:1, 1:2, 2:2, 2:1; 5:5, 5:6, 6:6, 6:5";
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:0, 0:4, 4:4, 4:0; 5:5, 5:6, 6:6, 6:5");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 1:1, 1:2, 2:2, 2:1");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 4:4, 4:0; 2:1, 2:2, 1:2, 1:1");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:0, 0:4, 4:4, 4:0; 2:1, 2:2, 1:2, 1:1; "
               "5:5, 5:6, 6:6, 6:5");
}

TEST(S2BooleanOperation, PolygonEdgesDegenerateAfterSnapping) {
  S2BooleanOperation::Options options = RoundToE(0);
  auto a = "# # 0:-1, 0:1, 0.1:1, 0.1:-1";
  auto b = "# # -1:0.1, 1:0.1, 1:0, -1:0";
  // When snapping causes an output edge to become degenerate, it is still
  // emitted (since otherwise loops that contract to a single point would be
  // lost).  If the output layer doesn't want such edges, they can be removed
  // via DegenerateEdges::DISCARD or DISCARD_EXCESS.
  ExpectResult(OpType::UNION, options, a, b,
               "# # 0:-1, 0:-1, 0:0, 0:1, 0:1, 0:0 | "
               "-1:0, -1:0, 0:0, 1:0, 1:0, 0:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
               "# # 0:0, 0:0, 0:0, 0:0");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
               "# # 0:-1, 0:-1, 0:0, 0:1, 0:1, 0:0 | 0:0, 0:0");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
               "# # 0:-1, 0:-1, 0:0, 0:1, 0:1, 0:0 | "
               "-1:0, -1:0, 0:0, 1:0, 1:0, 0:0 | 0:0, 0:0, 0:0, 0:0");
}

///////////////////////////////////////////////////////////////////////////
// The remaining tests are intended to cover combinations of features or
// interesting special cases.

TEST(S2BooleanOperation, ThreeOverlappingBars) {
  // Two vertical bars and a horizontal bar that overlaps both of the other
  // bars and connects them.

  // Round intersection points to E2 precision because the expected results
  // were computed in lat/lng space rather than using geodesics.
  S2BooleanOperation::Options options = RoundToE(2);
  auto a = "# # 0:0, 0:2, 3:2, 3:0; 0:3, 0:5, 3:5, 3:3";
  auto b = "# # 1:1, 1:4, 2:4, 2:1";
  ExpectResult(OpType::UNION, options, a, b,
      "# # 0:0, 0:2, 1:2, 1:3, 0:3, 0:5, 3:5, 3:3, 2:3, 2:2, 3:2, 3:0");
  ExpectResult(OpType::INTERSECTION, options, a, b,
      "# # 1:1, 1:2, 2:2, 2:1; 1:3, 1:4, 2:4, 2:3");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
      "# # 0:0, 0:2, 1:2, 1:1, 2:1, 2:2, 3:2, 3:0; "
      "0:3, 0:5, 3:5, 3:3, 2:3, 2:4, 1:4, 1:3");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
      "# # 0:0, 0:2, 1:2, 1:1, 2:1, 2:2, 3:2, 3:0; "
      "0:3, 0:5, 3:5, 3:3, 2:3, 2:4, 1:4, 1:3; "
      "1:2, 1:3, 2:3, 2:2");
}

TEST(S2BooleanOperation, FourOverlappingBars) {
  // Two vertical bars and two horizontal bars.

  // Round intersection points to E2 precision because the expected results
  // were computed in lat/lng space rather than using geodesics.
  S2BooleanOperation::Options options = RoundToE(2);
  auto a = "# # 1:88, 1:93, 2:93, 2:88; -1:88, -1:93, 0:93, 0:88";
  auto b = "# # -2:89, -2:90, 3:90, 3:89; -2:91, -2:92, 3:92, 3:91";
  ExpectResult(OpType::UNION, options, a, b,
      "# # -1:88, -1:89, -2:89, -2:90, -1:90, -1:91, -2:91, -2:92, -1:92, "
      "-1:93, 0:93, 0:92, 1:92, 1:93, 2:93, 2:92, 3:92, 3:91, 2:91, "
      "2:90, 3:90, 3:89, 2:89, 2:88, 1:88, 1:89, 0:89, 0:88; "
      "0:90, 1:90, 1:91, 0:91" /*CW*/ );
  ExpectResult(OpType::INTERSECTION, options, a, b,
      "# # 1:89, 1:90, 2:90, 2:89; 1:91, 1:92, 2:92, 2:91; "
      "-1:89, -1:90, 0:90, 0:89; -1:91, -1:92, 0:92, 0:91");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
      "# # 1:88, 1:89, 2:89, 2:88; 1:90, 1:91, 2:91, 2:90; "
      "1:92, 1:93, 2:93, 2:92; -1:88, -1:89, 0:89, 0:88; "
      "-1:90, -1:91, 0:91, 0:90; -1:92, -1:93, 0:93, 0:92");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
      "# # 1:88, 1:89, 2:89, 2:88; -1:88, -1:89, 0:89, 0:88; "
      "1:90, 1:91, 2:91, 2:90; -1:90, -1:91, 0:91, 0:90; "
      "1:92, 1:93, 2:93, 2:92; -1:92, -1:93, 0:93, 0:92; "
      "-2:89, -2:90, -1:90, -1:89; -2:91, -2:92, -1:92, -1:91; "
      "0:89, 0:90, 1:90, 1:89; 0:91, 0:92, 1:92, 1:91; "
      "2:89, 2:90, 3:90, 3:89; 2:91, 2:92, 3:92, 3:91");
}

TEST(S2BooleanOperation, OverlappingDoughnuts) {
  // Two overlapping square doughnuts whose holes do not overlap.
  // This means that the union polygon has only two holes rather than three.

  // Round intersection points to E2 precision because the expected results
  // were computed in lat/lng space rather than using geodesics.
  S2BooleanOperation::Options options = RoundToE(1);
  auto a = "# # -1:-93, -1:-89, 3:-89, 3:-93; "
                      "0:-92, 2:-92, 2:-90, 0:-90" /*CW*/ ;
  auto b = "# # -3:-91, -3:-87, 1:-87, 1:-91; "
                      "-2:-90, 0:-90, 0:-88, -2:-88" /*CW*/ ;
  ExpectResult(OpType::UNION, options, a, b,
      "# # -1:-93, -1:-91, -3:-91, -3:-87, 1:-87, 1:-89, 3:-89, 3:-93; "
      "0:-92, 2:-92, 2:-90, 1:-90, 1:-91, 0:-91; " /*CW */
      "-2:-90, -1:-90, -1:-89, 0:-89, 0:-88, -2:-88" /* CW */ );
  ExpectResult(OpType::INTERSECTION, options, a, b,
      "# # -1:-91, -1:-90, 0:-90, 0:-91; "
      "0:-90, 0:-89, 1:-89, 1:-90");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
      "# # -1:-93, -1:-91, 0:-91, 0:-92, 2:-92, "
      "2:-90, 1:-90, 1:-89, 3:-89, 3:-93; "
      "-1:-90, -1:-89, 0:-89, 0:-90");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
      "# # -1:-93, -1:-91, 0:-91, 0:-92, 2:-92, "
      "2:-90, 1:-90, 1:-89, 3:-89, 3:-93; "
      "-3:-91, -3:-87, 1:-87, 1:-89, 0:-89, 0:-88,-2:-88,-2:-90,-1:-90,-1:-91; "
      "-1:-90, -1:-89, 0:-89, 0:-90; "
      "1:-91, 0:-91, 0:-90, 1:-90");
}

TEST(S2BooleanOperation, PolylineEnteringRectangle) {
  // A polyline that enters a rectangle very close to one of its vertices.
  S2BooleanOperation::Options options = RoundToE(1);
  auto a = "# 0:0, 2:2 #";
  auto b = "# # 1:1, 1:3, 3:3, 3:1";
  ExpectResult(OpType::UNION, options, a, b,
      "# 0:0, 1:1 # 1:1, 1:3, 3:3, 3:1");
  ExpectResult(OpType::INTERSECTION, options, a, b,
      "# 1:1, 2:2 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
      "# 0:0, 1:1 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
      "# 0:0, 1:1 # 1:1, 1:3, 3:3, 3:1");
}

TEST(S2BooleanOperation, PolylineCrossingRectangleTwice) {
  // A polyline that crosses a rectangle in one direction, then moves to a
  // different side and crosses the rectangle in the other direction.  Note
  // that an extra vertex is added where the two polyline edges cross.
  S2BooleanOperation::Options options = RoundToE(1);
  auto a = "# 0:-5, 0:5, 5:0, -5:0 #";
  auto b = "# # 1:1, 1:-1, -1:-1, -1:1";
  ExpectResult(OpType::UNION, options, a, b,
      "# 0:-5, 0:-1 | 0:1, 0:5, 5:0, 1:0 | -1:0, -5:0 "
      "# 1:1, 1:0, 1:-1, 0:-1, -1:-1, -1:0, -1:1, 0:1");
  ExpectResult(OpType::INTERSECTION, options, a, b,
      "# 0:-1, 0:0, 0:1 | 1:0, 0:0, -1:0 #");
  ExpectResult(OpType::DIFFERENCE, options, a, b,
      "# 0:-5, 0:-1 | 0:1, 0:5, 5:0, 1:0 | -1:0, -5:0 #");
  ExpectResult(OpType::SYMMETRIC_DIFFERENCE, options, a, b,
      "# 0:-5, 0:-1 | 0:1, 0:5, 5:0, 1:0 | -1:0, -5:0 "
      "# 1:1, 1:0, 1:-1, 0:-1, -1:-1, -1:0, -1:1, 0:1");
}
