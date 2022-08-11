// Copyright Google Inc. All Rights Reserved.
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

// 2020 Google Inc. All Rights Reserved.
// Author: ericv@google.com (Eric Veach)

#include "s2/s2winding_operation.h"

#include <memory>
#include <utility>

#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2builderutil_get_snapped_winding_delta.h"
#include "s2/s2builderutil_graph_shape.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2crossing_edge_query.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_crossings.h"
#include "s2/s2shapeutil_shape_edge_id.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using SnapFunction = S2Builder::SnapFunction;

using Edge = Graph::Edge;
using EdgeId = Graph::EdgeId;
using InputEdgeId = Graph::InputEdgeId;
using InputEdgeIdSetId = Graph::InputEdgeIdSetId;
using VertexId = Graph::VertexId;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using ShapeEdgeId = s2shapeutil::ShapeEdgeId;
using WindingRule = S2WindingOperation::WindingRule;

namespace s2builderutil {

// The purpose of WindingOracle is to compute winding numbers with respect to
// a set of input loops after snapping.  It is given the input edges (via
// S2Builder), the output eges (an S2Builder::Graph), and the winding number
// at a reference point with respect to the input edges ( "ref_input_edge_id"
// and "ref_winding_in").  GetWindingNumber() can then be called to compute
// the winding number at any point with respect to the snapped loops.  The
// main algorithm uses this to compute the winding number at one arbitrary
// vertex of each connected component of the S2Builder output graph.
class WindingOracle {
 public:
  WindingOracle(InputEdgeId ref_input_edge_id, int ref_winding_in,
                const S2Builder& builder, const Graph* g);

  // Returns the winding number at the given point after snapping.
  int GetWindingNumber(const S2Point& p);

  // Returns the current reference point.
  S2Point current_ref_point() const { return ref_p_; }

  // Returns the winding number at the current reference point.
  int current_ref_winding() const { return ref_winding_; }

 private:
  int SignedCrossingDelta(S2EdgeCrosser* crosser, EdgeId e);

  const Graph& g_;

  // The current reference point.  Initially this is simply the snapped
  // position of the input reference point, but it is updated on each call to
  // GetWindingNumber (see below).
  S2Point ref_p_;

  // The winding number at the current reference point.
  int ref_winding_;

  // For each connected component of the S2Builder output graph, we determine
  // the winding number at one arbitrary vertex by counting edge crossings.
  // Initially we do this by brute force, but if there are too many connected
  // components then we build an S2ShapeIndex to speed up the process.
  //
  // Building an index takes about as long as 25 brute force queries.  The
  // classic competitive algorithm technique would be to do 25 brute force
  // queries and then build the index.  However in practice, inputs with one
  // connected component are common while inputs with 2-25 connected
  // components are rare.  Therefore we build the index as soon as we realize
  // that there is more than one component.
  //
  // Another idea is to count the connected components in advance, however it
  // turns out that this takes about 25% as long as building the index does.
  int brute_force_winding_tests_left_ = 1;
  MutableS2ShapeIndex index_;  // Built only if needed.
};

WindingOracle::WindingOracle(
    InputEdgeId ref_input_edge_id, int ref_winding_in,
    const S2Builder& builder, const Graph* g)
    : g_(*g) {
  S2_DCHECK(g_.options().edge_type() == EdgeType::DIRECTED);
  S2_DCHECK(g_.options().degenerate_edges() == DegenerateEdges::KEEP);
  S2_DCHECK(g_.options().duplicate_edges() == DuplicateEdges::KEEP);
  S2_DCHECK(g_.options().sibling_pairs() == SiblingPairs::KEEP);

  // Compute the winding number at the reference point after snapping (see
  // s2builderutil::GetSnappedWindingDelta).
  S2Point ref_in = builder.input_edge(ref_input_edge_id).v0;
  VertexId ref_v = s2builderutil::FindFirstVertexId(ref_input_edge_id, g_);
  S2_DCHECK_GE(ref_v, 0);  // No errors are possible.
  ref_p_ = g_.vertex(ref_v);
  S2Error error;
  ref_winding_ = ref_winding_in + s2builderutil::GetSnappedWindingDelta(
      ref_in, ref_v, s2builderutil::InputEdgeFilter{}, builder, g_, &error);
  S2_DCHECK(error.ok()) << error;  // No errors are possible.

  // Winding numbers at other points are computed by counting signed edge
  // crossings.  If we need to do this many times, it is worthwhile to build an
  // index.  Note that although we initialize the index here, it is only built
  // the first time we use it (i.e., when brute_force_winding_tests_left_ < 0).
  //
  // As a small optimization, we set max_edges_per_cell() higher than its
  // default value.  This makes it faster to build the index but increases the
  // time per query.  This is a good tradeoff because the number of calls to
  // GetWindingNumber() is typically small relative to the number of graph
  // edges.  It also saves memory, which is important when the graph is very
  // large (e.g. because the input loops have many self-intersections).
  constexpr int kMaxEdgesPerCell = 40;
  MutableS2ShapeIndex::Options options;
  options.set_max_edges_per_cell(kMaxEdgesPerCell);
  index_.Init(options);
  index_.set_memory_tracker(builder.options().memory_tracker());
  index_.Add(make_unique<s2builderutil::GraphShape>(&g_));
}

// Returns the change in winding number due to crossing the given graph edge.
inline int WindingOracle::SignedCrossingDelta(S2EdgeCrosser* crosser,
                                              EdgeId e) {
  return crosser->SignedEdgeOrVertexCrossing(&g_.vertex(g_.edge(e).first),
                                             &g_.vertex(g_.edge(e).second));
}

// Returns the winding number at the given point "p".
int WindingOracle::GetWindingNumber(const S2Point& p) {
  // Count signed edge crossings starting from the reference point, whose
  // winding number is known.  If we need to do this many times then we build
  // an S2ShapeIndex to speed up this process.
  S2EdgeCrosser crosser(&ref_p_, &p);
  int winding = ref_winding_;
  if (--brute_force_winding_tests_left_ >= 0) {
    for (EdgeId e = 0; e < g_.num_edges(); ++e) {
      winding += SignedCrossingDelta(&crosser, e);
    }
  } else {
    S2CrossingEdgeQuery query(&index_);
    for (ShapeEdgeId id : query.GetCandidates(ref_p_, p, *index_.shape(0))) {
      winding += SignedCrossingDelta(&crosser, id.edge_id);
    }
  }
  // It turns out that GetWindingNumber() is called with a sequence of points
  // that are sorted in approximate S2CellId order.  This means that if we
  // update our reference point as we go along, the edges for which we need to
  // count crossings are much shorter on average.
  ref_p_ = p;
  ref_winding_ = winding;
  return winding;
}

// The actual winding number operation is implemented as an S2Builder layer.
class WindingLayer : public S2Builder::Layer {
 public:
  WindingLayer(const S2WindingOperation* op,
               unique_ptr<S2Builder::Layer> result_layer)
      : op_(*op), result_layer_(std::move(result_layer)),
        tracker_(op->options().memory_tracker()) {
  }

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  bool ComputeBoundary(const Graph& g, WindingOracle* oracle, S2Error* error);
  EdgeId GetContainingLoopEdge(VertexId v, EdgeId start, const Graph& g,
                               const vector<EdgeId>& left_turn_map,
                               const vector<EdgeId>& sibling_map) const;
  bool MatchesRule(int winding) const;
  bool MatchesDegeneracy(int winding, int winding_minus, int winding_plus)
      const;
  inline void OutputEdge(const Graph& g, EdgeId e);

  // Constructor parameters.
  const S2WindingOperation& op_;
  unique_ptr<S2Builder::Layer> result_layer_;

  // The graph data that will be sent to result_layer_.
  vector<Edge> result_edges_;
  vector<InputEdgeIdSetId> result_input_edge_ids_;

  S2MemoryTracker::Client tracker_;
};

GraphOptions WindingLayer::graph_options() const {
  // The algorithm below has several steps with different graph requirements,
  // however the first step is to determine how snapping has affected the
  // winding number of the reference point.  This requires keeping all
  // degenerate edges and sibling pairs.  We also keep all duplicate edges
  // since this makes it easier to remove the reference edge.
  return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                      DuplicateEdges::KEEP, SiblingPairs::KEEP);
}

void WindingLayer::Build(const Graph& g, S2Error* error) {
  if (!error->ok()) return;  // Abort if an error already exists.

  // The WindingOracle computes the winding number of any point on the sphere.
  // It requires knowing the winding number at a reference point which must be
  // an input vertex to S2Builder.  This is achieved by adding a degenerate
  // edge from the reference point to itself (ref_input_edge_id_).  Once we
  // have computed the change in winding number, we create a new graph with
  // this edge removed (since it should not be emitted to the result layer).
  WindingOracle oracle(op_.ref_input_edge_id_, op_.ref_winding_in_,
                       op_.builder_, &g);
  S2_DCHECK(error->ok()) << *error;  // No errors are possible.

  // Now that we have computed the change in winding number, we create a new
  // graph with the reference edge removed.  Note that S2MemoryTracker errors
  // are automatically copied to "error" by S2Builder.
  vector<Edge> new_edges;
  vector<InputEdgeIdSetId> new_input_edge_ids;
  if (!tracker_.AddSpace(&new_edges, g.num_edges() - 1)) return;
  if (!tracker_.AddSpace(&new_input_edge_ids, g.num_edges() - 1)) return;
  IdSetLexicon new_input_edge_id_set_lexicon = g.input_edge_id_set_lexicon();

  // Copy all of the edges except the reference edge.
  for (int e = 0; e < g.num_edges(); ++e) {
    if (*g.input_edge_ids(e).begin() == op_.ref_input_edge_id_) continue;
    new_edges.push_back(g.edge(e));
    new_input_edge_ids.push_back(g.input_edge_id_set_id(e));
  }

  // Our goal is to assemble the given edges into loops that parition the
  // sphere.  In order to do this we merge duplicate edges and create sibling
  // edges so that every region can have its own directed boundary loop.
  //
  // Isolated degenerate edges and sibling pairs are preserved in order to
  // provide limited support for working with geometry of dimensions 0 and 1
  // (i.e., points and polylines).  Clients can simply convert these objects to
  // degenerate loops and then convert these degenerate loops back to
  // points/polylines in the output using s2builderutil::ClosedSetNormalizer.
  GraphOptions new_options(EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
                           DuplicateEdges::MERGE, SiblingPairs::CREATE);
  Graph new_graph = g.MakeSubgraph(
      new_options, &new_edges, &new_input_edge_ids,
      &new_input_edge_id_set_lexicon, nullptr, error, &tracker_);
  if (!error->ok()) return;

  // Now visit each connected component of the graph and assemble its edges
  // into loops.  For each loop we determine the winding number of the region
  // to its left, and if the winding number matches the given rule then we
  // add its edges to result_edges_.
  if (!ComputeBoundary(new_graph, &oracle, error)) return;

  // Now we construct yet another S2Builder::Graph by starting with the edges
  // that bound the desired output region and then processing them according to
  // the client's requested GraphOptions.  (Note that ProcessEdges can change
  // these options in certain cases; see S2Builder::GraphOptions for details.)
  //
  // The IsFullPolygonPredicate below allows clients to distinguish full from
  // empty results (including cases where there are degeneracies).  Note that
  // we can use the winding number at any point on the sphere for this
  // purpose.
  auto is_full_polygon_predicate = [&](const Graph& g, S2Error* error) {
      return MatchesRule(oracle.current_ref_winding());
    };
  IdSetLexicon result_id_set_lexicon = new_graph.input_edge_id_set_lexicon();
  Graph result_graph = new_graph.MakeSubgraph(
      result_layer_->graph_options(), &result_edges_, &result_input_edge_ids_,
      &result_id_set_lexicon, is_full_polygon_predicate, error, &tracker_);
  if (tracker_.Clear(&new_edges) && tracker_.Clear(&new_input_edge_ids)) {
    result_layer_->Build(result_graph, error);
  }
}

bool WindingLayer::ComputeBoundary(const Graph& g, WindingOracle* oracle,
                                   S2Error* error) {
  // We assemble the edges into loops using an algorithm similar to
  // S2Builder::Graph::GetDirectedComponents(), except that we also keep track
  // of winding numbers and output the relevant edges as we go along.
  //
  // The following accounts for sibling_map, left_turn_map, and edge_winding
  // (which have g.num_edges() elements each).
  const int64 kTempUsage = 3 * sizeof(EdgeId) * g.num_edges();
  if (!tracker_.Tally(kTempUsage)) return false;

  vector<EdgeId> sibling_map = g.GetSiblingMap();
  vector<EdgeId> left_turn_map;
  g.GetLeftTurnMap(sibling_map, &left_turn_map, error);
  S2_DCHECK(error->ok()) << *error;

  // A map from EdgeId to the winding number of the region it bounds.
  vector<int> edge_winding(g.num_edges());
  vector<EdgeId> frontier;  // Unexplored sibling edges.
  for (EdgeId e_min = 0; e_min < g.num_edges(); ++e_min) {
    if (left_turn_map[e_min] < 0) continue;  // Already visited.

    // We have found a new connected component of the graph.  Each component
    // consists of a set of loops that partition the sphere.  We start by
    // choosing an arbitrary vertex "v0" and computing the winding number at
    // this vertex.  Recall that point containment is defined such that when a
    // set of loops partition the sphere, every point is contained by exactly
    // one loop.  Therefore the winding number at "v0" is the same as the
    // winding number of the adjacent loop that contains it.  We choose "e0" to
    // be an arbitrary edge of this loop (it is the incoming edge to "v0").
    VertexId v0 = g.edge(e_min).second;
    EdgeId e0 = GetContainingLoopEdge(v0, e_min, g, left_turn_map, sibling_map);
    edge_winding[e0] = oracle->GetWindingNumber(g.vertex(v0));

    // Now visit all loop edges in this connected component of the graph.
    // "frontier" is a stack of unexplored siblings of the edges visited far.
    if (!tracker_.AddSpace(&frontier, 1)) return false;
    frontier.push_back(e0);
    while (!frontier.empty()) {
      EdgeId e = frontier.back();
      frontier.pop_back();
      if (left_turn_map[e] < 0) continue;  // Already visited.

      // Visit all edges of the loop starting at "e".
      int winding = edge_winding[e];
      for (EdgeId next; left_turn_map[e] >= 0; e = next) {
        // Count signed edge crossings to determine the winding number of
        // the sibling region.  Input edges that snapped to "e" decrease the
        // winding number by one (since we cross them from left to right),
        // while input edges that snapped to its sibling edge increase the
        // winding number by one (since we cross them from right to left).
        EdgeId sibling = sibling_map[e];
        int winding_minus = g.input_edge_ids(e).size();
        int winding_plus = g.input_edge_ids(sibling).size();
        int sibling_winding = winding - winding_minus + winding_plus;

        // Output all edges that bound the region selected by the winding
        // rule, plus certain degenerate edges.
        if ((MatchesRule(winding) && !MatchesRule(sibling_winding)) ||
            MatchesDegeneracy(winding, winding_minus, winding_plus)) {
          OutputEdge(g, e);
        }
        next = left_turn_map[e];
        left_turn_map[e] = -1;
        // If the sibling hasn't been visited yet, add it to the frontier.
        if (left_turn_map[sibling] >= 0) {
          edge_winding[sibling] = sibling_winding;
          if (!tracker_.AddSpace(&frontier, 1)) return false;
          frontier.push_back(sibling);
        }
      }
    }
  }
  tracker_.Untally(frontier);
  return tracker_.Tally(-kTempUsage);
}

// Given an incoming edge "start" to a vertex "v", returns an edge of the loop
// that contains "v" with respect to the usual semi-open boundary rules.
EdgeId WindingLayer::GetContainingLoopEdge(
    VertexId v, EdgeId start, const Graph& g,
    const vector<EdgeId>& left_turn_map,
    const vector<EdgeId>& sibling_map) const {
  // If the given edge is degenerate, this is an isolated vertex.
  S2_DCHECK_EQ(g.edge(start).second, v);
  if (g.edge(start).first == v) return start;

  // Otherwise search for the loop that contains "v".
  EdgeId e0 = start;
  for (;;) {
    EdgeId e1 = left_turn_map[e0];
    S2_DCHECK_EQ(g.edge(e0).second, v);
    S2_DCHECK_EQ(g.edge(e1).first, v);

    // The first test below handles the case where there are only two edges
    // incident to this vertex (i.e., the vertex angle is 360 degrees).
    if (g.edge(e0).first == g.edge(e1).second ||
        S2::AngleContainsVertex(g.vertex(g.edge(e0).first), g.vertex(v),
                                g.vertex(g.edge(e1).second))) {
      return e0;
    }
    e0 = sibling_map[e1];
    S2_DCHECK_NE(e0, start);
  }
}

bool WindingLayer::MatchesRule(int winding) const {
  switch (op_.rule_) {
    case WindingRule::POSITIVE:  return winding > 0;
    case WindingRule::NEGATIVE:  return winding < 0;
    case WindingRule::NON_ZERO:  return winding != 0;
    case WindingRule::ODD:       return (winding & 1) != 0;
  }
}

bool WindingLayer::MatchesDegeneracy(int winding, int winding_minus,
                                     int winding_plus) const {
  if (!op_.options_.include_degeneracies()) return false;

  // A degeneracy is either a self-loop or a sibling pair where equal numbers
  // of input edges snapped to both edges of the pair.  The test below covers
  // both cases (because self-loops are their own sibling).
  if (winding_minus != winding_plus) return false;

  if (op_.rule_ == WindingRule::ODD) {
    // Any degeneracy whose multiplicity is odd should be part of the result
    // independent of the winding number of the region that contains it.
    // This rule allows computing symmetric differences of any combination of
    // points, polylines, and polygons (where the first two are represented as
    // degenerate loops).
    return (winding_plus & 1) != 0;
  } else {
    // For all other winding rules we output degeneracies only if they are
    // contained by a region of winding number zero.  Even though the interface
    // to this class does not provide enough information to allow consistent
    // handling of degeneracies in general, this rule is sufficient for several
    // important cases.  Specifically it allows computing N-way unions of any
    // mixture of points, polylines, and polygons by converting the points and
    // polylines to degenerate loops.  In this case all input loops are
    // degenerate or CCW, and the boundary of the result can be computed using
    // WindingRule::POSITIVE.  Since there are no clockwise loops, all
    // degeneracies contained by a region of winding number zero represent
    // degenerate shells and should be emitted.  (They can be converted back to
    // points/polylines using s2builderutil::ClosedSetNormalizer.)
    //
    // Similarly, this heuristic is sufficient to compute unions of points,
    // polylines, and polygons where all boundaries are clockwise (by using
    // WindingRule::NEGATIVE) or where all boundaries are of an unknown but
    // consistent oreientation (by using WindingRule::NON_ZERO).
    return winding == 0;
  }
}

// Adds an edge to the set of output edges.
inline void WindingLayer::OutputEdge(const Graph& g, EdgeId e) {
  if (!tracker_.AddSpace(&result_edges_, 1)) return;
  if (!tracker_.AddSpace(&result_input_edge_ids_, 1)) return;
  result_edges_.push_back(g.edge(e));
  result_input_edge_ids_.push_back(g.input_edge_id_set_id(e));
}

}  // namespace s2builderutil

S2WindingOperation::Options::Options()
    : snap_function_(
          make_unique<s2builderutil::IdentitySnapFunction>(S1Angle::Zero())) {
}

S2WindingOperation::Options::Options(const SnapFunction& snap_function)
    : snap_function_(snap_function.Clone()) {
}

S2WindingOperation::Options::Options(const Options& options)
    : snap_function_(options.snap_function_->Clone()),
      include_degeneracies_(options.include_degeneracies_),
      memory_tracker_(options.memory_tracker_) {
}

S2WindingOperation::Options& S2WindingOperation::Options::operator=(
    const Options& options) {
  snap_function_ = options.snap_function_->Clone();
  include_degeneracies_ = options.include_degeneracies_;
  memory_tracker_ = options.memory_tracker_;
  return *this;
}

const S2Builder::SnapFunction& S2WindingOperation::Options::snap_function()
    const {
  return *snap_function_;
}

void S2WindingOperation::Options::set_snap_function(
    const SnapFunction& snap_function) {
  snap_function_ = snap_function.Clone();
}

bool S2WindingOperation::Options::include_degeneracies() const {
  return include_degeneracies_;
}

void S2WindingOperation::Options::set_include_degeneracies(
    bool include_degeneracies) {
  include_degeneracies_ = include_degeneracies;
}

S2MemoryTracker* S2WindingOperation::Options::memory_tracker() const {
  return memory_tracker_;
}

void S2WindingOperation::Options::set_memory_tracker(S2MemoryTracker* tracker) {
  memory_tracker_ = tracker;
}

S2WindingOperation::S2WindingOperation() {
}

S2WindingOperation::S2WindingOperation(
    unique_ptr<S2Builder::Layer> result_layer, const Options& options) {
  Init(std::move(result_layer), options);
}

void S2WindingOperation::Init(std::unique_ptr<S2Builder::Layer> result_layer,
                              const Options& options) {
  options_ = options;
  S2Builder::Options builder_options{options_.snap_function()};
  builder_options.set_split_crossing_edges(true);
  builder_options.set_memory_tracker(options.memory_tracker());
  builder_.Init(builder_options);
  builder_.StartLayer(make_unique<s2builderutil::WindingLayer>(
      this, std::move(result_layer)));
}

const S2WindingOperation::Options& S2WindingOperation::options() const {
  return options_;
}

void S2WindingOperation::AddLoop(S2PointLoopSpan loop) {
  builder_.AddLoop(loop);
}

bool S2WindingOperation::Build(const S2Point& ref_p, int ref_winding,
                               WindingRule rule, S2Error* error) {
  // The reference point must be an S2Builder input vertex in order to
  // determine how its winding number is affected by snapping.  We record the
  // input edge id of this edge so that we can find it later.
  ref_input_edge_id_ = builder_.num_input_edges();
  builder_.AddPoint(ref_p);
  ref_winding_in_ = ref_winding;
  rule_ = rule;
  return builder_.Build(error);
}
