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
//
// The following algorithm would not be necessary with planar geometry, since
// then winding numbers could be computed by starting at a point at infinity
// (whose winding number is zero) and counting signed edge crossings.  However
// points at infinity do not exist on the sphere.
//
// Instead we compute the change in winding number of a reference vertex R
// using only the set of edges incident to the snapped reference vertex R'.
// Essentially this involves looking at the set of input edges that snapped to
// R' and assembling them into edge chains.  These edge chains can be divided
// into two categories:
//
// (1) Edge chains that are entirely contained by the Voronoi region of R'.
//     This means that the input edges form a closed loop where every vertex
//     snaps to R'.  We can compute the change in winding number due to this
//     loop by simply choosing a point Z outside the Voronoi region of R' and
//     computing the winding numbers of R and R' relative to Z.
//
// (2) Edge chains that enter the Voronoi region of R' and later leave it.  In
//     this case the input chain has the form C = (A0, A1, ..., B0, B1) where
//     A0 and B1 are outside the Voronoi region of R' and all other vertices
//     snap to R'.  In the vicinity of R' this input chain snaps to a chain C'
//     of the form (A0', R', B1') where A0' is the second-last vertex in the
//     snapped edge chain for A0A1 and B1' is the second vertex in the snapped
//     edge chain for B0B1.  In principle we handle this similarly to the case
//     above (by finding a point Z whose change in winding number is known,
//     and then counting signed edge crossings along ZR with respect to C and
//     along ZR' with respect to C').  However the details are more
//     complicated and are described in GetSnappedWindingDelta().
//
// The total change in winding number is simply the sum of the changes in
// winding number due to each of these edge chains.

#include "s2/s2builderutil_get_snapped_winding_delta.h"

#include <utility>
#include <vector>

#include "s2/base/logging.h"
#include "absl/types/span.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2edge_distances.h"

using absl::Span;
using std::make_pair;
using std::vector;

using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using EdgeId = Graph::EdgeId;
using InputEdgeId = Graph::InputEdgeId;
using VertexId = Graph::VertexId;

namespace s2builderutil {

namespace {

// An input edge may snap to zero, one, or two non-degenerate output edges
// incident to the reference vertex, consisting of at most one incoming and
// one outgoing edge.
//
// If v_in >= 0, an incoming edge to the reference vertex is present.
// If v_out >= 0, an outgoing edge from the reference vertex is present.
struct EdgeSnap {
  S2Shape::Edge input;
  VertexId v_in = -1;
  VertexId v_out = -1;
};

// A map that allows finding all the input edges that start at a given point.
using InputVertexEdgeMap = absl::btree_multimap<S2Point, EdgeSnap>;

// The winding number returned when a usage error is detected.
constexpr int kErrorResult = std::numeric_limits<int>::max();

bool BuildChain(
    VertexId ref_v, const Graph& g, InputVertexEdgeMap* input_vertex_edge_map,
    vector<S2Point>* chain_in, vector<S2Point>* chain_out, S2Error* error) {
  S2_DCHECK(chain_in->empty());
  S2_DCHECK(chain_out->empty());

  // First look for an incoming edge to the reference vertex.  (This will be
  // the start of a chain that eventually leads to an outgoing edge.)
  {
    auto it = input_vertex_edge_map->begin();
    for (; it != input_vertex_edge_map->end(); ++it) {
      const EdgeSnap& snap = it->second;
      if (snap.v_in >= 0) {
        chain_out->push_back(g.vertex(snap.v_in));
        break;
      }
    }
    if (it == input_vertex_edge_map->end()) {
      // Pick an arbitrary edge to start a closed loop.
      it = input_vertex_edge_map->begin();
    }
    EdgeSnap snap = it->second;
    input_vertex_edge_map->erase(it);

    chain_in->push_back(snap.input.v0);
    chain_in->push_back(snap.input.v1);
    chain_out->push_back(g.vertex(ref_v));
    if (snap.v_out >= 0) {
      // This input edge enters and immediately exits the Voronoi region.
      chain_out->push_back(g.vertex(snap.v_out));
      return true;
    }
  }

  // Now repeatedly add edges until the chain or loop is finished.
  while (chain_in->back() != chain_in->front()) {
    const auto& range = input_vertex_edge_map->equal_range(chain_in->back());
    if (range.first == range.second) {
      error->Init(S2Error::INVALID_ARGUMENT,
                  "Input edges (after filtering) do not form loops");
      return false;
    }
    EdgeSnap snap = range.first->second;
    input_vertex_edge_map->erase(range.first);
    chain_in->push_back(snap.input.v1);
    if (snap.v_out >= 0) {
      // The chain has exited the Voronoi region.
      chain_out->push_back(g.vertex(snap.v_out));
      break;
    }
  }
  return true;
}

// Returns the change in winding number along the edge AB with respect to the
// given edge chain.  This is simply the sum of the signed edge crossings.
int GetEdgeWindingDelta(const S2Point& a, const S2Point& b,
                        absl::Span<const S2Point> chain) {
  S2_DCHECK_GT(chain.size(), 0);

  int delta = 0;
  S2EdgeCrosser crosser(&a, &b, &chain[0]);
  for (int i = 1; i < chain.size(); ++i) {
    delta += crosser.SignedEdgeOrVertexCrossing(&chain[i]);
  }
  return delta;
}

// Given an input edge (B0, B1) that snaps to an edge chain (B0', B1', ...),
// returns a connecting vertex "Bc" that can be used as a substitute for the
// remaining snapped vertices "..." when computing winding numbers.  This
// requires that (1) the edge (B1', Bc) does not intersect the Voronoi region
// of B0', and (2) the edge chain (B0', B1', Bc, B1) always stays within the
// snap radius of the input edge (B0, B1).
S2Point GetConnector(const S2Point& b0, const S2Point& b1,
                     const S2Point& b1_snapped) {
  // If B1' within 90 degrees of B1, no connecting vertex is necessary.
  if (b1_snapped.DotProd(b1) >= 0) return b1;

  // Otherwise we use the point on (B0, B1) that is 90 degrees away from B1'.
  // This is sufficient to ensure conditions (1) and (2).
  S2Point x = S2::RobustCrossProd(b0, b1).CrossProd(b1_snapped).Normalize();
  return (x.DotProd(S2::Interpolate(b0, b1, 0.5)) >= 0) ? x : -x;
}

// Returns the set of incoming and outgoing edges incident to the given
// vertex.  This method takes time linear in the size of the graph "g";
// if you need to call this function many times then it is more efficient to
// use Graph::VertexOutMap and Graph::VertexInMap instead.
vector<EdgeId> GetIncidentEdgesBruteForce(VertexId v, const Graph& g) {
  vector<EdgeId> result;
  for (EdgeId e = 0; e < g.num_edges(); ++e) {
    if (g.edge(e).first == v || g.edge(e).second == v) {
      result.push_back(e);
    }
  }
  return result;
}

}  // namespace

int GetSnappedWindingDelta(
    const S2Point& ref_in, VertexId ref_v, Span<const EdgeId> incident_edges,
    const InputEdgeFilter& input_edge_filter, const S2Builder& builder,
    const Graph& g, S2Error* error) {
  S2_DCHECK(!builder.options().simplify_edge_chains());
  S2_DCHECK(g.options().edge_type() == S2Builder::EdgeType::DIRECTED);
  S2_DCHECK(g.options().degenerate_edges() == GraphOptions::DegenerateEdges::KEEP);
  S2_DCHECK(g.options().sibling_pairs() == GraphOptions::SiblingPairs::KEEP ||
         g.options().sibling_pairs() == GraphOptions::SiblingPairs::REQUIRE ||
         g.options().sibling_pairs() == GraphOptions::SiblingPairs::CREATE);

  // First we group all the incident edges by input edge id, to handle the
  // problem that input edges can map to either one or two snapped edges.
  absl::btree_map<InputEdgeId, EdgeSnap> input_id_edge_map;
  for (EdgeId e : incident_edges) {
    Graph::Edge edge = g.edge(e);
    for (InputEdgeId input_id : g.input_edge_ids(e)) {
      if (input_edge_filter && input_edge_filter(input_id)) continue;
      EdgeSnap* snap = &input_id_edge_map[input_id];
      snap->input = builder.input_edge(input_id);
      if (edge.first != ref_v) snap->v_in = edge.first;
      if (edge.second != ref_v) snap->v_out = edge.second;
    }
  }
  // Now we regroup the edges according to the reference vertex of the
  // corresponding input edge.  This makes it easier to assemble these edges
  // into (portions of) input edge loops.
  InputVertexEdgeMap input_vertex_edge_map;
  for (const auto& entry : input_id_edge_map) {
    const EdgeSnap& snap = entry.second;
    input_vertex_edge_map.insert(make_pair(snap.input.v0, snap));
  }

  // The position of the reference vertex after snapping.  In comments we will
  // refer to the reference vertex before and after snapping as R and R'.
  S2Point ref_out = g.vertex(ref_v);

  // Now we repeatedly assemble edges into an edge chain and determine the
  // change in winding number due to snapping of that edge chain.  These
  // values are summed to compute the final winding number delta.
  //
  // An edge chain is either a closed loop of input vertices where every
  // vertex snapped to the reference vertex R', or a partial loop such that
  // all interior vertices snap to R' while the first and last vertex do not.
  // Note that the latter includes the case where the first and last input
  // vertices in the chain are identical but do not snap to R'.
  //
  // Essentially we compute the winding number of the unsnapped reference
  // vertex R with respect to the input chain and the winding number of the
  // snapped reference vertex R' with respect to the output chain, and then
  // subtract them.  In the case of open chains, we compute winding numbers as
  // if the chains had been closed in a way that preserves topology while
  // snapping (i.e., in a way that does not the cause chain to pass through
  // the reference vertex as it continuously deforms from the input to the
  // output).
  //
  // Any changes to this code should be validated by running the RandomLoops
  // unit test with at least 10 million iterations.
  int winding_delta = 0;
  while (!input_vertex_edge_map.empty()) {
    vector<S2Point> chain_in, chain_out;
    if (!BuildChain(ref_v, g, &input_vertex_edge_map,
                    &chain_in, &chain_out, error)) {
      return kErrorResult;
    }
    if (chain_out.size() == 1) {
      // We have a closed chain C of input vertices such that every vertex
      // snaps to the reference vertex R'.  Therefore we can easily find a
      // point Z whose winding number is not affected by the snapping of C; it
      // just needs to be outside the Voronoi region of R'.  Since the snap
      // radius is at most 70 degrees, we can use a point 90 degrees away such
      // as S2::Ortho(R').
      //
      // We then compute the winding numbers of R and R' relative to Z.  We
      // compute the winding number of R by counting signed crossings of the
      // edge ZR, while the winding number of R' relative to Z is always zero
      // because the snapped chain collapses to a single point.
      S2_DCHECK_EQ(chain_out[0], ref_out);         // Snaps to R'.
      S2_DCHECK_EQ(chain_in[0], chain_in.back());  // Chain is a loop.
      S2Point z = S2::Ortho(ref_out);
      winding_delta += 0 - GetEdgeWindingDelta(z, ref_in, chain_in);
    } else {
      // We have an input chain C = (A0, A1, ..., B0, B1) that snaps to a
      // chain C' = (A0', R', B1'), where A0 and B1 are outside the Voronoi
      // region of R' and all other input vertices snap to R'.  This includes
      // the case where A0 == B1 and also the case where the input chain
      // consists of only two vertices.  Note that technically the chain C
      // snaps to a supersequence of C', since A0A1 snaps to a chain whose
      // last two vertices are (A0', R') and B0B1 snaps to a chain whose first
      // two vertices are (R', B1').  This implies that A0 does not
      // necessarily snap to A0', and similarly for B1 and B1'.
      //
      // Note that A0 and B1 can be arbitrarily far away from R'.  This makes
      // it difficult (on the sphere) to construct a point Z whose winding
      // number is guaranteed not to be affected by snapping the edges A0A1
      // and B0B1.  Instead we construct two points Za and Zb such that Za is
      // guaranteed not be affected by the snapping of A0A1, Zb is guaranteed
      // not to be affected by the snapping of B0B1, and both points are
      // guaranteed not to be affected by the snapping of any other input
      // edges.  We can then compute the change in winding number of Zb by
      // considering only the single edge A0A1 that snaps to A0'R'.
      // Furthermore we can compute the latter by using Za as the reference
      // point, since its winding number is guaranteed not be affected by this
      // particular edge.
      //
      // Given the point Zb, whose change in winding number is now known, we
      // can compute the change in winding number of the reference vertex R.
      // We essentially want to count the signed crossings of ZbR' with respect
      // to C' and subtract the signed crossings of ZbR with respect to C,
      // which we will write as s(ZbR', C') - s(ZbR, C).
      //
      // However to do this we need to close both chains in a way that is
      // topologically consistent and does not affect the winding number of
      // Zb.  This can be achieved by counting the signed crossings of ZbR' by
      // following the two-edge path (Zb, R, R').  In other words, we compute
      // this as s(ZbR, C') + s(RR', C') - s(ZbR, C).  We can then compute
      // s(ZbR, C') - s(ZbR, C) by simply concatenating the vertices of C in
      // reverse order to C' to form a single closed loop.  The remaining term
      // s(RR', C') can be implemented as signed edge crossing tests, or more
      // directly by testing whether R is contained by the wedge C'.
      S2_DCHECK_EQ(chain_out.size(), 3);
      S2_DCHECK_EQ(chain_out[1], ref_out);

      // Compute two points Za and Zb such that Za is not affected by the
      // snapping of any edge except possibly B0B1, and Zb is not affected by
      // the snapping of any edge except possibly A0A1.  Za and Zb are simply
      // the normals to the edges A0A1 and B0B1 respectively, with their sign
      // chosen to point away from the Voronoi site R'.  This ensures at least
      // 20 degrees of separation from all edges except the ones mentioned.
      S2Point za = S2::RobustCrossProd(chain_in[0], chain_in[1]).Normalize();
      S2Point zb = S2::RobustCrossProd(chain_in.end()[-2], chain_in.back())
                   .Normalize();
      if (za.DotProd(ref_out) > 0) za = -za;
      if (zb.DotProd(ref_out) > 0) zb = -zb;

      // We now want to determine the change in winding number of Zb due to
      // A0A1 snapping to A0'R'.  Conceptually we do this by closing these
      // two single-edge chains into loops L and L' and then computing
      // s(ZaZb, L') - s(ZaZb, L).  Recall that Za was constructed so as not
      // to be affected by the snapping of edge A0A1, however this is only
      // true provided that L can snap to L' without passing through Za.
      //
      // To achieve this we let L be the degenerate loop (A0, A1, A0), and L'
      // be the loop (A0', R', A1, A0, A0').  The only problem is that we need
      // to ensure that the edge A0A0' stays within 90 degrees of A0A1, since
      // otherwise when the latter edge snaps to the former it might pass
      // through Za.  (This problem arises because we only consider the last
      // two vertices (A0', R') that A0A1 snaps to.  If we used the full chain
      // of snapped vertices for A0A1 then L' would always stay within the
      // snap radius of this edge.)
      //
      // The simplest way to fix this problem is to insert a connecting vertex
      // Ac between A0 and A0'.  THis vertex acts as a proxy for the missing
      // snapped vertices, yielding the loop L' = (A0', R', A1, A0, Ac, A0').
      // The vertex Ac is located on the edge A0A1 and is at most 90 degrees
      // away from A0'.  This ensures that the chain (A0, Ac, A0') always
      // stays within the snap radius of the input edge A0A1.
      //
      // Similarly we insert a connecting vertex Bc between B0 and B1 to
      // ensure that the edge B1'B1 passes on the correct side of Zb.
      S2Point a0_connector = GetConnector(chain_in[1], chain_in[0],
                                          chain_out[0]);
      S2Point b1_connector = GetConnector(chain_in.end()[-2], chain_in.back(),
                                          chain_out[2]);

      // Compute the change in winding number for reference vertex Zb.  Note
      // that we must duplicate the first/last vertex of the loop since the
      // argument to GetEdgeWindingDelta() is a polyline.
      vector<S2Point> chain_z {chain_out[0], chain_out[1], chain_in[1],
                               chain_in[0], a0_connector, chain_out[0]};
      winding_delta += GetEdgeWindingDelta(za, zb, chain_z);

      // Compute the change in winding number of ZbR due to snapping C to C'.
      // As before, conceptually we do this by closing these chains into loops
      // L and L' such that L snaps to L' without passing through Zb.  Again
      // this can be done by concatenating the vertices of C' with the
      // reversed vertices of C, along with the two extra connecting vertices
      // Ac and Bc to ensure that L and L' pass on the same side of Za and Zb.
      // This yields the loop (A0', R', B1', Bc, B1, B0, ..., A1, A0, Ac, A0').
      vector<S2Point> chain_diff = chain_out;
      chain_diff.push_back(b1_connector);
      chain_diff.insert(chain_diff.end(), chain_in.rbegin(), chain_in.rend());
      chain_diff.push_back(a0_connector);
      chain_diff.push_back(chain_out[0]);  // Close the loop.
      winding_delta += GetEdgeWindingDelta(zb, ref_in, chain_diff);

      // Compute the change in winding number of RR' with respect to C' only.
      // (This could also be computed using two calls to s2pred::OrderedCCW.)
      S2_DCHECK_EQ(chain_out[1], ref_out);
      winding_delta += GetEdgeWindingDelta(ref_in, ref_out, chain_out);
    }
  }
  return winding_delta;
}

int GetSnappedWindingDelta(
    const S2Point& ref_in, S2Builder::Graph::VertexId ref_v,
    const InputEdgeFilter &input_edge_filter, const S2Builder& builder,
    const Graph& g, S2Error* error) {
  return GetSnappedWindingDelta(
      ref_in, ref_v, GetIncidentEdgesBruteForce(ref_v, g),
      input_edge_filter, builder, g, error);
}

VertexId FindFirstVertexId(InputEdgeId input_edge_id, const Graph& g) {
  // A given input edge snaps to a chain of output edges.  To determine which
  // output vertex the source of the given input edge snaps to, we must find
  // the first edge in this chain.
  //
  // The search below takes linear time in the number of edges; it can be done
  // more efficiently if duplicate edges are not being merged and the mapping
  // returned by Graph::GetInputEdgeOrder() is available.  The algorithm would
  // also be much simpler if input_edge_id were known to be degenerate.
  absl::btree_map<VertexId, int> excess_degree_map;
  for (EdgeId e = 0; e < g.num_edges(); ++e) {
    IdSetLexicon::IdSet id_set = g.input_edge_ids(e);
    for (InputEdgeId id : id_set) {
      if (id == input_edge_id) {
        excess_degree_map[g.edge(e).first] += 1;
        excess_degree_map[g.edge(e).second] -= 1;
        break;
      }
    }
  }
  if (excess_degree_map.empty()) return -1;  // Does not exist.

  // Look for the (unique) vertex whose excess degree is +1.
  for (const auto& entry : excess_degree_map) {
    if (entry.second == 1) return entry.first;
  }
  // Otherwise "input_edge_id" must snap to a single degenerate edge.
  S2_DCHECK_EQ(excess_degree_map.size(), 1);
  return excess_degree_map.begin()->first;
}

}  // namespace s2builderutil
