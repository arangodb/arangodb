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

#include "s2/s2builderutil_get_snapped_winding_delta.h"

#include <iostream>

#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2cap.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2lax_loop_shape.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::StrCat;
using absl::make_unique;
using std::string;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using EdgeId = Graph::EdgeId;
using InputEdgeId = Graph::InputEdgeId;
using VertexId = Graph::VertexId;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

namespace {

// This S2Builder layer simply calls s2builderutil::GetSnappedWindingDelta()
// with the given "ref_input_edge_id" and compares the result to
// "expected_winding_delta".
class WindingNumberComparingLayer : public S2Builder::Layer {
 public:
  explicit WindingNumberComparingLayer(InputEdgeId ref_input_edge_id,
                                       const S2Builder* builder,
                                       int expected_winding_delta)
      : ref_input_edge_id_(ref_input_edge_id), builder_(*builder),
        expected_winding_delta_(expected_winding_delta) {
  }

  GraphOptions graph_options() const override {
    return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                        DuplicateEdges::MERGE, SiblingPairs::KEEP);
  }

  void Build(const Graph& g, S2Error* error) override {
  // Find the reference vertex before and after snapping.
    S2Point ref_in = builder_.input_edge(ref_input_edge_id_).v0;
    VertexId ref_v = s2builderutil::FindFirstVertexId(ref_input_edge_id_, g);
    S2_DCHECK_GE(ref_v, 0);
    int winding_delta = s2builderutil::GetSnappedWindingDelta(
        ref_in, ref_v, s2builderutil::InputEdgeFilter{}, builder_, g, error);
    S2_CHECK(error->ok()) << *error;
    EXPECT_EQ(winding_delta, expected_winding_delta_);
  }

 private:
  InputEdgeId ref_input_edge_id_;
  const S2Builder& builder_;
  int expected_winding_delta_;
};


// Given a set of loops, a set of forced vertices, and a snap radius in
// degrees, verifies that the change in winding number computed by
// s2builderutil::GetSnappedWindingDelta() for the degenerate edge
// "ref_input_edge_id" is "expected_winding_delta".
void ExpectWindingDelta(
    const string& loops_str, const string& forced_vertices_str,
    double snap_radius_degrees, InputEdgeId ref_input_edge_id,
    int expected_winding_delta) {
  S2Builder builder{S2Builder::Options{s2builderutil::IdentitySnapFunction{
        S1Angle::Degrees(snap_radius_degrees)}}};
  builder.StartLayer(make_unique<WindingNumberComparingLayer>(
      ref_input_edge_id, &builder, expected_winding_delta));
  for (const auto& v : s2textformat::ParsePointsOrDie(forced_vertices_str)) {
    builder.ForceVertex(v);
  }
  builder.AddShape(*s2textformat::MakeLaxPolygonOrDie(loops_str));
  S2Shape::Edge ref_edge = builder.input_edge(ref_input_edge_id);
  S2_DCHECK(ref_edge.v0 == ref_edge.v1) << "Reference edge not degenerate";
  S2Error error;
  EXPECT_TRUE(builder.Build(&error)) << error;
}

// Since the GetSnappedWindingDelta() algorithm doesn't depend on absolute
// vertex positions, we use "0:0" as the snapped vertex position in the tests
// below (using ForceVertex() to ensure this).  Most tests use a snap radius
// of 10 degrees since this makes it convenient to construct suitable tests.
//
// FYI the S2::RefDir() direction for "0:0" (used to determine whether a loop
// contains one of its vertices) is just slightly north of due west,
// i.e. approximately 179.7 degrees CCW from the positive longitude axis.

// No edges except the degenerate edges that defines the reference vertex.
TEST(GetSnappedWindingDelta, NoOtherEdges) {
  ExpectWindingDelta("0:0", "0:0", 10.0, 0, 0);
}

// Degenerate input loops.
TEST(GetSnappedWindingDelta, DegenerateInputLoops) {
  ExpectWindingDelta("0:0; 1:1; 2:2", "0:0", 10.0, 0, 0);
}

// Duplicate degenerate input loops.
TEST(GetSnappedWindingDelta, DuplicateDegenerateInputLoops) {
  ExpectWindingDelta("0:0; 0:0; 1:1; 1:1", "0:0", 10.0, 0, 0);
}

// A shell around the reference vertex that collapses to a single point.
TEST(GetSnappedWindingDelta, CollapsingShell) {
  ExpectWindingDelta("0:0; 1:1, 1:-2, -2:1", "0:0", 10.0, 0, -1);
}

// A hole around the reference vertex that collapses to a single point.
TEST(GetSnappedWindingDelta, CollapsingHole) {
  ExpectWindingDelta("0:0; 1:1, -2:1, 1:-2", "0:0", 10.0, 0, +1);
}

// A single "shell" that winds around the reference vertex twice.
TEST(GetSnappedWindingDelta, CollapsingDoubleShell) {
  ExpectWindingDelta("0:0; 1:1, 1:-2, -2:1, 2:2, 2:-3, -3:2",
                     "0:0", 10.0, 0, -2);
}

// A loop that enters the Voronoi region of the snapped reference vertex and
// then leaves again, where the reference vertex is not contained by the loop
// and does not move during snapping.
TEST(GetSnappedWindingDelta, ExternalLoopRefVertexStaysOutside) {
  ExpectWindingDelta("0:0; 20:0, 0:0, 0:20", "0:0", 10.0, 0, 0);
}

// Like the above, except that the reference vertex is contained by the loop.
// (See S2::RefDir comments above.)
TEST(GetSnappedWindingDelta, ExternalLoopRefVertexStaysInside) {
  ExpectWindingDelta("0:0; 0:-20, 0:0, 20:0", "0:0", 10.0, 0, 0);
}

// The reference vertex moves from outside to inside an external loop during
// snapping.
TEST(GetSnappedWindingDelta, ExternalLoopRefVertexMovesInside) {
  ExpectWindingDelta("1:1; 0:-20, 1:-1, 20:0", "0:0", 10.0, 0, +1);
}

// A single loop edge crosses the Voronoi region of the reference vertex and the
// reference vertex stays outside the loop during snapping.
TEST(GetSnappedWindingDelta, CrossingEdgeRefVertexStaysOutside) {
  ExpectWindingDelta("-1:-1; 20:-20, -20:20, 20:20", "0:0", 10.0, 0, 0);
}

// A single loop edge crosses the Voronoi region of the reference vertex and the
// reference vertex moves outside the loop during snapping.
TEST(GetSnappedWindingDelta, CrossingEdgeRefVertexMovesOutside) {
  ExpectWindingDelta("1:1; 20:-20, -20:20, 20:20", "0:0", 10.0, 0, -1);
}

// An external loop that winds CW around the reference vertex twice, where the
// reference vertex moves during snapping, and where the reference vertex is
// outside the loop after snapping (so that its winding number only increases
// by 1).
TEST(GetSnappedWindingDelta, ExternalLoopDoubleHoleToSingleHole) {
  ExpectWindingDelta("4:4; 0:20, 3:3, 6:3, 2:7, 2:2, 2:20", "0:0", 10.0, 0, +1);
}

// An external loop that winds CW around the reference vertex twice, where the
// reference vertex moves during snapping, and where the reference vertex is
// inside the loop after snapping (so that its winding number increases by 3).
TEST(GetSnappedWindingDelta, ExternalLoopDoubleHoleToSingleShell) {
  ExpectWindingDelta("4:4; 0:-20, 6:2, 2:6, 2:2, 6:2, 2:6, 2:2, 20:0",
                     "0:0", 10.0, 0, +3);
}

// This and the following tests vertify that the partial loops formed by the
// local input and output edges are closed consistently with each other (such
// that the hypothetical connecting edges can deform from one to the other
// without passing through the reference vertex).
//
// An external loop where the input edges that enter/exit the Voronoi region
// cross, but the snapped edges do not.  (This can happen when the input edges
// snap to multiple edges and the crossing occurs outside the Voronoi region
// of the reference vertex.)  In this particular test, the entering/exiting
// edges snap to the same adjacent Voronoi site so that the snapped edges form
// a loop with one external vertex.
TEST(GetSnappedWindingDelta, ExternalEdgesCrossSnapToSameVertex) {
  ExpectWindingDelta("1:1; -5:30, 7:-3, -7:-3, 5:30",
                     "0:0, 0:15", 10.0, 0, -1);
}

// This test is similar except that the entering/exiting edges snap to two
// different external Voronoi sites.  Again, the input edges cross but the
// snapped edges do not.
TEST(GetSnappedWindingDelta, ExternalEdgesCrossSnapToDifferentVertices) {
  ExpectWindingDelta("1:1; -5:40, 7:-3, -7:-3, 5:40",
                     "0:0, 6:10, -6:10", 10.0, 0, -1);
}

// Test cases where the winding numbers of the reference points Za and Zb in
// the algorithm description change due to snapping.  (The points Za and Zb
// are the centers of the great circles defined by the first and last input
// edges.)  For their winding numbers to change, the input loop needs to cross
// these points as it deforms during snapping.
//
// In all the test below we use perturbations of 70:-180, 5:0 as the first
// input edge (which yields points close to 0:90 as Za) and perturbations of
// 0:5, 0:110 as the last input edge (which yields points close to 90:0 as Zb).
// The first/last vertex can be adjusted slightly to control which side of each
// edge Za/Zb is on.
TEST(GetSnappedWindingDelta, ReferencePointWindingNumbersChange) {
  // Winding number of Za ~= 0.01:90 changes.
  ExpectWindingDelta("1:1; 70:-179.99, 5:0, 0:5, -0.01:110",
                     "0:0, 1:90", 10.0, 0, 0);

  // Winding number of Zb ~= 89.99:90 changes.
  ExpectWindingDelta("1:1; 70:-179.99, 5:0, 0:5, -0.01:110",
                     "0:0, 89:90", 10.0, 0, 0);

  // Winding numbers of Za and Zb both change.
  ExpectWindingDelta("1:1; 70:-179.99, 5:0, 0:5, -0.01:110",
                     "0:0, 1:90, 89:90", 10.0, 0, 0);

  // Winding number of Za ~= -0.01:90 changes in the opposite direction.
  ExpectWindingDelta("1:1; 70:179.99, 5:0, 0:5, 0:110",
                     "0:0, -1:20, 1:90", 10.0, 0, 0);
}

// This test demonstrates that a connecting vertex may be necessary in order
// to ensure that the loops L and L' used to compute the change in winding
// number for reference points Za and Zb do not pass through those points
// during snapping.  (This can only happen when the edges A0A1 or B0B1 snap
// to an edge chain longer than 180 degrees, i.e. where the shortest edge
// between their new endpoints goes the wrong way around the sphere.)
TEST(GetSnappedWindingDelta, ReferenceLoopsTopologicallyConsistent) {
  // A0A1 follows the equator, Za is at the north pole.  A0A1 snaps to the
  // polyline (0:148, 0:74, 44:-39, -31:-48), where the last two vertices are
  // A0' and R' respectively.  (The perpendicular bisector of A0' and R' just
  // barely intersects A0A1 and therefore it snaps to both vertices.)  A
  // connecting vertex is needed between A0 = 0:148 and A0' = 44:-39 to ensure
  // that this edge stays within the snap radius of A0A1.
  ExpectWindingDelta("-45:24; 0:148, 0:0, -31:-48, 44:-39, -59:0",
                     "-31:-48, 44:-39", 60.0, 0, -1);

  // This tests the symmetric case where a connecting vertex is needed between
  // B1 and B1' to ensure that the edge stays within the snap radius of B0B1.
  ExpectWindingDelta("-45:24;  -59:0, 44:-39, -31:-48, 0:0, 0:148",
                     "-31:-48, 44:-39", 60.0, 0, 1);
}

// A complex example with multiple loops that combines many of the situations
// tested individually above.
TEST(GetSnappedWindingDelta, ComplexExample) {
  ExpectWindingDelta("1:1; "
                     "70:179.99, 5:0, 0:5, 0:110; "
                     "70:179.99, 0:0, 0:3, 3:0, 0:-1, 0:110; "
                     "10:-10, -10:10, 10:10; "
                     "2:2, 1:-2, -1:2, 2:2, 1:-2, -1:2 ",
                     "0:0, -1:90, 1:90, 45:-5", 10.0, 0, -5);
}

// This test demonstrates the necessity of the algorithm step that reverses
// the sign of Za, Zb if necessary to point away from the Voronoi site R'.
// Further examples can be generated by running RandomLoops test below for
// enough iterations.
TEST(GetSnappedWindingDelta, EnsureZaZbNotInVoronoiRegion) {
  ExpectWindingDelta(
      "30:42, 30:42; -27:52, 66:131, 30:-93", "", 67.0, 0, -1);
}

// This test demonstrates the necessity of closing the "chain_diff" loop used
// by the algorithm.  Further examples can be found by running RandomLoops.
TEST(GetSnappedWindingDelta, EnsureChainDiffLoopIsClosed) {
  ExpectWindingDelta(
      "8:26, 8:26; -36:70, -64:-35, -41:48", "", 66, 0, 0);
}

// This test previously failed due to a bug in GetVoronoiSiteExclusion()
// involving long edges (near 180 degrees) and large snap radii.
TEST(GetSnappedWindingDelta, VoronoiExclusionBug) {
  ExpectWindingDelta(
      "24.97:102.02, 24.97:102.02; "
      "25.84:131.46, -29.23:-166.58, 29.40:173.03, -18.02:-5.83",
      "", 64.83, 0, -1);
}

// Used to build a histogram of winding numbers.
using WindingTally = absl::btree_map<int, int>;

// This S2Builder::Layer checks that the change in winding number due to
// snapping computed by s2builderutil::GetSnappedWindingDelta() is correct for
// the given configuration of input edges.
//
// "ref_input_edge_id" should be a degenerate edge SS that specifies the
// reference vertex R whose change in winding number is verified.
//
// "isolated_input_edge_id" should be a degenerate edge II that is not
// expected to snap together with any other edges.  (This ensures that the
// winding number of its vertex I does not change due to snapping.)  I should
// be chosen to be as far away as possible from other vertices and edges used
// for testing purposes.  If more than one edge happens to snap to this
// vertex, S2Error::FAILED_PRECONDITION is returned.
class WindingNumberCheckingLayer : public S2Builder::Layer {
 public:
  explicit WindingNumberCheckingLayer(InputEdgeId ref_input_edge_id,
                                      InputEdgeId isolated_input_edge_id,
                                      const S2Builder* builder,
                                      WindingTally* winding_tally)
      : ref_input_edge_id_(ref_input_edge_id),
        isolated_input_edge_id_(isolated_input_edge_id),
        builder_(*builder), winding_tally_(winding_tally) {
  }

  GraphOptions graph_options() const override {
    // Some of the graph options are chosen randomly.
    return GraphOptions(
        EdgeType::DIRECTED, DegenerateEdges::KEEP,
        S2Testing::rnd.OneIn(2) ? DuplicateEdges::KEEP : DuplicateEdges::MERGE,
        S2Testing::rnd.OneIn(2) ? SiblingPairs::KEEP : SiblingPairs::CREATE);
  }

  void Build(const Graph& g, S2Error* error) override;

 private:
  InputEdgeId ref_input_edge_id_;
  InputEdgeId isolated_input_edge_id_;
  const S2Builder& builder_;
  WindingTally* winding_tally_;
};

void WindingNumberCheckingLayer::Build(const Graph& g, S2Error* error) {
  // First we locate the vertices R, R', I, I'.
  S2Point ref_in = builder_.input_edge(ref_input_edge_id_).v0;
  VertexId ref_v = s2builderutil::FindFirstVertexId(ref_input_edge_id_, g);
  S2Point ref_out = g.vertex(ref_v);

  S2Point iso_in = builder_.input_edge(isolated_input_edge_id_).v0;
  VertexId iso_v = s2builderutil::FindFirstVertexId(isolated_input_edge_id_, g);
  S2Point iso_out = g.vertex(iso_v);

  // If more than one edge snapped to the isolated vertex I, we skip this test
  // since we have no way to  independently verify correctness of the results.
  Graph::VertexOutMap out_map(g);
  if (out_map.degree(iso_v) != 1 ||
      g.input_edge_ids(*out_map.edge_ids(iso_v).begin()).size() != 1) {
    error->Init(S2Error::FAILED_PRECONDITION, "Isolated vertex not isolated");
    return;
  }

  // Next we compute the winding number of R relative to I by counting signed
  // edge crossings of the input edges, and the winding number of R' related
  // to I' by counting signed edge crossings of the output edges.  (In order
  // to support DuplicateEdges::MERGE and SiblingEdges::CREATE, we also need
  // to take into account the number of input edges that snapped to each
  // output edge.)
  int winding_in = 0;
  S2CopyingEdgeCrosser crosser(iso_in, ref_in);
  for (int e = 0; e < builder_.num_input_edges(); ++e) {
    S2Shape::Edge edge = builder_.input_edge(e);
    winding_in += crosser.SignedEdgeOrVertexCrossing(edge.v0, edge.v1);
  }
  int winding_out = 0;
  crosser.Init(iso_out, ref_out);
  for (EdgeId e = 0; e < g.num_edges(); ++e) {
    Graph::Edge edge = g.edge(e);
    winding_out += g.input_edge_ids(e).size() *
                   crosser.SignedEdgeOrVertexCrossing(g.vertex(edge.first),
                                                      g.vertex(edge.second));
  }

  // Finally, check that s2builderutil::GetSnappedWindingDelta() computes the
  // difference between the two (using only local snapping information).
  int winding_delta = s2builderutil::GetSnappedWindingDelta(
      ref_in, ref_v, s2builderutil::InputEdgeFilter{}, builder_, g, error);
  S2_CHECK(error->ok()) << *error;
  EXPECT_EQ(winding_delta, winding_out - winding_in);
  (*winding_tally_)[winding_delta] += 1;
}

TEST(GetSnappedWindingDelta, RandomLoops) {
  // Count the number of tests for each winding number result and also the
  // number of tests where the isolated vertex was not isolated, to verify
  // that the test is working as intended.
  constexpr int numIters = 1000;  // Passes with 10,000,000 iterations.
  int num_not_isolated = 0;
  WindingTally winding_tally;
  auto& rnd = S2Testing::rnd;
  for (int iter = 0; iter < numIters; ++iter) {
    S2Testing::rnd.Reset(iter + 1);  // For reproducability.
    SCOPED_TRACE(StrCat("Iteration ", iter));

    // Choose a random snap radius up to the allowable maximum.
    S1Angle snap_radius = rnd.RandDouble() *
                          S2Builder::SnapFunction::kMaxSnapRadius();
    S2Builder builder{S2Builder::Options{
        s2builderutil::IdentitySnapFunction{snap_radius}}};
    builder.StartLayer(make_unique<WindingNumberCheckingLayer>(
        0 /*ref_input_edge_id*/, 1 /*isolated_input_edge_id*/,
        &builder, &winding_tally));

    // Choose a random reference vertex, and an isolated vertex that is as far
    // away as possible.  (The small amount of perturbation reduces the number
    // of calls to S2::ExpensiveSign() and is not necessary for correctness.)
    S2Point ref = S2Testing::RandomPoint();
    S2Point isolated = (-ref + 1e-12 * S2::Ortho(ref)).Normalize();
    builder.AddEdge(ref, ref);            // Reference vertex edge.
    builder.AddEdge(isolated, isolated);  // Isolated vertex edge.

    // Now repeatedly build and add loops.  Loops consist of 1 or more random
    // vertices where approximately 2/3 are expected to be within snap_radius
    // of the reference vertex.  Some vertices are duplicates of previous
    // vertices.  Vertices are kept at least snap_radius away from the
    // isolated vertex to reduce the chance that edges will snap to it.
    // (This can still happen with long edges, or because the reference
    // vertex snapped to a new location far away from its original location.)
    vector<S2Point> vertices_used, loop;
    for (int num_loops = rnd.Uniform(5) + 1; --num_loops >= 0; ) {
      for (int num_vertices = rnd.Uniform(9) + 1; --num_vertices >= 0; ) {
        if (!vertices_used.empty() && rnd.OneIn(4)) {
          loop.push_back(vertices_used[rnd.Uniform(vertices_used.size())]);
        } else if (rnd.OneIn(3)) {
          loop.push_back(S2Testing::SamplePoint(
              S2Cap(ref, S1Angle::Radians(M_PI) - snap_radius)));
        } else {
          loop.push_back(S2Testing::SamplePoint(S2Cap(ref, snap_radius)));
        }
      }
      builder.AddShape(S2LaxLoopShape(loop));
      loop.clear();
    }
    S2Error error;
    if (!builder.Build(&error)) ++num_not_isolated;
  }
  // We expect that at most 20% of tests will result in an isolated vertex.
  EXPECT_LE(num_not_isolated, 0.2 * numIters);
  std::cerr << "Histogram of winding number deltas tested:\n";
  for (const auto& entry : winding_tally) {
    std::cerr << entry.first << " : " << entry.second << "\n";
  }
}

}  // namespace
