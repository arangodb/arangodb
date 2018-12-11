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
//
// Boolean operations are implemented by constructing the boundary of the
// result and then using S2Builder to assemble the edges.  The boundary is
// obtained by clipping each of the two input regions to the interior or
// exterior of the other region.  For example, to compute the union of A and
// B, we clip the boundary of A to the exterior of B and the boundary of B to
// the exterior of A; the resulting set of edges defines the union of the two
// regions.
//
// We use exact predicates, but inexact constructions (e.g. computing the
// intersection point of two edges).  Nevertheless, the following algorithm is
// guaranteed to be 100% robust, in that the computed boundary stays within a
// small tolerance (snap_radius + S2::kIntersectionError) of the exact
// result, and also preserves the correct topology (i.e., no crossing edges).
//
// Unfortunately this robustness cannot quite be achieved using the strategy
// outlined above (clipping the two input regions and assembling the
// resulting edges).  Since computed intersection points are not exact, the
// input geometry passed to S2Builder might contain self-intersections, and
// these self-intersections cannot be eliminated reliably by snap rounding.
//
// So instead, we pass S2Builder the entire set of input edges where at least
// some portion of each edge belongs to the output boundary.  We allow
// S2Builder to compute the intersection points and snap round the edges
// (which it does in a way that is guaranteed to preserve the input topology).
// Then once this is finished, we remove the portions of each edge that would
// have been clipped if we had done the clipping first.  This step only
// involves deciding whether to keep or discard each edge in the output, since
// all intersection points have already been resolved, and therefore there is
// no risk of creating new self-intersections.
//
// This is implemented using the following classes:
//
//  - S2BooleanOperation::Impl: the top-level class that clips each of
//                              the two regions to the other region.
//
//  - CrossingProcessor: a class that processes edge crossings and maintains
//                       the necessary state in order to clip the boundary
//                       of one region to the interior or exterior of the
//                       other region.
//
//  - EdgeClippingLayer: an S2Builder::Layer that removes graph edges that
//                       correspond to clipped portions of input edges, and
//                       passes the result to another layer for assembly.
//
//  - GraphEdgeClipper: a helper class that does the actual work of the
//                      EdgeClippingLayer.

#include "s2/s2boolean_operation.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2builder.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2crossing_edge_query.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2measures.h"
#include "s2/s2predicates.h"
#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"
#include "s2/util/gtl/btree_map.h"

// TODO(ericv): Remove this debugging output at some point.
extern bool s2builder_verbose;

namespace {  // Anonymous namespace for helper classes.

using absl::make_unique;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::swap;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using SnapFunction = S2Builder::SnapFunction;
using GraphOptions = S2Builder::GraphOptions;
using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using Graph = S2Builder::Graph;
using EdgeId = Graph::EdgeId;
using VertexId = Graph::VertexId;
using InputEdgeId = Graph::InputEdgeId;
using InputEdgeIdSetId = Graph::InputEdgeIdSetId;

using PolygonModel = S2BooleanOperation::PolygonModel;
using PolylineModel = S2BooleanOperation::PolylineModel;
using Precision = S2BooleanOperation::Precision;

// A collection of special InputEdgeIds that allow the GraphEdgeClipper state
// modifications to be inserted into the list of edge crossings.
static const InputEdgeId kSetInside = -1;
static const InputEdgeId kSetInvertB = -2;
static const InputEdgeId kSetReverseA = -3;

// CrossingInputEdge represents an input edge B that crosses some other input
// edge A.  It stores the input edge id of edge B and also whether it crosses
// edge A from left to right (or vice versa).
class CrossingInputEdge {
 public:
  // Indicates that input edge "input_id" crosses another edge (from left to
  // right if "left_to_right" is true).
  CrossingInputEdge(InputEdgeId input_id, bool left_to_right)
      : left_to_right_(left_to_right), input_id_(input_id) {
  }

  InputEdgeId input_id() const { return input_id_; }
  bool left_to_right() const { return left_to_right_; }

  bool operator<(const CrossingInputEdge& other) const {
    return input_id_ < other.input_id_;
  }
  bool operator<(const InputEdgeId& other) const {
    return input_id_ < other;
  }

 private:
  bool left_to_right_ : 1;
  InputEdgeId input_id_ : 31;
};

// InputEdgeCrossings represents all pairs of intersecting input edges.
// It is sorted in lexicographic order.
using InputEdgeCrossings = vector<pair<InputEdgeId, CrossingInputEdge>>;

// Given two input edges A and B that intersect, suppose that A maps to a
// chain of snapped edges A_0, A_1, ..., A_m and B maps to a chain of snapped
// edges B_0, B_1, ..., B_n.  CrossingGraphEdge represents an edge from chain
// B that shares a vertex with chain A.  It is used as a temporary data
// representation while processing chain A.  The arguments are:
//
//   "id" - the Graph::EdgeId of an edge from chain B.
//   "a_index" - the index of the vertex (A_i) that is shared with chain A.
//   "outgoing" - true if the shared vertex is the first vertex of the B edge.
//   "dst" - the Graph::VertexId of the vertex that is not shared with chain A.
//
// Note that if an edge from the B chain shares both vertices with the A
// chain, there will be two entries: an outgoing edge that treats its first
// vertex as being shared, and an incoming edge that treats its second vertex
// as being shared.
struct CrossingGraphEdge {
  CrossingGraphEdge(EdgeId _id, int _a_index, bool _outgoing, VertexId _dst)
      : id(_id), a_index(_a_index), outgoing(_outgoing), dst(_dst) {
  }
  EdgeId id;
  int a_index;
  bool outgoing;
  VertexId dst;
};
using CrossingGraphEdgeVector = absl::InlinedVector<CrossingGraphEdge, 2>;

// Returns a vector of EdgeIds sorted by input edge id.  When more than one
// output edge has the same input edge id (i.e., the input edge snapped to a
// chain of edges), the edges are sorted so that they form a directed edge
// chain.
//
// This function could possibily be moved to S2Builder::Graph, but note that
// it has special requirements.  Namely, duplicate edges and sibling pairs
// must be kept in order to ensure that every output edge corresponds to
// exactly one input edge.  (See also S2Builder::Graph::GetInputEdgeOrder.)
static vector<EdgeId> GetInputEdgeChainOrder(
    const Graph& g, const vector<InputEdgeId>& input_ids) {

  S2_DCHECK(g.options().edge_type() == EdgeType::DIRECTED);
  S2_DCHECK(g.options().duplicate_edges() == DuplicateEdges::KEEP);
  S2_DCHECK(g.options().sibling_pairs() == SiblingPairs::KEEP);

  // First, sort the edges so that the edges corresponding to each input edge
  // are consecutive.  (Each input edge was snapped to a chain of output
  // edges, or two chains in the case of undirected input edges.)
  vector<EdgeId> order = g.GetInputEdgeOrder(input_ids);

  // Now sort the group of edges corresponding to each input edge in edge
  // chain order (e.g.  AB, BC, CD).
  vector<pair<VertexId, EdgeId>> vmap;     // Map from source vertex to edge id.
  vector<int> indegree(g.num_vertices());  // Restricted to current input edge.
  for (int end, begin = 0; begin < order.size(); begin = end) {
    // Gather the edges that came from a single input edge.
    InputEdgeId input_id = input_ids[order[begin]];
    for (end = begin; end < order.size(); ++end) {
      if (input_ids[order[end]] != input_id) break;
    }
    if (end - begin == 1) continue;

    // Build a map from the source vertex of each edge to its edge id,
    // and also compute the indegree at each vertex considering only the edges
    // that came from the current input edge.
    for (int i = begin; i < end; ++i) {
      EdgeId e = order[i];
      vmap.push_back(make_pair(g.edge(e).first, e));
      indegree[g.edge(e).second] += 1;
    }
    std::sort(vmap.begin(), vmap.end());

    // Find the starting edge for building the edge chain.
    EdgeId next = g.num_edges();
    for (int i = begin; i < end; ++i) {
      EdgeId e = order[i];
      if (indegree[g.edge(e).first] == 0) next = e;
    }
    // Build the edge chain.
    for (int i = begin; ;) {
      order[i] = next;
      VertexId v = g.edge(next).second;
      indegree[v] = 0;  // Clear as we go along.
      if (++i == end) break;
      auto out = lower_bound(vmap.begin(), vmap.end(), make_pair(v, 0));
      S2_DCHECK_EQ(v, out->first);
      next = out->second;
    }
    vmap.clear();
  }
  return order;
}

// Given a set of clipping instructions encoded as a set of InputEdgeCrossings,
// GraphEdgeClipper determines which graph edges correspond to clipped
// portions of input edges and removes them.
//
// The clipping model is as follows.  The input consists of edge chains.  The
// clipper maintains an "inside" boolean state as it clips each chain, and
// toggles this state whenever an input edge is crossed.  Any edges that are
// deemed to be "outside" after clipping are removed.
//
// The "inside" state can be reset when necessary (e.g., when jumping to the
// start of a new chain) by adding a special crossing marked kSetInside.
// There are also two other special "crossings" that modify the clipping
// parameters: kSetInvertB specifies that edges should be clipped to the
// exterior of the other region, and kSetReverseA specifies that edges should
// be reversed before emitting them (which is needed to implement difference
// operations).
class GraphEdgeClipper {
 public:
  // "input_dimensions" is a vector specifying the dimension of each input
  // edge (0, 1, or 2).  "input_crossings" is the set of all crossings to be
  // used when clipping the edges of "g", sorted in lexicographic order.
  //
  // The clipped set of edges and their corresponding set of input edge ids
  // are returned in "new_edges" and "new_input_edge_ids".  (These can be used
  // to construct a new S2Builder::Graph.)
  GraphEdgeClipper(const Graph& g, const vector<int8>& input_dimensions,
                   const InputEdgeCrossings& input_crossings,
                   vector<Graph::Edge>* new_edges,
                   vector<InputEdgeIdSetId>* new_input_edge_ids);
  void Run();

 private:
  void AddEdge(Graph::Edge edge, InputEdgeId input_edge_id);
  void GatherIncidentEdges(
      const vector<VertexId>& a, int ai,
      const vector<CrossingInputEdge>& b_input_edges,
      vector<CrossingGraphEdgeVector>* b_edges) const;
  int GetCrossedVertexIndex(
      const vector<VertexId>& a, const CrossingGraphEdgeVector& b,
      bool left_to_right) const;
  int GetVertexRank(const CrossingGraphEdge& e) const;
  bool EdgeChainOnLeft(const vector<VertexId>& a,
                       EdgeId b_first, EdgeId b_last) const;

  const Graph& g_;
  Graph::VertexInMap in_;
  Graph::VertexOutMap out_;
  const vector<int8>& input_dimensions_;
  const InputEdgeCrossings& input_crossings_;
  vector<Graph::Edge>* new_edges_;
  vector<InputEdgeIdSetId>* new_input_edge_ids_;

  // Every graph edge is associated with exactly one input edge in our case,
  // which means that we can declare g_.input_edge_id_set_ids() as a vector of
  // InputEdgeIds rather than a vector of InputEdgeIdSetIds.  (This also takes
  // advantage of the fact that IdSetLexicon represents a singleton set as the
  // value of its single element.)
  const vector<InputEdgeId>& input_ids_;

  vector<EdgeId> order_;  // Graph edges sorted in input edge id order.
  vector<int> rank_;      // The rank of each graph edge within order_.
};

GraphEdgeClipper::GraphEdgeClipper(
    const Graph& g, const vector<int8>& input_dimensions,
    const InputEdgeCrossings& input_crossings,
    vector<Graph::Edge>* new_edges,
    vector<InputEdgeIdSetId>* new_input_edge_ids)
    : g_(g), in_(g), out_(g),
      input_dimensions_(input_dimensions),
      input_crossings_(input_crossings),
      new_edges_(new_edges),
      new_input_edge_ids_(new_input_edge_ids),
      input_ids_(g.input_edge_id_set_ids()),
      order_(GetInputEdgeChainOrder(g_, input_ids_)),
      rank_(order_.size()) {
  for (int i = 0; i < order_.size(); ++i) {
    rank_[order_[i]] = i;
  }
}

inline void GraphEdgeClipper::AddEdge(Graph::Edge edge,
                                      InputEdgeId input_edge_id) {
  new_edges_->push_back(edge);
  new_input_edge_ids_->push_back(input_edge_id);
}

void GraphEdgeClipper::Run() {
  // Declare vectors here and reuse them to avoid reallocation.
  vector<VertexId> a_vertices;
  vector<int> a_num_crossings;
  vector<bool> a_isolated;
  vector<CrossingInputEdge> b_input_edges;
  vector<CrossingGraphEdgeVector> b_edges;

  bool inside = false;
  bool invert_b = false;
  bool reverse_a = false;
  auto next = input_crossings_.begin();
  for (int i = 0; i < order_.size(); ++i) {
    // For each input edge (the "A" input edge), gather all the input edges
    // that cross it (the "B" input edges).
    InputEdgeId a_input_id = input_ids_[order_[i]];
    const Graph::Edge& edge0 = g_.edge(order_[i]);
    b_input_edges.clear();
    for (; next != input_crossings_.end(); ++next) {
      if (next->first != a_input_id) break;
      if (next->second.input_id() >= 0) {
        b_input_edges.push_back(next->second);
      } else if (next->second.input_id() == kSetInside) {
        inside = next->second.left_to_right();
      } else if (next->second.input_id() == kSetInvertB) {
        invert_b = next->second.left_to_right();
      } else {
        S2_DCHECK_EQ(next->second.input_id(), kSetReverseA);
        reverse_a = next->second.left_to_right();
      }
    }
    // Optimization for degenerate edges.
    // TODO(ericv): If the output layer for this edge dimension specifies
    // DegenerateEdges::DISCARD, then remove the edge here.
    if (edge0.first == edge0.second) {
      inside ^= (b_input_edges.size() & 1);
      AddEdge(edge0, a_input_id);
      continue;
    }
    // Optimization for the case where there are no crossings.
    if (b_input_edges.empty()) {
      // In general the caller only passes edges that are part of the output
      // (i.e., we could S2_DCHECK(inside) here).  The one exception is for
      // polyline/polygon operations, where the polygon edges are needed to
      // compute the polyline output but are not emitted themselves.
      if (inside) {
        AddEdge(reverse_a ? Graph::reverse(edge0) : edge0, a_input_id);
      }
      continue;
    }
    // Walk along the chain of snapped edges for input edge A, and at each
    // vertex collect all the incident edges that belong to one of the
    // crossing edge chains (the "B" input edges).
    a_vertices.clear();
    a_vertices.push_back(edge0.first);
    b_edges.clear();
    b_edges.resize(b_input_edges.size());
    GatherIncidentEdges(a_vertices, 0, b_input_edges, &b_edges);
    for (; i < order_.size() && input_ids_[order_[i]] == a_input_id; ++i) {
      a_vertices.push_back(g_.edge(order_[i]).second);
      GatherIncidentEdges(a_vertices, a_vertices.size() - 1, b_input_edges,
                          &b_edges);
    }
    --i;
    if (s2builder_verbose) {
      std::cout << "input edge " << a_input_id << " (inside=" << inside << "):";
      for (VertexId id : a_vertices) std::cout << " " << id;
    }
    // Now for each B edge chain, decide which vertex of the A chain it
    // crosses, and keep track of the number of signed crossings at each A
    // vertex.  The sign of a crossing depends on whether the other edge
    // crosses from left to right or right to left.
    //
    // This would not be necessary if all calculations were done in exact
    // arithmetic, because crossings would have strictly alternating signs.
    // But because we have already snapped the result, some crossing locations
    // are ambiguous, and GetCrossedVertexIndex() handles this by choosing a
    // candidate vertex arbitrarily.  The end result is that rarely, we may
    // see two crossings in a row with the same sign.  We correct for this by
    // adding extra output edges that essentially link up the crossings in the
    // correct (alternating sign) order.  Compared to the "correct" behavior,
    // the only difference is that we have added some extra sibling pairs
    // (consisting of an edge and its corresponding reverse edge) which do not
    // affect the result.
    a_num_crossings.clear();
    a_num_crossings.resize(a_vertices.size());
    a_isolated.clear();
    a_isolated.resize(a_vertices.size());
    for (int bi = 0; bi < b_input_edges.size(); ++bi) {
      bool left_to_right = b_input_edges[bi].left_to_right();
      int a_index = GetCrossedVertexIndex(a_vertices, b_edges[bi],
                                          left_to_right);
      if (s2builder_verbose) {
        std::cout << std::endl << "  " << "b input edge "
                  << b_input_edges[bi].input_id() << " (l2r=" << left_to_right
                  << ", crossing=" << a_vertices[a_index] << ")";
        for (const auto& x : b_edges[bi]) {
          const Graph::Edge& e = g_.edge(x.id);
          std::cout << " (" << e.first << ", " << e.second << ")";
        }
      }
      // Keep track of the number of signed crossings (see above).
      bool is_line = input_dimensions_[b_input_edges[bi].input_id()] == 1;
      int sign = is_line ? 0 : (left_to_right == invert_b) ? -1 : 1;
      a_num_crossings[a_index] += sign;

      // Any polyline or polygon vertex that has at least one crossing but no
      // adjacent emitted edge may be emitted as an isolated vertex.
      a_isolated[a_index] = true;
    }
    if (s2builder_verbose) std::cout << std::endl;

    // Finally, we iterate through the A edge chain, keeping track of the
    // number of signed crossings as we go along.  The "multiplicity" is
    // defined as the cumulative number of signed crossings, and indicates how
    // many edges should be output (and in which direction) in order to link
    // up the edge crossings in the correct order.  (The multiplicity is
    // almost always either 0 or 1 except in very rare cases.)
    int multiplicity = inside + a_num_crossings[0];
    for (int ai = 1; ai < a_vertices.size(); ++ai) {
      if (multiplicity != 0) {
        a_isolated[ai - 1] = a_isolated[ai] = false;
      }
      int edge_count = reverse_a ? -multiplicity : multiplicity;
      // Output any forward edges required.
      for (int i = 0; i < edge_count; ++i) {
        AddEdge(Graph::Edge(a_vertices[ai - 1], a_vertices[ai]), a_input_id);
      }
      // Output any reverse edges required.
      for (int i = edge_count; i < 0; ++i) {
        AddEdge(Graph::Edge(a_vertices[ai], a_vertices[ai - 1]), a_input_id);
      }
      multiplicity += a_num_crossings[ai];
    }
    // Multiplicities other than 0 or 1 can only occur in the edge interior.
    S2_DCHECK(multiplicity == 0 || multiplicity == 1);
    inside = (multiplicity != 0);

    // Output any isolated polyline vertices.
    // TODO(ericv): Only do this if an output layer wants degenerate edges.
    if (input_dimensions_[a_input_id] != 0) {
      for (int ai = 0; ai < a_vertices.size(); ++ai) {
        if (a_isolated[ai]) {
          AddEdge(Graph::Edge(a_vertices[ai], a_vertices[ai]), a_input_id);
        }
      }
    }
  }
}

// Given the vertices of the snapped edge chain for an input edge A and the
// set of input edges B that cross input edge A, this method gathers all of
// the snapped edges of B that are incident to a given snapped vertex of A.
// The incident edges for each input edge of B are appended to a separate
// output vector.  (A and B can refer to either the input edge or the
// corresponding snapped edge chain.)
void GraphEdgeClipper::GatherIncidentEdges(
    const vector<VertexId>& a, int ai,
    const vector<CrossingInputEdge>& b_input_edges,
    vector<CrossingGraphEdgeVector>* b_edges) const {
  // Examine all of the edges incident to the given vertex of A.  If any edge
  // comes from a B input edge, append it to the appropriate vector.
  S2_DCHECK_EQ(b_input_edges.size(), b_edges->size());
  for (EdgeId e : in_.edge_ids(a[ai])) {
    InputEdgeId id = input_ids_[e];
    auto it = lower_bound(b_input_edges.begin(), b_input_edges.end(), id);
    if (it != b_input_edges.end() && it->input_id() == id) {
      auto& edges = (*b_edges)[it - b_input_edges.begin()];
      edges.push_back(CrossingGraphEdge(e, ai, false, g_.edge(e).first));
    }
  }
  for (EdgeId e : out_.edge_ids(a[ai])) {
    InputEdgeId id = input_ids_[e];
    auto it = lower_bound(b_input_edges.begin(), b_input_edges.end(), id);
    if (it != b_input_edges.end() && it->input_id() == id) {
      auto& edges = (*b_edges)[it - b_input_edges.begin()];
      edges.push_back(CrossingGraphEdge(e, ai, true, g_.edge(e).second));
    }
  }
}

// Returns the "vertex rank" of the shared vertex associated with the given
// CrossingGraphEdge.  Recall that graph edges are sorted in input edge order,
// and that the rank of an edge is its position in this order (rank_[e]).
// VertexRank(e) is defined such that VertexRank(e.src) == rank_[e] and
// VertexRank(e.dst) == rank_[e] + 1.  Note that the concept of "vertex rank"
// is only defined within a single edge chain (since different edge chains can
// have overlapping vertex ranks).
int GraphEdgeClipper::GetVertexRank(const CrossingGraphEdge& e) const {
  return rank_[e.id] + !e.outgoing;
}

// Given an edge chain A that is crossed by another edge chain B (where
// "left_to_right" indicates whether B crosses A from left to right), this
// method decides which vertex of A the crossing takes place at.  The
// parameters are the vertices of the A chain ("a") and the set of edges in
// the B chain ("b") that are incident to vertices of A.  The B chain edges
// are sorted in increasing order of (a_index, outgoing) tuple.
int GraphEdgeClipper::GetCrossedVertexIndex(
    const vector<VertexId>& a, const CrossingGraphEdgeVector& b,
    bool left_to_right) const {
  S2_DCHECK(!a.empty());
  S2_DCHECK(!b.empty());

  // The reason this calculation is tricky is that after snapping, the A and B
  // chains may meet and separate several times.  For example, if B crosses A
  // from left to right, then B may touch A, make an excursion to the left of
  // A, come back to A, then make an excursion to the right of A and come back
  // to A again, like this:
  //
  //  *--B--*-\             /-*-\
  //           B-\       /-B     B-\      6     7     8     9
  //  *--A--*--A--*-A,B-*--A--*--A--*-A,B-*--A--*--A--*-A,B-*
  //  0     1     2     3     4     5      \-B     B-/
  //                                          \-*-/
  //
  // (where "*" is a vertex, and "A" and "B" are edge labels).  Note that B
  // may also follow A for one or more edges whenever they touch (e.g. between
  // vertices 2 and 3 ).  In this case the only vertices of A where the
  // crossing could take place are 5 and 6, i.e. after all excursions of B to
  // the left of A, and before all excursions of B to the right of A.
  //
  // Other factors to consider are that the portion of B before and/or after
  // the crossing may be degenerate, and some or all of the B edges may be
  // reversed relative to the A edges.

  // First, check whether edge A is degenerate.
  int n = a.size();
  if (n == 1) return 0;

  // If edge chain B is incident to only one vertex of A, we're done.
  if (b[0].a_index == b.back().a_index) return b[0].a_index;

  // Determine whether the B chain visits the first and last vertices that it
  // shares with the A chain in the same order or the reverse order.  This is
  // only needed to implement one special case (see below).
  bool b_reversed = GetVertexRank(b[0]) > GetVertexRank(b.back());

  // Examine each incident B edge and use it to narrow the range of positions
  // where the crossing could occur in the B chain.  Vertex positions are
  // represented as a range [lo, hi] of vertex ranks in the B chain (see
  // GetVertexRank).
  //
  // Note that if an edge of B is incident to the first or last vertex of A,
  // we can't test which side of the A chain it is on.  There can be up to 4
  // such edges (one incoming and one outgoing edge at each vertex).  Two of
  // these edges logically extend past the end of the A chain and place no
  // restrictions on the crossing vertex.  The other two edges define the ends
  // of the subchain where B shares vertices with A.  We save these edges in
  // order to handle a special case (see below).
  int lo = -1, hi = order_.size();   // Vertex ranks of acceptable crossings
  EdgeId b_first = -1, b_last = -1;  // "b" subchain connecting "a" endpoints
  for (const auto& e : b) {
    int ai = e.a_index;
    if (ai == 0) {
      if (e.outgoing != b_reversed && e.dst != a[1]) b_first = e.id;
    } else if (ai == n - 1) {
      if (e.outgoing == b_reversed && e.dst != a[n - 2]) b_last = e.id;
    } else {
      // This B edge is incident to an interior vertex of the A chain.  First
      // check whether this edge is identical (or reversed) to an edge in the
      // A chain, in which case it does not create any restrictions.
      if (e.dst == a[ai - 1] || e.dst == a[ai + 1]) continue;

      // Otherwise we can test which side of the A chain the edge lies on.
      bool on_left = s2pred::OrderedCCW(g_.vertex(a[ai + 1]), g_.vertex(e.dst),
                                        g_.vertex(a[ai - 1]), g_.vertex(a[ai]));

      // Every B edge that is incident to an interior vertex of the A chain
      // places some restriction on where the crossing vertex could be.
      if (left_to_right == on_left) {
        // This is a pre-crossing edge, so the crossing cannot be before the
        // destination vertex of this edge.  (For example, the input B edge
        // crosses the input A edge from left to right and this edge of the B
        // chain is to the left of the A chain.)
        lo = max(lo, rank_[e.id] + 1);
      } else {
        // This is a post-crossing edge, so the crossing cannot be after the
        // source vertex of this edge.
        hi = min(hi, rank_[e.id]);
      }
    }
  }
  // There is one special case.  If there were no B edges incident to interior
  // vertices of A, then we can't reliably test which side of A the B edges
  // are on.  (An s2pred::Sign test doesn't work, since an edge of B can snap to
  // the "wrong" side of A while maintaining topological guarantees.)  So
  // instead we construct a loop consisting of the A edge chain plus the
  // portion of the B chain that connects the endpoints of A.  We can then
  // test the orientation of this loop.
  //
  // Note that it would be possible to avoid this in some situations by
  // testing whether either endpoint of the A chain has two incident B edges,
  // in which case we could check which side of the B chain the A edge is on
  // and use this to limit the possible crossing locations.
  if (lo < 0 && hi >= order_.size() && b_first >= 0 && b_last >= 0) {
    // Swap the edges if necessary so that they are in B chain order.
    if (b_reversed) swap(b_first, b_last);
    bool on_left = EdgeChainOnLeft(a, b_first, b_last);
    if (left_to_right == on_left) {
      lo = max(lo, rank_[b_last] + 1);
    } else {
      hi = min(hi, rank_[b_first]);
    }
  }

  // Otherwise we choose the smallest shared VertexId in the acceptable range,
  // in order to ensure that both chains choose the same crossing vertex.
  int best = -1;
  S2_DCHECK_LE(lo, hi);
  for (const auto& e : b) {
    int ai = e.a_index;
    int vrank = GetVertexRank(e);
    if (vrank >= lo && vrank <= hi && (best < 0 || a[ai] < a[best])) {
      best = ai;
    }
  }
  return best;
}

// Given edge chains A and B that form a loop (after possibly reversing the
// direction of chain B), returns true if chain B is to the left of chain A.
// Chain A is given as a sequence of vertices, while chain B is specified as
// the first and last edges of the chain.
bool GraphEdgeClipper::EdgeChainOnLeft(
    const vector<VertexId>& a, EdgeId b_first, EdgeId b_last) const {
  // Gather all the interior vertices of the B subchain.
  vector<VertexId> loop;
  for (int i = rank_[b_first]; i < rank_[b_last]; ++i) {
    loop.push_back(g_.edge(order_[i]).second);
  }
  // Possibly reverse the chain so that it forms a loop when "a" is appended.
  if (g_.edge(b_last).second != a[0]) std::reverse(loop.begin(), loop.end());
  loop.insert(loop.end(), a.begin(), a.end());
  // Duplicate the first two vertices to simplify vertex indexing.
  loop.insert(loop.end(), loop.begin(), loop.begin() + 2);
  // Now B is to the left of A if and only if the loop is counterclockwise.
  double sum = 0;
  for (int i = 2; i < loop.size(); ++i) {
    sum += S2::TurnAngle(g_.vertex(loop[i - 2]), g_.vertex(loop[i - 1]),
                         g_.vertex(loop[i]));
  }
  return sum > 0;
}

// Given a set of clipping instructions encoded as a set of intersections
// between input edges, EdgeClippingLayer determines which graph edges
// correspond to clipped portions of input edges and removes them.  It
// assembles the remaining edges into a new S2Builder::Graph and passes the
// result to the given output layer for assembly.
class EdgeClippingLayer : public S2Builder::Layer {
 public:
  EdgeClippingLayer(const vector<unique_ptr<S2Builder::Layer>>* layers,
                    const vector<int8>* input_dimensions,
                    const InputEdgeCrossings* input_crossings)
      : layers_(*layers),
        input_dimensions_(*input_dimensions),
        input_crossings_(*input_crossings) {
  }

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  const vector<unique_ptr<S2Builder::Layer>>& layers_;
  const vector<int8>& input_dimensions_;
  const InputEdgeCrossings& input_crossings_;
};

GraphOptions EdgeClippingLayer::graph_options() const {
  // We keep all edges, including degenerate ones, so that we can figure out
  // the correspondence between input edge crossings and output edge
  // crossings.
  return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                      DuplicateEdges::KEEP, SiblingPairs::KEEP);
}

// Helper function (in anonymous namespace) to create an S2Builder::Graph from
// a vector of edges.
Graph MakeGraph(
    const Graph& g, GraphOptions* options, vector<Graph::Edge>* new_edges,
    vector<InputEdgeIdSetId>* new_input_edge_ids,
    IdSetLexicon* new_input_edge_id_set_lexicon, S2Error* error) {
  if (options->edge_type() == EdgeType::UNDIRECTED) {
    // Create a reversed edge for every edge.
    int n = new_edges->size();
    new_edges->reserve(2 * n);
    new_input_edge_ids->reserve(2 * n);
    for (int i = 0; i < n; ++i) {
      new_edges->push_back(Graph::reverse((*new_edges)[i]));
      new_input_edge_ids->push_back(IdSetLexicon::EmptySetId());
    }
  }
  Graph::ProcessEdges(options, new_edges, new_input_edge_ids,
                      new_input_edge_id_set_lexicon, error);
  return Graph(*options, &g.vertices(), new_edges, new_input_edge_ids,
               new_input_edge_id_set_lexicon, &g.label_set_ids(),
               &g.label_set_lexicon(), g.is_full_polygon_predicate());
}

void EdgeClippingLayer::Build(const Graph& g, S2Error* error) {
  // The bulk of the work is handled by GraphEdgeClipper.
  vector<Graph::Edge> new_edges;
  vector<InputEdgeIdSetId> new_input_edge_ids;
  // Destroy the GraphEdgeClipper immediately to save memory.
  GraphEdgeClipper(g, input_dimensions_, input_crossings_,
                   &new_edges, &new_input_edge_ids).Run();
  if (s2builder_verbose) {
    std::cout << "Edges after clipping: " << std::endl;
    for (int i = 0; i < new_edges.size(); ++i) {
      std::cout << "  " << new_input_edge_ids[i] << " (" << new_edges[i].first
                << ", " << new_edges[i].second << ")" << std::endl;
    }
  }
  // Construct one or more graphs from the clipped edges and pass them to the
  // given output layer(s).
  IdSetLexicon new_input_edge_id_set_lexicon;
  if (layers_.size() == 1) {
    GraphOptions options = layers_[0]->graph_options();
    Graph new_graph = MakeGraph(g, &options, &new_edges, &new_input_edge_ids,
                                &new_input_edge_id_set_lexicon, error);
    layers_[0]->Build(new_graph, error);
  } else {
    // The Graph objects must be valid until the last Build() call completes,
    // so we store all of the graph data in arrays with 3 elements.
    S2_DCHECK_EQ(3, layers_.size());
    vector<Graph::Edge> layer_edges[3];
    vector<InputEdgeIdSetId> layer_input_edge_ids[3];
    S2Builder::GraphOptions layer_options[3];
    vector<S2Builder::Graph> layer_graphs;  // No default constructor.
    layer_graphs.reserve(3);
    // Separate the edges according to their dimension.
    for (int i = 0; i < new_edges.size(); ++i) {
      int d = input_dimensions_[new_input_edge_ids[i]];
      layer_edges[d].push_back(new_edges[i]);
      layer_input_edge_ids[d].push_back(new_input_edge_ids[i]);
    }
    // Clear variables to save space.
    vector<Graph::Edge>().swap(new_edges);
    vector<InputEdgeIdSetId>().swap(new_input_edge_ids);
    for (int d = 0; d < 3; ++d) {
      layer_options[d] = layers_[d]->graph_options();
      layer_graphs.push_back(MakeGraph(
          g, &layer_options[d], &layer_edges[d], &layer_input_edge_ids[d],
          &new_input_edge_id_set_lexicon, error));
      layers_[d]->Build(layer_graphs[d], error);
    }
  }
}

}  // namespace

class S2BooleanOperation::Impl {
 public:
  explicit Impl(S2BooleanOperation* op)
      : op_(op), index_crossings_first_region_id_(-1) {
  }
  bool Build(S2Error* error);

 private:
  class CrossingIterator;
  class CrossingProcessor;
  using ShapeEdge = s2shapeutil::ShapeEdge;
  using ShapeEdgeId = s2shapeutil::ShapeEdgeId;

  // An IndexCrossing represents a pair of intersecting S2ShapeIndex edges
  // ("a_edge" and "b_edge").  We store all such intersections because the
  // algorithm needs them twice, once when processing the boundary of region A
  // and once when processing the boundary of region B.
  struct IndexCrossing {
    ShapeEdgeId a, b;

    // True if S2::CrossingSign(a_edge, b_edge) > 0.
    uint32 is_interior_crossing : 1;

    // True if "a_edge" crosses "b_edge" from left to right.  Undefined if
    // is_interior_crossing is false.
    uint32 left_to_right: 1;

    // Equal to S2::VertexCrossing(a_edge, b_edge).  Undefined if "a_edge" and
    // "b_edge" do not share exactly one vertex or either edge is degenerate.
    uint32 is_vertex_crossing : 1;

    // All flags are "false" by default.
    IndexCrossing(ShapeEdgeId _a, ShapeEdgeId _b)
        : a(_a), b(_b), is_interior_crossing(false), left_to_right(false),
          is_vertex_crossing(false) {
    }

    friend bool operator==(const IndexCrossing& x, const IndexCrossing& y) {
      return x.a == y.a && x.b == y.b;
    }
    friend bool operator<(const IndexCrossing& x, const IndexCrossing& y) {
      // The compiler (2017) doesn't optimize the following as well:
      // return x.a < y.a || (x.a == y.a && x.b < y.b);
      if (x.a.shape_id < y.a.shape_id) return true;
      if (y.a.shape_id < x.a.shape_id) return false;
      if (x.a.edge_id < y.a.edge_id) return true;
      if (y.a.edge_id < x.a.edge_id) return false;
      if (x.b.shape_id < y.b.shape_id) return true;
      if (y.b.shape_id < x.b.shape_id) return false;
      return x.b.edge_id < y.b.edge_id;
    }
  };
  using IndexCrossings = vector<IndexCrossing>;

  bool is_boolean_output() const { return op_->result_empty_ != nullptr; }

  // All of the methods below support "early exit" in the case of boolean
  // results by returning "false" as soon as the result is known to be
  // non-empty.
  bool AddBoundary(int a_region_id, bool invert_a, bool invert_b,
                   bool invert_result,
                   const vector<ShapeEdgeId>& a_chain_starts,
                   CrossingProcessor* cp);
  bool GetChainStarts(int a_region_id, bool invert_a, bool invert_b,
                      bool invert_result, CrossingProcessor* cp,
                      vector<ShapeEdgeId>* chain_starts);
  bool ProcessIncidentEdges(const ShapeEdge& a,
                            S2ContainsPointQuery<S2ShapeIndex>* query,
                            CrossingProcessor* cp);
  static bool HasInterior(const S2ShapeIndex& index);
  static bool AddIndexCrossing(const ShapeEdge& a, const ShapeEdge& b,
                               bool is_interior, IndexCrossings* crossings);
  bool GetIndexCrossings(int region_id);
  bool AddBoundaryPair(bool invert_a, bool invert_b, bool invert_result,
                       CrossingProcessor* cp);
  bool AreRegionsIdentical() const;
  bool BuildOpType(OpType op_type);

  S2BooleanOperation* op_;

  // The S2Builder used to construct the output.
  unique_ptr<S2Builder> builder_;

  // A vector specifying the dimension of each edge added to S2Builder.
  vector<int8> input_dimensions_;

  // The set of all input edge crossings, which is used by EdgeClippingLayer
  // to construct the clipped output polygon.
  InputEdgeCrossings input_crossings_;

  // kSentinel is a sentinel value used to mark the end of vectors.
  static const ShapeEdgeId kSentinel;

  // A vector containing all pairs of crossing edges from the two input
  // regions (including edge pairs that share a common vertex).  The first
  // element of each pair is an edge from "index_crossings_first_region_id_",
  // while the second element of each pair is an edge from the other region.
  IndexCrossings index_crossings_;

  // Indicates that the first element of each crossing edge pair in
  // "index_crossings_" corresponds to an edge from the given region.
  // This field is negative if index_crossings_ has not been computed yet.
  int index_crossings_first_region_id_;

  // Temporary storage used in GetChainStarts(), declared here to avoid
  // repeatedly allocating memory.
  IndexCrossings tmp_crossings_;
};

const s2shapeutil::ShapeEdgeId S2BooleanOperation::Impl::kSentinel(
    std::numeric_limits<int32>::max(), 0);

// A helper class for iterating through the edges from region B that cross a
// particular edge from region A.  It caches information from the current
// shape, chain, and edge so that it doesn't need to be looked up repeatedly.
// Typical usage:
//
//  void SomeFunction(ShapeEdgeId a_id, CrossingIterator *it) {
//    // Iterate through the edges that cross edge "a_id".
//    for (; !it->Done(a_id); it->Next()) {
//      ... use it->b_shape(), it->b_edge(), etc ...
//    }
class S2BooleanOperation::Impl::CrossingIterator {
 public:
  // Creates an iterator over crossing edge pairs (a, b) where "b" is an edge
  // from "b_index".  "crossings_complete" indicates that "crossings" contains
  // all edge crossings between the two regions (rather than a subset).
  CrossingIterator(const S2ShapeIndex* b_index,
                   const IndexCrossings* crossings, bool crossings_complete)
      : b_index_(*b_index), it_(crossings->begin()), b_shape_id_(-1),
        crossings_complete_(crossings_complete) {
    Update();
  }
  void Next() {
    ++it_;
    Update();
  }
  bool Done(ShapeEdgeId id) const { return a_id() != id; }

  // True if all edge crossings are available (see above).
  bool crossings_complete() const { return crossings_complete_; }

  // True if this crossing occurs at a point interior to both edges.
  bool is_interior_crossing() const { return it_->is_interior_crossing; }

  // Equal to S2::VertexCrossing(a_edge, b_edge), provided that a_edge and
  // b_edge have exactly one vertex in common and neither edge is degenerate.
  bool is_vertex_crossing() const { return it_->is_vertex_crossing; }

  // True if a_edge crosses b_edge from left to right (for interior crossings).
  bool left_to_right() const { return it_->left_to_right; }

  ShapeEdgeId a_id() const { return it_->a; }
  ShapeEdgeId b_id() const { return it_->b; }
  const S2ShapeIndex& b_index() const { return b_index_; }
  const S2Shape& b_shape() const { return *b_shape_; }
  int b_dimension() const { return b_dimension_; }
  int b_shape_id() const { return b_shape_id_; }
  int b_edge_id() const { return b_id().edge_id; }

  S2Shape::Edge b_edge() const {
    return b_shape_->edge(b_edge_id());  // Opportunity to cache this.
  }

  // Information about the chain to which an edge belongs.
  struct ChainInfo {
    int chain_id;  // chain id
    int start;     // starting edge id
    int limit;     // limit edge id
  };
  // Returns a description of the chain to which the current B edge belongs.
  const ChainInfo& b_chain_info() const {
    if (b_info_.chain_id < 0) {
      b_info_.chain_id = b_shape().chain_position(b_edge_id()).chain_id;
      auto chain = b_shape().chain(b_info_.chain_id);
      b_info_.start = chain.start;
      b_info_.limit = chain.start + chain.length;
    }
    return b_info_;
  }

 private:
  // Updates information about the B shape whenever it changes.
  void Update() {
    if (a_id() != kSentinel && b_id().shape_id != b_shape_id_) {
      b_shape_id_ = b_id().shape_id;
      b_shape_ = b_index_.shape(b_shape_id_);
      b_dimension_ = b_shape_->dimension();
      b_info_.chain_id = -1;  // Computed on demand.
    }
  }

  const S2ShapeIndex& b_index_;
  IndexCrossings::const_iterator it_;
  const S2Shape* b_shape_;
  int b_shape_id_;
  int b_dimension_;
  mutable ChainInfo b_info_;  // Computed on demand.
  bool crossings_complete_;
};

// CrossingProcessor is a helper class that processes all the edges from one
// region that cross a specific edge of the other region.  It outputs the
// appropriate edges to an S2Builder, and outputs other information required
// by GraphEdgeClipper to the given vectors.
class S2BooleanOperation::Impl::CrossingProcessor {
 public:
  // Prepares to build output for the given polygon and polyline boundary
  // models.  Edges are emitted to "builder", while other auxiliary data is
  // appended to the given vectors.
  //
  // If a predicate is being evaluated (i.e., we do not need to construct the
  // actual result), then "builder" and the various output vectors should all
  // be nullptr.
  CrossingProcessor(const PolygonModel& polygon_model,
                    const PolylineModel& polyline_model,
                    bool polyline_loops_have_boundaries,
                    S2Builder* builder,
                    vector<int8>* input_dimensions,
                    InputEdgeCrossings *input_crossings)
      : polygon_model_(polygon_model), polyline_model_(polyline_model),
        polyline_loops_have_boundaries_(polyline_loops_have_boundaries),
        builder_(builder), input_dimensions_(input_dimensions),
        input_crossings_(input_crossings), prev_inside_(false) {
  }

  // Starts processing edges from the given region.  "invert_a", "invert_b",
  // and "invert_result" indicate whether region A, region B, and/or the
  // result should be inverted, which allows operations such as union and
  // difference to be implemented.  For example, union is ~(~A & ~B).
  //
  // This method should be called in pairs, once to process the edges from
  // region A and once to process the edges from region B.
  void StartBoundary(int a_region_id, bool invert_a, bool invert_b,
                     bool invert_result);

  // Starts processing edges from the given shape.
  void StartShape(const S2Shape* a_shape);

  // Starts processing edges from the given chain.
  void StartChain(int chain_id, S2Shape::Chain chain, bool inside);

  // Processes the given edge "a_id".  "it" should be positioned to the set of
  // edges from the other region that cross "a_id" (if any).
  //
  // Supports "early exit" in the case of boolean results by returning false
  // as soon as the result is known to be non-empty.
  bool ProcessEdge(ShapeEdgeId a_id, CrossingIterator* it);

  // This method should be called after each pair of calls to StartBoundary.
  // (The only operation that processes more than one pair of boundaries is
  // SYMMETRIC_DIFFERENCE, which computes the union of A-B and B-A.)
  //
  // Resets the state of the CrossingProcessor.
  void DoneBoundaryPair();

  // Indicates whether the point being processed along the current edge chain
  // is in the polygonal interior of the opposite region, using semi-open
  // boundaries.  If "invert_b_" is true then this field is inverted.
  //
  // This value along with the set of incident edges can be used to compute
  // whether the opposite region contains this point under any of the
  // supported boundary models (PolylineModel::CLOSED, etc).
  bool inside() const { return inside_; }

 private:
  // SourceEdgeCrossing represents an input edge that crosses some other
  // edge; it crosses the edge from left to right iff the second parameter
  // is "true".
  using SourceEdgeCrossing = pair<SourceId, bool>;
  class PointCrossingResult;
  class EdgeCrossingResult;

  InputEdgeId input_edge_id() const { return input_dimensions_->size(); }

  // Returns true if the edges on either side of the first vertex of the
  // current edge have not been emitted.
  //
  // REQUIRES: This method is called just after updating "inside_" for "v0".
  bool is_v0_isolated(ShapeEdgeId a_id) const {
    return !inside_ && v0_emitted_max_edge_id_ < a_id.edge_id;
  }

  // Returns true if "a_id" is the last edge of the current chain, and the
  // edges on either side of the last vertex have not been emitted (including
  // the possibility that the chain forms a loop).
  bool is_chain_last_vertex_isolated(ShapeEdgeId a_id) const {
    return (a_id.edge_id == chain_limit_ - 1 && !chain_v0_emitted_ &&
            v0_emitted_max_edge_id_ <= a_id.edge_id);
  }

  // Returns true if the given polyline edge contains "v0", taking into
  // account the specified PolylineModel.
  bool polyline_contains_v0(int edge_id, int chain_start) const {
    return (polyline_model_ != PolylineModel::OPEN || edge_id > chain_start);
  }

  void AddCrossing(const SourceEdgeCrossing& crossing) {
    source_edge_crossings_.push_back(make_pair(input_edge_id(), crossing));
  }

  void SetClippingState(InputEdgeId parameter, bool state) {
    AddCrossing(SourceEdgeCrossing(SourceId(parameter), state));
  }

  // Supports "early exit" in the case of boolean results by returning false
  // as soon as the result is known to be non-empty.
  bool AddEdge(ShapeEdgeId a_id, const S2Shape::Edge& a,
               int dimension, int interior_crossings) {
    if (builder_ == nullptr) return false;  // Boolean output.
    if (interior_crossings > 0) {
      // Build a map that translates temporary edge ids (SourceId) to
      // the representation used by EdgeClippingLayer (InputEdgeId).
      SourceId src_id(a_region_id_, a_id.shape_id, a_id.edge_id);
      source_id_map_[src_id] = input_edge_id();
    }
    // Set the GraphEdgeClipper's "inside" state to match ours.
    if (inside_ != prev_inside_) SetClippingState(kSetInside, inside_);
    input_dimensions_->push_back(dimension);
    builder_->AddEdge(a.v0, a.v1);
    inside_ ^= (interior_crossings & 1);
    prev_inside_ = inside_;
    return true;
  }

  // Supports "early exit" in the case of boolean results by returning false
  // as soon as the result is known to be non-empty.
  bool AddPointEdge(const S2Point& p, int dimension) {
    if (builder_ == nullptr) return false;  // Boolean output.
    if (!prev_inside_) SetClippingState(kSetInside, true);
    input_dimensions_->push_back(dimension);
    builder_->AddEdge(p, p);
    prev_inside_ = true;
    return true;
  }

  bool ProcessEdge0(ShapeEdgeId a_id, const S2Shape::Edge& a,
                    CrossingIterator* it);
  bool ProcessEdge1(ShapeEdgeId a_id, const S2Shape::Edge& a,
                    CrossingIterator* it);
  bool ProcessEdge2(ShapeEdgeId a_id, const S2Shape::Edge& a,
                    CrossingIterator* it);

  void SkipCrossings(ShapeEdgeId a_id, CrossingIterator* it);
  PointCrossingResult ProcessPointCrossings(
      ShapeEdgeId a_id, const S2Point& a0, CrossingIterator* it) const;
  EdgeCrossingResult ProcessEdgeCrossings(
      ShapeEdgeId a_id, const S2Shape::Edge& a, CrossingIterator* it);

  bool IsPolylineVertexInside(bool matches_polyline,
                              bool matches_polygon) const;
  bool IsPolylineEdgeInside(const EdgeCrossingResult& r) const;
  bool PolylineEdgeContainsVertex(const S2Point& v,
                                  const CrossingIterator& it) const;

  // Constructor parameters:

  PolygonModel polygon_model_;
  PolylineModel polyline_model_;
  bool polyline_loops_have_boundaries_;

  // The output of the CrossingProcessor consists of a subset of the input
  // edges that are emitted to "builder_", and some auxiliary information
  // that allows GraphEdgeClipper to determine which segments of those input
  // edges belong to the output.  The auxiliary information consists of the
  // dimension of each input edge, and set of input edges from the other
  // region that cross each input input edge.
  S2Builder* builder_;
  vector<int8>* input_dimensions_;
  InputEdgeCrossings* input_crossings_;

  // Fields set by StartBoundary:

  int a_region_id_, b_region_id_;
  bool invert_a_, invert_b_, invert_result_;
  bool is_union_;  // True if this is a UNION operation.

  // Fields set by StartShape:

  const S2Shape* a_shape_;
  int a_dimension_;

  // Fields set by StartChain:

  int chain_id_;
  int chain_start_;
  int chain_limit_;

  // Fields updated by ProcessEdge:

  // A temporary representation of input_crossings_ that is used internally
  // until all necessary edges from *both* polygons have been emitted to the
  // S2Builder.  This field is then converted by DoneBoundaryPair() into
  // the InputEdgeCrossings format expected by GraphEdgeClipper.
  //
  // The reason that we can't construct input_crossings_ directly is that it
  // uses InputEdgeIds to identify the edges from both polygons, and when we
  // are processing edges from the first polygon, InputEdgeIds have not yet
  // been assigned to the second polygon.  So instead this field identifies
  // edges from the first polygon using an InputEdgeId, and edges from the
  // second polygon using a (region_id, shape_id, edge_id) tuple (i.e., a
  // SourceId).
  //
  // All crossings are represented twice, once to indicate that an edge from
  // polygon 0 is crossed by an edge from polygon 1, and once to indicate that
  // an edge from polygon 1 is crossed by an edge from polygon 0.
  using SourceEdgeCrossings = vector<pair<InputEdgeId, SourceEdgeCrossing>>;
  SourceEdgeCrossings source_edge_crossings_;

  // A map that translates from SourceId (the (region_id, shape_id,
  // edge_id) triple that identifies an S2ShapeIndex edge) to InputEdgeId (the
  // sequentially increasing numbers assigned to input edges by S2Builder).
  using SourceIdMap = gtl::btree_map<SourceId, InputEdgeId>;
  SourceIdMap source_id_map_;

  // Indicates whether the point being processed along the current edge chain
  // is in the polygonal interior of the opposite region, using semi-open
  // boundaries.  If "invert_b_" is true then this field is inverted.
  //
  // Equal to: b_index_.Contains(current point) ^ invert_b_
  bool inside_;

  // The value of that "inside_" would have just before the end of the
  // previous edge added to S2Builder.  This value is used to determine
  // whether the GraphEdgeClipper state needs to be updated when jumping from
  // one edge chain to another.
  bool prev_inside_;

  // The maximum edge id of any edge in the current chain whose v0 vertex has
  // already been emitted.  This is used to determine when an isolated vertex
  // needs to be emitted, e.g. when two closed polygons share only a vertex.
  int v0_emitted_max_edge_id_;

  // True if the first vertex of the current chain has been emitted.  This is
  // used when processing loops in order to determine whether the first/last
  // vertex of the loop should be emitted as an isolated vertex.
  bool chain_v0_emitted_;
};

// See documentation above.
void S2BooleanOperation::Impl::CrossingProcessor::StartBoundary(
    int a_region_id, bool invert_a, bool invert_b, bool invert_result) {
  a_region_id_ = a_region_id;
  b_region_id_ = 1 - a_region_id;
  invert_a_ = invert_a;
  invert_b_ = invert_b;
  invert_result_ = invert_result;
  is_union_ = invert_b && invert_result;

  // Specify to GraphEdgeClipper how these edges should be clipped.
  SetClippingState(kSetReverseA, invert_a != invert_result);
  SetClippingState(kSetInvertB, invert_b);
}

// See documentation above.
inline void S2BooleanOperation::Impl::CrossingProcessor::StartShape(
    const S2Shape* a_shape) {
  a_shape_ = a_shape;
  a_dimension_ = a_shape->dimension();
}

// See documentation above.
inline void S2BooleanOperation::Impl::CrossingProcessor::StartChain(
    int chain_id, S2Shape::Chain chain, bool inside) {
  chain_id_ = chain_id;
  chain_start_ = chain.start;
  chain_limit_ = chain.start + chain.length;
  inside_ = inside;
  v0_emitted_max_edge_id_ = chain.start - 1;  // No edges emitted yet.
  chain_v0_emitted_ = false;
}

// See documentation above.
bool S2BooleanOperation::Impl::CrossingProcessor::ProcessEdge(
    ShapeEdgeId a_id, CrossingIterator* it) {
  // chain_edge() is faster than edge() when there are multiple chains.
  auto a = a_shape_->chain_edge(chain_id_, a_id.edge_id - chain_start_);
  if (a_dimension_ == 0) {
    return ProcessEdge0(a_id, a, it);
  } else if (a_dimension_ == 1) {
    return ProcessEdge1(a_id, a, it);
  } else {
    S2_DCHECK_EQ(2, a_dimension_);
    return ProcessEdge2(a_id, a, it);
  }
}

// PointCrossingResult describes the relationship between a point from region A
// and a set of crossing edges from region B.  For example, "matches_polygon"
// indicates whether a polygon vertex from region B matches the given point.
struct S2BooleanOperation::Impl::CrossingProcessor::PointCrossingResult {
  PointCrossingResult()
      : matches_point(false), matches_polyline(false), matches_polygon(false) {
  }
  // Note that "matches_polyline" is true only if the point matches a polyline
  // vertex of B *and* the polyline contains that vertex, whereas
  // "matches_polygon" is true if the point matches any polygon vertex.
  bool matches_point;     // Matches point.
  bool matches_polyline;  // Matches contained polyline vertex.
  bool matches_polygon;   // Matches polygon vertex.
};

// Processes an edge of dimension 0 (i.e., a point) from region A.
//
// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::CrossingProcessor::ProcessEdge0(
    ShapeEdgeId a_id, const S2Shape::Edge& a, CrossingIterator* it) {
  S2_DCHECK_EQ(a.v0, a.v1);
  // When a region is inverted, all points and polylines are discarded.
  if (invert_a_ != invert_result_) {
    SkipCrossings(a_id, it);
    return true;
  }
  PointCrossingResult r = ProcessPointCrossings(a_id, a.v0, it);

  // "contained" indicates whether the current point is inside the polygonal
  // interior of the opposite region, using semi-open boundaries.
  bool contained = inside_ ^ invert_b_;
  if (r.matches_polygon && polygon_model_ != PolygonModel::SEMI_OPEN) {
    contained = (polygon_model_ == PolygonModel::CLOSED);
  }
  if (r.matches_polyline) contained = true;

  // The output of UNION includes duplicate values, so ensure that points are
  // not suppressed by other points.
  if (r.matches_point && !is_union_) contained = true;

  // Test whether the point is contained after region B is inverted.
  if (contained == invert_b_) return true;  // Don't exit early.
  return AddPointEdge(a.v0, 0);
}

// Skip any crossings that were not needed to determine the result.
inline void S2BooleanOperation::Impl::CrossingProcessor::SkipCrossings(
    ShapeEdgeId a_id, CrossingIterator* it) {
  while (!it->Done(a_id)) it->Next();
}

// Returns a summary of the relationship between a point from region A and
// a set of crossing edges from region B (see PointCrossingResult).
S2BooleanOperation::Impl::CrossingProcessor::PointCrossingResult
S2BooleanOperation::Impl::CrossingProcessor::ProcessPointCrossings(
    ShapeEdgeId a_id, const S2Point& a0, CrossingIterator* it) const {
  PointCrossingResult r;
  for (; !it->Done(a_id); it->Next()) {
    if (it->b_dimension() == 0) {
      r.matches_point = true;
    } else if (it->b_dimension() == 1) {
      if (PolylineEdgeContainsVertex(a0, *it)) {
        r.matches_polyline = true;
      }
    } else {
      r.matches_polygon = true;
    }
  }
  return r;
}

// EdgeCrossingResult describes the relationship between an edge from region A
// ("a_edge") and a set of crossing edges from region B.  For example,
// "matches_polygon" indicates whether "a_edge" matches a polygon edge from
// region B.
struct S2BooleanOperation::Impl::CrossingProcessor::EdgeCrossingResult {
  EdgeCrossingResult()
      : matches_polyline(false), matches_polygon(false), matches_sibling(false),
        a0_matches_polyline(false), a1_matches_polyline(false),
        a0_matches_polygon(false), a1_matches_polygon(false),
        a0_crossings(0), a1_crossings(0), interior_crossings(0) {
  }
  // These fields indicate that "a_edge" exactly matches an edge of B.
  bool matches_polyline;     // Matches polyline edge (either direction).
  bool matches_polygon;      // Matches polygon edge (same direction).
  bool matches_sibling;      // Matches polygon edge (reverse direction).

  // These fields indicate that a vertex of "a_edge" matches a polyline vertex
  // of B *and* the polyline contains that vertex.
  bool a0_matches_polyline;  // Start vertex matches contained polyline vertex.
  bool a1_matches_polyline;  // End vertex matches contained polyline vertex.

  // These fields indicate that a vertex of "a_edge" matches a polygon vertex
  // of B.  (Unlike with polylines, the polygon may not contain that vertex.)
  bool a0_matches_polygon;   // Start vertex matches polygon vertex.
  bool a1_matches_polygon;   // End vertex matches polygon vertex.

  // These fields count the number of edge crossings at the start vertex, end
  // vertex, and interior of "a_edge".
  int a0_crossings;          // Count of polygon crossings at start vertex.
  int a1_crossings;          // Count of polygon crossings at end vertex.
  int interior_crossings;    // Count of polygon crossings in edge interior.
};

// Processes an edge of dimension 1 (i.e., a polyline edge) from region A.
//
// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::CrossingProcessor::ProcessEdge1(
    ShapeEdgeId a_id, const S2Shape::Edge& a, CrossingIterator* it) {
  // When a region is inverted, all points and polylines are discarded.
  if (invert_a_ != invert_result_) {
    SkipCrossings(a_id, it);
    return true;
  }
  // Evaluate whether the start vertex should belong to the output, in case it
  // needs to be emitted as an isolated vertex.
  EdgeCrossingResult r = ProcessEdgeCrossings(a_id, a, it);
  bool a0_inside = IsPolylineVertexInside(r.a0_matches_polyline,
                                          r.a0_matches_polygon);

  // Test whether the entire polyline edge should be emitted (or not emitted)
  // because it matches a polyline or polygon edge.
  inside_ ^= (r.a0_crossings & 1);
  if (inside_ != IsPolylineEdgeInside(r)) {
    inside_ ^= true;   // Invert the inside_ state.
    ++r.a1_crossings;  // Restore the correct (semi-open) state later.
  }

  // If neither edge adjacent to v0 was emitted, and this polyline contains
  // v0, and the other region contains v0, then emit an isolated vertex.
  if (!polyline_loops_have_boundaries_ && a_id.edge_id == chain_start_ &&
      a.v0 == a_shape_->chain_edge(chain_id_,
                                   chain_limit_ - chain_start_ - 1).v1) {
    // This is the first vertex of a polyline loop, so we can't decide if it
    // is isolated until we process the last polyline edge.
    chain_v0_emitted_ = inside_;
  } else if (is_v0_isolated(a_id) &&
             polyline_contains_v0(a_id.edge_id, chain_start_) && a0_inside) {
    if (!AddPointEdge(a.v0, 1)) return false;
  }

  // Test whether the entire edge or any part of it belongs to the output.
  if (inside_ || r.interior_crossings > 0) {
    // Note: updates "inside_" to correspond to the state just before a1.
    if (!AddEdge(a_id, a, 1 /*dimension*/, r.interior_crossings)) {
      return false;
    }
  }
  // Remember whether the edge portion just before "a1" was emitted, so that
  // we can decide whether "a1" need to be emitted as an isolated vertex.
  if (inside_) v0_emitted_max_edge_id_ = a_id.edge_id + 1;

  // Verify that edge crossings are being counted correctly.
  inside_ ^= (r.a1_crossings & 1);
  if (it->crossings_complete()) {
    S2_DCHECK_EQ(MakeS2ContainsPointQuery(&it->b_index()).Contains(a.v1),
              inside_ ^ invert_b_);
  }

  // Special case to test whether the last vertex of a polyline should be
  // emitted as an isolated vertex.
  if (it->crossings_complete() && is_chain_last_vertex_isolated(a_id) &&
      (polyline_model_ == PolylineModel::CLOSED ||
       (!polyline_loops_have_boundaries_ &&
        a.v1 == a_shape_->chain_edge(chain_id_, chain_start_).v0)) &&
      IsPolylineVertexInside(r.a1_matches_polyline, r.a1_matches_polygon)) {
    if (!AddPointEdge(a.v1, 1)) return false;
  }
  return true;
}

// Returns true if the current point being processed (which must be a polyline
// vertex) is contained by the opposite region (after inversion if "invert_b_"
// is true).  "matches_polyline" and "matches_polygon" indicate whether the
// vertex matches a polyline/polygon vertex of the opposite region.
bool S2BooleanOperation::Impl::CrossingProcessor::IsPolylineVertexInside(
    bool matches_polyline, bool matches_polygon) const {
  // "contained" indicates whether the current point is inside the polygonal
  // interior of the opposite region using semi-open boundaries.
  bool contained = inside_ ^ invert_b_;

  // For UNION the output includes duplicate polylines.  The test below
  // ensures that isolated polyline vertices are not suppressed by other
  // polyline vertices in the output.
  if (matches_polyline && !is_union_) {
    contained = true;
  } else if (matches_polygon && polygon_model_ != PolygonModel::SEMI_OPEN) {
    contained = (polygon_model_ == PolygonModel::CLOSED);
  }
  // Finally, invert the result if the opposite region should be inverted.
  return contained ^ invert_b_;
}

// Returns true if the current polyline edge is contained by the opposite
// region (after inversion if "invert_b_" is true).
inline bool S2BooleanOperation::Impl::CrossingProcessor::IsPolylineEdgeInside(
    const EdgeCrossingResult& r) const {
  // "contained" indicates whether the current point is inside the polygonal
  // interior of the opposite region using semi-open boundaries.
  bool contained = inside_ ^ invert_b_;
  if (r.matches_polyline && !is_union_) {
    contained = true;
  } else if (r.matches_polygon) {
    // In the SEMI_OPEN model, polygon sibling pairs cancel each other and
    // have no effect on point or edge containment.
    if (!(r.matches_sibling && polygon_model_ == PolygonModel::SEMI_OPEN)) {
      contained = (polygon_model_ != PolygonModel::OPEN);
    }
  } else if (r.matches_sibling) {
    contained = (polygon_model_ == PolygonModel::CLOSED);
  }
  // Finally, invert the result if the opposite region should be inverted.
  return contained ^ invert_b_;
}

// Processes an edge of dimension 2 (i.e., a polygon edge) from region A.
//
// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::CrossingProcessor::ProcessEdge2(
    ShapeEdgeId a_id, const S2Shape::Edge& a, CrossingIterator* it) {
  // In order to keep only one copy of any shared polygon edges, we only
  // output shared edges when processing the second region.
  bool emit_shared = (a_region_id_ == 1);

  // Degeneracies such as isolated vertices and sibling pairs can only be
  // created by intersecting CLOSED polygons or unioning OPEN polygons.
  bool emit_degenerate =
      (polygon_model_ == PolygonModel::CLOSED && !invert_a_ && !invert_b_) ||
      (polygon_model_ == PolygonModel::OPEN && invert_a_ && invert_b_);

  EdgeCrossingResult r = ProcessEdgeCrossings(a_id, a, it);
  S2_DCHECK(!r.matches_polyline);
  inside_ ^= (r.a0_crossings & 1);

  // If only one region is inverted, matching/sibling relations are reversed.
  // TODO(ericv): Update the following code to handle degenerate loops.
  S2_DCHECK(!r.matches_polygon || !r.matches_sibling);
  if (invert_a_ != invert_b_) swap(r.matches_polygon, r.matches_sibling);

  // Test whether the entire polygon edge should be emitted (or not emitted)
  // because it matches a polygon edge or its sibling.
  bool new_inside = inside_;

  // Shared edge are emitted only while processing the second region.
  if (r.matches_polygon) new_inside = emit_shared;

  // Sibling pairs are emitted only when degeneracies are desired.
  if (r.matches_sibling) new_inside = emit_degenerate;
  if (inside_ != new_inside) {
    inside_ ^= true;   // Invert the inside_ state.
    ++r.a1_crossings;  // Restore the correct (semi-open) state later.
  }

  // Test whether the first vertex of this edge should be emitted as an
  // isolated degenerate vertex.
  if (a_id.edge_id == chain_start_) {
    chain_v0_emitted_ = inside_;
  } else if (emit_shared && emit_degenerate && r.a0_matches_polygon &&
             is_v0_isolated(a_id)) {
    if (!AddPointEdge(a.v0, 2)) return false;
  }

  // Test whether the entire edge or any part of it belongs to the output.
  if (inside_ || r.interior_crossings > 0) {
    // Note: updates "inside_" to correspond to the state just before a1.
    if (!AddEdge(a_id, a, 2 /*dimension*/, r.interior_crossings)) {
      return false;
    }
  }

  // Remember whether the edge portion just before "a1" was emitted, so that
  // we can decide whether "a1" need to be emitted as an isolated vertex.
  if (inside_) v0_emitted_max_edge_id_ = a_id.edge_id + 1;
  inside_ ^= (r.a1_crossings & 1);

  // Verify that edge crossings are being counted correctly.
  if (it->crossings_complete()) {
    S2_DCHECK_EQ(MakeS2ContainsPointQuery(&it->b_index()).Contains(a.v1),
              inside_ ^ invert_b_);
  }

  // Special case to test whether the last vertex of a loop should be emitted
  // as an isolated degenerate vertex.
  if (emit_shared && emit_degenerate && r.a1_matches_polygon &&
      it->crossings_complete() && is_chain_last_vertex_isolated(a_id)) {
    if (!AddPointEdge(a.v1, 2)) return false;
  }
  return true;
}

// Returns a summary of the relationship between a test edge from region A and
// a set of crossing edges from region B (see EdgeCrossingResult).
//
// NOTE(ericv): We could save a bit of work when matching polygon vertices by
// passing in a flag saying whether this information is needed.  For example
// if is only needed in ProcessEdge2 when (emit_shared && emit_degenerate).
S2BooleanOperation::Impl::CrossingProcessor::EdgeCrossingResult
S2BooleanOperation::Impl::CrossingProcessor::ProcessEdgeCrossings(
    ShapeEdgeId a_id, const S2Shape::Edge& a, CrossingIterator* it) {
  EdgeCrossingResult r;
  if (it->Done(a_id)) return r;

  // TODO(ericv): bool a_degenerate = (a.v0 == a.v1);
  for (; !it->Done(a_id); it->Next()) {
    // Polylines and polygons are not affected by point geometry.
    if (it->b_dimension() == 0) continue;
    S2Shape::Edge b = it->b_edge();
    if (it->is_interior_crossing()) {
      // The crossing occurs in the edge interior.  The condition below says
      // that (1) polyline crossings don't affect polygon output, and (2)
      // subtracting a crossing polyline from a polyline has no effect.
      if (a_dimension_ <= it->b_dimension() &&
          !(invert_b_ != invert_result_ && it->b_dimension() == 1)) {
        SourceId src_id(b_region_id_, it->b_shape_id(), it->b_edge_id());
        AddCrossing(make_pair(src_id, it->left_to_right()));
      }
      r.interior_crossings += (it->b_dimension() == 1) ? 2 : 1;
    } else if (it->b_dimension() == 1) {
      // Polygons are not affected by polyline geometry.
      if (a_dimension_ == 2) continue;
      if ((a.v0 == b.v0 && a.v1 == b.v1) || (a.v0 == b.v1 && a.v1 == b.v0)) {
        r.matches_polyline = true;
      }
      if ((a.v0 == b.v0 || a.v0 == b.v1) &&
          PolylineEdgeContainsVertex(a.v0, *it)) {
        r.a0_matches_polyline = true;
      }
      if ((a.v1 == b.v0 || a.v1 == b.v1) &&
          PolylineEdgeContainsVertex(a.v1, *it)) {
        r.a1_matches_polyline = true;
      }
    } else {
      S2_DCHECK_EQ(2, it->b_dimension());
      if (a.v0 == b.v0 && a.v1 == b.v1) {
        ++r.a0_crossings;
        r.matches_polygon = true;
      } else if (a.v0 == b.v1 && a.v1 == b.v0) {
        ++r.a0_crossings;
        r.matches_sibling = true;
      } else if (it->is_vertex_crossing()) {
        if (a.v0 == b.v0 || a.v0 == b.v1) {
          ++r.a0_crossings;
        } else {
          ++r.a1_crossings;
        }
      }
      if (a.v0 == b.v0 || a.v0 == b.v1) {
        r.a0_matches_polygon = true;
      }
      if (a.v1 == b.v0 || a.v1 == b.v1) {
        r.a1_matches_polygon = true;
      }
    }
  }
  return r;
}

// Returns true if the vertex "v" is contained by the polyline edge referred
// to by the CrossingIterator "it", taking into account the PolylineModel.
//
// REQUIRES: it.b_dimension() == 1
// REQUIRES: "v" is an endpoint of it.b_edge()
bool S2BooleanOperation::Impl::CrossingProcessor::PolylineEdgeContainsVertex(
    const S2Point& v, const CrossingIterator& it) const {
  S2_DCHECK_EQ(1, it.b_dimension());
  S2_DCHECK(it.b_edge().v0 == v || it.b_edge().v1 == v);

  // Closed polylines contain all their vertices.
  if (polyline_model_ == PolylineModel::CLOSED) return true;

  // Note that the code below is structured so that it.b_edge() is not usually
  // needed (since accessing the edge can be relatively expensive).
  const auto& b_chain = it.b_chain_info();
  int b_edge_id = it.b_edge_id();

  // The last polyline vertex is never contained.  (For polyline loops, it is
  // sufficient to treat the first vertex as begin contained.)  This case also
  // handles degenerate polylines (polylines with one edge where v0 == v1),
  // which do not contain any points.
  if (b_edge_id == b_chain.limit - 1 && v == it.b_edge().v1) return false;

  // Otherwise all interior vertices are contained.  The first polyline
  // vertex is contained if either the polyline model is not OPEN, or the
  // polyline forms a loop and polyline_loops_have_boundaries_ is false.
  if (polyline_contains_v0(b_edge_id, b_chain.start)) return true;
  if (v != it.b_edge().v0) return true;
  if (polyline_loops_have_boundaries_) return false;
  return v == it.b_shape().chain_edge(b_chain.chain_id,
                                      b_chain.limit - b_chain.start - 1).v1;
}

// Translates the temporary representation of crossing edges (SourceId) into
// the format expected by EdgeClippingLayer (InputEdgeId).
void S2BooleanOperation::Impl::CrossingProcessor::DoneBoundaryPair() {
  // Add entries that translate the "special" crossings.
  source_id_map_[SourceId(kSetInside)] = kSetInside;
  source_id_map_[SourceId(kSetInvertB)] = kSetInvertB;
  source_id_map_[SourceId(kSetReverseA)] = kSetReverseA;
  input_crossings_->reserve(input_crossings_->size() +
                            source_edge_crossings_.size());
  for (const auto& tmp : source_edge_crossings_) {
    auto it = source_id_map_.find(tmp.second.first);
    S2_DCHECK(it != source_id_map_.end());
    input_crossings_->push_back(make_pair(
        tmp.first, CrossingInputEdge(it->second, tmp.second.second)));
  }
  source_edge_crossings_.clear();
  source_id_map_.clear();
}

// Clips the boundary of A to the interior of the opposite region B and adds
// the resulting edges to the output.  Optionally, any combination of region
// A, region B, and the result may be inverted, which allows operations such
// as union and difference to be implemented.
//
// Note that when an input region is inverted with respect to the output
// (e.g., invert_a != invert_result), all polygon edges are reversed and all
// points and polylines are discarded, since the complement of such objects
// cannot be represented.  (If you want to compute the complement of points
// or polylines, you can use S2LaxPolygonShape to represent your geometry as
// degenerate polygons instead.)
//
// This method must be called an even number of times (first to clip A to B
// and then to clip B to A), calling DoneBoundaryPair() after each pair.
//
// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::AddBoundary(
    int a_region_id, bool invert_a, bool invert_b, bool invert_result,
    const vector<ShapeEdgeId>& a_chain_starts, CrossingProcessor* cp) {
  const S2ShapeIndex& a_index = *op_->regions_[a_region_id];
  const S2ShapeIndex& b_index = *op_->regions_[1 - a_region_id];
  if (!GetIndexCrossings(a_region_id)) return false;
  cp->StartBoundary(a_region_id, invert_a, invert_b, invert_result);

  // Walk the boundary of region A and build a list of all edge crossings.
  // We also keep track of whether the current vertex is inside region B.
  auto next_start = a_chain_starts.begin();
  CrossingIterator next_crossing(&b_index, &index_crossings_,
                                 true /*crossings_complete*/);
  ShapeEdgeId next_id = min(*next_start, next_crossing.a_id());
  while (next_id != kSentinel) {
    int a_shape_id = next_id.shape_id;
    const S2Shape& a_shape = *a_index.shape(a_shape_id);
    cp->StartShape(&a_shape);
    while (next_id.shape_id == a_shape_id) {
      // TODO(ericv): Special handling of dimension 0?  Can omit most of this
      // code, including the loop, since all chains are of length 1.
      int edge_id = next_id.edge_id;
      S2Shape::ChainPosition chain_position = a_shape.chain_position(edge_id);
      int chain_id = chain_position.chain_id;
      S2Shape::Chain chain = a_shape.chain(chain_id);
      bool start_inside = (next_id == *next_start);
      if (start_inside) ++next_start;
      cp->StartChain(chain_id, chain, start_inside);
      int chain_limit = chain.start + chain.length;
      while (edge_id < chain_limit) {
        ShapeEdgeId a_id(a_shape_id, edge_id);
        S2_DCHECK(cp->inside() || next_crossing.a_id() == a_id);
        if (!cp->ProcessEdge(a_id, &next_crossing)) {
          return false;
        }
        if (cp->inside()) {
          ++edge_id;
        } else if (next_crossing.a_id().shape_id == a_shape_id &&
                   next_crossing.a_id().edge_id < chain_limit) {
          edge_id = next_crossing.a_id().edge_id;
        } else {
          break;
        }
      }
      next_id = min(*next_start, next_crossing.a_id());
    }
  }
  return true;
}

// Returns the first edge of each edge chain from "a_region_id" whose first
// vertex is contained by opposite region's polygons (using the semi-open
// boundary model).  Each input region and the result region are inverted as
// specified (invert_a, invert_b, and invert_result) before testing for
// containment.  The algorithm uses these "chain starts" in order to clip the
// boundary of A to the interior of B in an output-senstive way.
//
// This method supports "early exit" in the case where a boolean predicate is
// being evaluated and the algorithm discovers that the result region will be
// non-empty.
bool S2BooleanOperation::Impl::GetChainStarts(
    int a_region_id, bool invert_a, bool invert_b, bool invert_result,
    CrossingProcessor* cp, vector<ShapeEdgeId>* chain_starts) {
  const S2ShapeIndex& a_index = *op_->regions_[a_region_id];
  const S2ShapeIndex& b_index = *op_->regions_[1 - a_region_id];

  if (is_boolean_output()) {
    // If boolean output is requested, then we use the CrossingProcessor to
    // determine whether the first edge of each chain will be emitted to the
    // output region.  This lets us terminate the operation early in many
    // cases.
    cp->StartBoundary(a_region_id, invert_a, invert_b, invert_result);
  }

  // If region B has no two-dimensional shapes and is not inverted, then by
  // definition no chain starts are contained.  However if boolean output is
  // requested then we check for containment anyway, since as a side effect we
  // may discover that the result region is non-empty and terminate the entire
  // operation early.
  bool b_has_interior = HasInterior(b_index);
  if (b_has_interior || invert_b || is_boolean_output()) {
    auto query = MakeS2ContainsPointQuery(&b_index);
    int num_shape_ids = a_index.num_shape_ids();
    for (int shape_id = 0; shape_id < num_shape_ids; ++shape_id) {
      S2Shape* a_shape = a_index.shape(shape_id);
      if (a_shape == nullptr) continue;

      // If region A is being subtracted from region B, points and polylines
      // in region A can be ignored since these shapes never contribute to the
      // output (they can only remove edges from region B).
      if (invert_a != invert_result && a_shape->dimension() < 2) continue;

      if (is_boolean_output()) cp->StartShape(a_shape);
      int num_chains = a_shape->num_chains();
      for (int chain_id = 0; chain_id < num_chains; ++chain_id) {
        S2Shape::Chain chain = a_shape->chain(chain_id);
        if (chain.length == 0) continue;
        ShapeEdge a(shape_id, chain.start, a_shape->chain_edge(chain_id, 0));
        bool inside = (b_has_interior && query.Contains(a.v0())) != invert_b;
        if (inside) {
          chain_starts->push_back(ShapeEdgeId(shape_id, chain.start));
        }
        if (is_boolean_output()) {
          cp->StartChain(chain_id, chain, inside);
          if (!ProcessIncidentEdges(a, &query, cp)) return false;
        }
      }
    }
  }
  chain_starts->push_back(kSentinel);
  return true;
}

bool S2BooleanOperation::Impl::ProcessIncidentEdges(
    const ShapeEdge& a, S2ContainsPointQuery<S2ShapeIndex>* query,
    CrossingProcessor* cp) {
  tmp_crossings_.clear();
  query->VisitIncidentEdges(a.v0(), [&a, this](const ShapeEdge& b) {
      return AddIndexCrossing(a, b, false /*is_interior*/, &tmp_crossings_);
    });
  // Fast path for the common case where there are no incident edges.  We
  // return false (terminating early) if the first chain edge will be emitted.
  if (tmp_crossings_.empty()) {
    return !cp->inside();
  }
  // Otherwise we invoke the full CrossingProcessor logic to determine whether
  // the first chain edge will be emitted.
  if (tmp_crossings_.size() > 1) {
    std::sort(tmp_crossings_.begin(), tmp_crossings_.end());
    // VisitIncidentEdges() should not generate any duplicate values.
    S2_DCHECK(std::adjacent_find(tmp_crossings_.begin(), tmp_crossings_.end()) ==
           tmp_crossings_.end());
  }
  tmp_crossings_.push_back(IndexCrossing(kSentinel, kSentinel));
  CrossingIterator next_crossing(&query->index(), &tmp_crossings_,
                                 false /*crossings_complete*/);
  return cp->ProcessEdge(a.id(),  &next_crossing);
}

bool S2BooleanOperation::Impl::HasInterior(const S2ShapeIndex& index) {
  for (int s = index.num_shape_ids(); --s >= 0; ) {
    S2Shape* shape = index.shape(s);
    if (shape && shape->dimension() == 2) return true;
  }
  return false;
}

inline bool S2BooleanOperation::Impl::AddIndexCrossing(
    const ShapeEdge& a, const ShapeEdge& b, bool is_interior,
    IndexCrossings* crossings) {
  crossings->push_back(IndexCrossing(a.id(), b.id()));
  IndexCrossing* crossing = &crossings->back();
  if (is_interior) {
    crossing->is_interior_crossing = true;
    if (s2pred::Sign(a.v0(), a.v1(), b.v0()) > 0) {
      crossing->left_to_right = true;
    }
  } else {
    // TODO(ericv): This field isn't used unless one shape is a polygon and
    // the other is a polyline or polygon, but we don't have the shape
    // dimension information readily available here.
    if (S2::VertexCrossing(a.v0(), a.v1(), b.v0(), b.v1())) {
      crossing->is_vertex_crossing = true;
    }
  }
  return true;  // Continue visiting.
}

// Initialize index_crossings_ to the set of crossing edge pairs such that the
// first element of each pair is an edge from "region_id".
//
// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::GetIndexCrossings(int region_id) {
  if (region_id == index_crossings_first_region_id_) return true;
  if (index_crossings_first_region_id_ < 0) {
    S2_DCHECK_EQ(region_id, 0);  // For efficiency, not correctness.
    if (!s2shapeutil::VisitCrossingEdgePairs(
            *op_->regions_[0], *op_->regions_[1],
            s2shapeutil::CrossingType::ALL,
            [this](const ShapeEdge& a, const ShapeEdge& b, bool is_interior) {
              // For all supported operations (union, intersection, and
              // difference), if the input edges have an interior crossing
              // then the output is guaranteed to have at least one edge.
              if (is_interior && is_boolean_output()) return false;
              return AddIndexCrossing(a, b, is_interior, &index_crossings_);
            })) {
      return false;
    }
    if (index_crossings_.size() > 1) {
      std::sort(index_crossings_.begin(), index_crossings_.end());
      index_crossings_.erase(
          std::unique(index_crossings_.begin(), index_crossings_.end()),
          index_crossings_.end());
    }
    // Add a sentinel value to simplify the loop logic.
    index_crossings_.push_back(IndexCrossing(kSentinel, kSentinel));
    index_crossings_first_region_id_ = 0;
  }
  if (region_id != index_crossings_first_region_id_) {
    for (auto& crossing : index_crossings_) {
      swap(crossing.a, crossing.b);
      // The following predicates get inverted when the edges are swapped.
      crossing.left_to_right ^= true;
      crossing.is_vertex_crossing ^= true;
    }
    std::sort(index_crossings_.begin(), index_crossings_.end());
    index_crossings_first_region_id_ = region_id;
  }
  return true;
}

// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::AddBoundaryPair(
    bool invert_a, bool invert_b, bool invert_result, CrossingProcessor* cp) {
  // Optimization: if the operation is DIFFERENCE or SYMMETRIC_DIFFERENCE,
  // it is worthwhile checking whether the two regions are identical (in which
  // case the output is empty).
  //
  // TODO(ericv): When boolean output is requested there are other quick
  // checks that could be done here, such as checking whether a full cell from
  // one S2ShapeIndex intersects a non-empty cell of the other S2ShapeIndex.
  auto type = op_->op_type();
  if (type == OpType::DIFFERENCE || type == OpType::SYMMETRIC_DIFFERENCE) {
    if (AreRegionsIdentical()) return true;
  } else if (!is_boolean_output()) {
  }
  vector<ShapeEdgeId> a_starts, b_starts;
  if (!GetChainStarts(0, invert_a, invert_b, invert_result, cp, &a_starts) ||
      !GetChainStarts(1, invert_b, invert_a, invert_result, cp, &b_starts) ||
      !AddBoundary(0, invert_a, invert_b, invert_result, a_starts, cp) ||
      !AddBoundary(1, invert_b, invert_a, invert_result, b_starts, cp)) {
    return false;
  }
  if (!is_boolean_output()) cp->DoneBoundaryPair();
  return true;
}

// Supports "early exit" in the case of boolean results by returning false
// as soon as the result is known to be non-empty.
bool S2BooleanOperation::Impl::BuildOpType(OpType op_type) {
  // CrossingProcessor does the real work of emitting the output edges.
  CrossingProcessor cp(op_->options_.polygon_model(),
                       op_->options_.polyline_model(),
                       op_->options_.polyline_loops_have_boundaries(),
                       builder_.get(), &input_dimensions_, &input_crossings_);
  switch (op_type) {
    case OpType::UNION:
      // A | B == ~(~A & ~B)
      return AddBoundaryPair(true, true, true, &cp);

    case OpType::INTERSECTION:
      // A & B
      return AddBoundaryPair(false, false, false, &cp);

    case OpType::DIFFERENCE:
      // A - B = A & ~B
      return AddBoundaryPair(false, true, false, &cp);

    case OpType::SYMMETRIC_DIFFERENCE:
      // Compute the union of (A - B) and (B - A).
      return (AddBoundaryPair(false, true, false, &cp) &&
              AddBoundaryPair(true, false, false, &cp));
  }
  S2_LOG(FATAL) << "Invalid S2BooleanOperation::OpType";
  return false;
}

// Given a polygon edge graph containing only degenerate edges and sibling
// edge pairs, the purpose of this function is to decide whether the polygon
// is empty or full except for the degeneracies, i.e. whether the degeneracies
// represent shells or holes.
//
// This function always returns false, meaning that the polygon is empty and
// the degeneracies represent shells.  The main side effect of this is that
// operations whose result should be the full polygon will instead be the
// empty polygon.  (Classes such as S2Polygon already have code to correct for
// this, but if that functionality were moved here then it would be useful for
// other polygon representations such as S2LaxPolygonShape.)
static bool IsFullPolygonNever(const S2Builder::Graph& g, S2Error* error) {
  return false;  // Assumes the polygon is empty.
}

bool S2BooleanOperation::Impl::AreRegionsIdentical() const {
  const S2ShapeIndex* a = op_->regions_[0];
  const S2ShapeIndex* b = op_->regions_[1];
  if (a == b) return true;
  int num_shape_ids = a->num_shape_ids();
  if (num_shape_ids != b->num_shape_ids()) return false;
  for (int s = 0; s < num_shape_ids; ++s) {
    const S2Shape* a_shape = a->shape(s);
    const S2Shape* b_shape = b->shape(s);
    if (a_shape->dimension() != b_shape->dimension()) return false;
    if (a_shape->dimension() == 2) {
      auto a_ref = a_shape->GetReferencePoint();
      auto b_ref = b_shape->GetReferencePoint();
      if (a_ref.point != b_ref.point) return false;
      if (a_ref.contained != b_ref.contained) return false;
    }
    int num_chains = a_shape->num_chains();
    if (num_chains != b_shape->num_chains()) return false;
    for (int c = 0; c < num_chains; ++c) {
      S2Shape::Chain a_chain = a_shape->chain(c);
      S2Shape::Chain b_chain = b_shape->chain(c);
      S2_DCHECK_EQ(a_chain.start, b_chain.start);
      if (a_chain.length != b_chain.length) return false;
      for (int i = 0; i < a_chain.length; ++i) {
        S2Shape::Edge a_edge = a_shape->chain_edge(c, i);
        S2Shape::Edge b_edge = b_shape->chain_edge(c, i);
        if (a_edge.v0 != b_edge.v0) return false;
        if (a_edge.v1 != b_edge.v1) return false;
      }
    }
  }
  return true;
}

bool S2BooleanOperation::Impl::Build(S2Error* error) {
  error->Clear();
  if (is_boolean_output()) {
    // BuildOpType() returns true if and only if the result is empty.
    *op_->result_empty_ = BuildOpType(op_->op_type());
    return true;
  }
  // TODO(ericv): Rather than having S2Builder split the edges, it would be
  // faster to call AddVertex() in this class and have a new S2Builder
  // option that increases the edge_snap_radius_ to account for errors in
  // the intersection point (the way that split_crossing_edges does).
  S2Builder::Options options(op_->options_.snap_function());
  options.set_split_crossing_edges(true);

  // TODO(ericv): Ideally idempotent() should be true, but existing clients
  // expect vertices closer than the full "snap_radius" to be snapped.
  options.set_idempotent(false);
  builder_ = make_unique<S2Builder>(options);
  builder_->StartLayer(make_unique<EdgeClippingLayer>(
      &op_->layers_, &input_dimensions_, &input_crossings_));

  // Polygons with no edges are assumed to be empty.  It is the responsibility
  // of clients to fix this if desired (e.g. S2Polygon has code for this).
  //
  // TODO(ericv): Implement a predicate that can determine whether a
  // degenerate polygon is empty or full based on the input S2ShapeIndexes.
  // (It is possible to do this 100% robustly, but tricky.)
  builder_->AddIsFullPolygonPredicate(IsFullPolygonNever);
  (void) BuildOpType(op_->op_type());
  return builder_->Build(error);
}

S2BooleanOperation::Options::Options()
    : snap_function_(make_unique<s2builderutil::IdentitySnapFunction>(
          S1Angle::Zero())) {
}

S2BooleanOperation::Options::Options(const SnapFunction& snap_function)
    : snap_function_(snap_function.Clone()) {
}

S2BooleanOperation::Options::Options(const Options& options)
    :  snap_function_(options.snap_function_->Clone()),
       polygon_model_(options.polygon_model_),
       polyline_model_(options.polyline_model_),
       polyline_loops_have_boundaries_(options.polyline_loops_have_boundaries_),
       precision_(options.precision_),
       conservative_output_(options.conservative_output_),
       source_id_lexicon_(options.source_id_lexicon_) {
}

S2BooleanOperation::Options& S2BooleanOperation::Options::operator=(
    const Options& options) {
  snap_function_ = options.snap_function_->Clone();
  polygon_model_ = options.polygon_model_;
  polyline_model_ = options.polyline_model_;
  polyline_loops_have_boundaries_ = options.polyline_loops_have_boundaries_;
  precision_ = options.precision_;
  conservative_output_ = options.conservative_output_;
  source_id_lexicon_ = options.source_id_lexicon_;
  return *this;
}

const SnapFunction& S2BooleanOperation::Options::snap_function() const {
  return *snap_function_;
}

void S2BooleanOperation::Options::set_snap_function(
    const SnapFunction& snap_function) {
  snap_function_ = snap_function.Clone();
}

PolygonModel S2BooleanOperation::Options::polygon_model() const {
  return polygon_model_;
}

void S2BooleanOperation::Options::set_polygon_model(PolygonModel model) {
  polygon_model_ = model;
}

PolylineModel S2BooleanOperation::Options::polyline_model() const {
  return polyline_model_;
}

void S2BooleanOperation::Options::set_polyline_model(PolylineModel model) {
  polyline_model_ = model;
}

bool S2BooleanOperation::Options::polyline_loops_have_boundaries() const {
  return polyline_loops_have_boundaries_;
}

void S2BooleanOperation::Options::set_polyline_loops_have_boundaries(
    bool value) {
  polyline_loops_have_boundaries_ = value;
}

Precision S2BooleanOperation::Options::precision() const {
  return precision_;
}

bool S2BooleanOperation::Options::conservative_output() const {
  return conservative_output_;
}

ValueLexicon<S2BooleanOperation::SourceId>*
S2BooleanOperation::Options::source_id_lexicon() const {
  return source_id_lexicon_;
}

const char* S2BooleanOperation::OpTypeToString(OpType op_type) {
  switch (op_type) {
    case OpType::UNION:                return "UNION";
    case OpType::INTERSECTION:         return "INTERSECTION";
    case OpType::DIFFERENCE:           return "DIFFERENCE";
    case OpType::SYMMETRIC_DIFFERENCE: return "SYMMETRIC DIFFERENCE";
    default:                           return "Unknown OpType";
  }
}

S2BooleanOperation::S2BooleanOperation(OpType op_type,
                                       const Options& options)
    : op_type_(op_type), options_(options), result_empty_(nullptr) {
}

S2BooleanOperation::S2BooleanOperation(OpType op_type, bool* result_empty,
                                       const Options& options)
    : op_type_(op_type), options_(options),
      result_empty_(result_empty) {
}

S2BooleanOperation::S2BooleanOperation(
    OpType op_type, unique_ptr<S2Builder::Layer> layer, const Options& options)
    : op_type_(op_type), options_(options), result_empty_(nullptr) {
  layers_.push_back(std::move(layer));
}

S2BooleanOperation::S2BooleanOperation(
    OpType op_type, vector<unique_ptr<S2Builder::Layer>> layers,
    const Options& options)
    : op_type_(op_type), options_(options), layers_(std::move(layers)),
      result_empty_(nullptr) {
}

bool S2BooleanOperation::Build(const S2ShapeIndex& a,
                               const S2ShapeIndex& b,
                               S2Error* error) {
  regions_[0] = &a;
  regions_[1] = &b;
  return Impl(this).Build(error);
}

bool S2BooleanOperation::IsEmpty(
    OpType op_type, const S2ShapeIndex& a, const S2ShapeIndex& b,
    const Options& options) {
  bool result_empty;
  S2BooleanOperation op(op_type, &result_empty, options);
  S2Error error;
  op.Build(a, b, &error);
  S2_DCHECK(error.ok());
  return result_empty;
}
