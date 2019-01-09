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

#include "s2/s2builder_graph.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>
#include "s2/base/logging.h"
#include "s2/id_set_lexicon.h"
#include "s2/s2builder.h"
#include "s2/s2error.h"
#include "s2/s2predicates.h"
#include "s2/util/gtl/btree_map.h"

using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::vector;

using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

Graph::Graph(const GraphOptions& options,
             const vector<S2Point>* vertices,
             const vector<Edge>* edges,
             const vector<InputEdgeIdSetId>* input_edge_id_set_ids,
             const IdSetLexicon* input_edge_id_set_lexicon,
             const vector<LabelSetId>* label_set_ids,
             const IdSetLexicon* label_set_lexicon,
             IsFullPolygonPredicate is_full_polygon_predicate)
    : options_(options), num_vertices_(vertices->size()), vertices_(vertices),
      edges_(edges), input_edge_id_set_ids_(input_edge_id_set_ids),
      input_edge_id_set_lexicon_(input_edge_id_set_lexicon),
      label_set_ids_(label_set_ids),
      label_set_lexicon_(label_set_lexicon),
      is_full_polygon_predicate_(std::move(is_full_polygon_predicate)) {
  S2_DCHECK(std::is_sorted(edges->begin(), edges->end()));
}

vector<Graph::EdgeId> Graph::GetInEdgeIds() const {
  vector<EdgeId> in_edge_ids(num_edges());
  std::iota(in_edge_ids.begin(), in_edge_ids.end(), 0);
  std::sort(in_edge_ids.begin(), in_edge_ids.end(),
            [this](EdgeId ai, EdgeId bi) {
      return StableLessThan(reverse(edge(ai)), reverse(edge(bi)), ai, bi);
    });
  return in_edge_ids;
}

vector<Graph::EdgeId> Graph::GetSiblingMap() const {
  vector<EdgeId> in_edge_ids = GetInEdgeIds();
  MakeSiblingMap(&in_edge_ids);
  return in_edge_ids;
}

void Graph::MakeSiblingMap(vector<Graph::EdgeId>* in_edge_ids) const {
  S2_DCHECK(options_.sibling_pairs() == SiblingPairs::REQUIRE ||
         options_.sibling_pairs() == SiblingPairs::CREATE ||
         options_.edge_type() == EdgeType::UNDIRECTED);
  for (EdgeId e = 0; e < num_edges(); ++e) {
    S2_DCHECK(edge(e) == reverse(edge((*in_edge_ids)[e])));
  }
  if (options_.edge_type() == EdgeType::DIRECTED) return;
  if (options_.degenerate_edges() == DegenerateEdges::DISCARD) return;

  for (EdgeId e = 0; e < num_edges(); ++e) {
    VertexId v = edge(e).first;
    if (edge(e).second == v) {
      S2_DCHECK_LT(e + 1, num_edges());
      S2_DCHECK_EQ(edge(e + 1).first, v);
      S2_DCHECK_EQ(edge(e + 1).second, v);
      S2_DCHECK_EQ((*in_edge_ids)[e], e);
      S2_DCHECK_EQ((*in_edge_ids)[e + 1], e + 1);
      (*in_edge_ids)[e] = e + 1;
      (*in_edge_ids)[e + 1] = e;
      ++e;
    }
  }
}

Graph::VertexOutMap::VertexOutMap(const Graph& g)
    : edges_(g.edges()), edge_begins_(g.num_vertices() + 1) {
  EdgeId e = 0;
  for (VertexId v = 0; v <= g.num_vertices(); ++v) {
    while (e < g.num_edges() && g.edge(e).first < v) ++e;
    edge_begins_[v] = e;
  }
}

Graph::VertexInMap::VertexInMap(const Graph& g)
    : in_edge_ids_(g.GetInEdgeIds()),
      in_edge_begins_(g.num_vertices() + 1) {
  EdgeId e = 0;
  for (VertexId v = 0; v <= g.num_vertices(); ++v) {
    while (e < g.num_edges() && g.edge(in_edge_ids_[e]).second < v) ++e;
    in_edge_begins_[v] = e;
  }
}

Graph::LabelFetcher::LabelFetcher(const Graph& g, S2Builder::EdgeType edge_type)
    : g_(g), edge_type_(edge_type) {
  if (edge_type == EdgeType::UNDIRECTED) sibling_map_ = g.GetSiblingMap();
}

void Graph::LabelFetcher::Fetch(EdgeId e, vector<S2Builder::Label>* labels) {
  labels->clear();
  for (InputEdgeId input_edge_id : g_.input_edge_ids(e)) {
    for (Label label : g_.labels(input_edge_id)) {
      labels->push_back(label);
    }
  }
  if (edge_type_ == EdgeType::UNDIRECTED) {
    for (InputEdgeId input_edge_id : g_.input_edge_ids(sibling_map_[e])) {
      for (Label label : g_.labels(input_edge_id)) {
        labels->push_back(label);
      }
    }
  }
  if (labels->size() > 1) {
    std::sort(labels->begin(), labels->end());
    labels->erase(std::unique(labels->begin(), labels->end()), labels->end());
  }
}

S2Builder::InputEdgeId Graph::min_input_edge_id(EdgeId e) const {
  IdSetLexicon::IdSet id_set = input_edge_ids(e);
  return (id_set.size() == 0) ? kNoInputEdgeId : *id_set.begin();
}

vector<S2Builder::InputEdgeId> Graph::GetMinInputEdgeIds() const {
  vector<InputEdgeId> min_input_ids(num_edges());
  for (EdgeId e = 0; e < num_edges(); ++e) {
    min_input_ids[e] = min_input_edge_id(e);
  }
  return min_input_ids;
}

vector<Graph::EdgeId> Graph::GetInputEdgeOrder(
    const vector<InputEdgeId>& input_ids) const {
  vector<EdgeId> order(input_ids.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&input_ids](EdgeId a, EdgeId b) {
      // Comparison function ensures sort is stable.
      return make_pair(input_ids[a], a) < make_pair(input_ids[b], b);
    });
  return order;
}

// A struct for sorting the incoming and outgoing edges around a vertex "v0".
struct VertexEdge {
  VertexEdge(bool _incoming, Graph::EdgeId _index,
             Graph::VertexId _endpoint, int32 _rank)
      : incoming(_incoming), index(_index),
        endpoint(_endpoint), rank(_rank) {
  }
  bool incoming;             // Is this an incoming edge to "v0"?
  Graph::EdgeId index;       // Index of this edge in "edges_" or "in_edge_ids"
  Graph::VertexId endpoint;  // The other (not "v0") endpoint of this edge
  int32 rank;                // Secondary key for edges with the same endpoint
};

// Given a set of duplicate outgoing edges (v0, v1) and a set of duplicate
// incoming edges (v1, v0), this method assigns each edge an integer "rank" so
// that the edges are sorted in a consistent order with respect to their
// orderings around "v0" and "v1".  Usually there is just one edge, in which
// case this is easy.  Sometimes there is one edge in each direction, in which
// case the outgoing edge is always ordered before the incoming edge.
//
// In general, we allow any number of duplicate edges in each direction, in
// which case outgoing edges are interleaved with incoming edges so as to
// create as many degenerate (two-edge) loops as possible.  In order to get a
// consistent ordering around "v0" and "v1", we move forwards through the list
// of outgoing edges and backwards through the list of incoming edges.  If
// there are more incoming edges, they go at the beginning of the ordering,
// while if there are more outgoing edges then they go at the end.
//
// For example, suppose there are 2 edges "a,b" from "v0" to "v1", and 4 edges
// "w,x,y,z" from "v1" to "v0".  Using lower/upper case letters to represent
// incoming/outgoing edges, the clockwise ordering around v0 would be zyAxBw,
// and the clockwise ordering around v1 would be WbXaYZ.  (Try making a
// diagram with each edge as a separate arc.)
static void AddVertexEdges(Graph::EdgeId out_begin, Graph::EdgeId out_end,
                           Graph::EdgeId in_begin, Graph::EdgeId in_end,
                           Graph::VertexId v1, vector<VertexEdge>* v0_edges) {
  int rank = 0;
  // Any extra incoming edges go at the beginning of the ordering.
  while (in_end - in_begin > out_end - out_begin) {
    v0_edges->push_back(VertexEdge(true, --in_end, v1, rank++));
  }
  // Next we interleave as many outgoing and incoming edges as possible.
  while (in_end > in_begin) {
    v0_edges->push_back(VertexEdge(false, out_begin++, v1, rank++));
    v0_edges->push_back(VertexEdge(true, --in_end, v1, rank++));
  }
  // Any extra outgoing edges to at the end of the ordering.
  while (out_end > out_begin) {
    v0_edges->push_back(VertexEdge(false, out_begin++, v1, rank++));
  }
}

bool Graph::GetLeftTurnMap(const vector<EdgeId>& in_edge_ids,
                           vector<EdgeId>* left_turn_map,
                           S2Error* error) const {
  left_turn_map->assign(num_edges(), -1);
  if (num_edges() == 0) return true;

  // Declare vectors outside the loop to avoid reallocating them each time.
  vector<VertexEdge> v0_edges;
  vector<EdgeId> e_in, e_out;

  // Walk through the two sorted arrays of edges (outgoing and incoming) and
  // gather all the edges incident to each vertex.  Then we sort those edges
  // and add an entry to the left turn map from each incoming edge to the
  // immediately following outgoing edge in clockwise order.
  int out = 0, in = 0;
  const Edge* out_edge = &edge(out);
  const Edge* in_edge = &edge(in_edge_ids[in]);
  Edge sentinel(num_vertices(), num_vertices());
  Edge min_edge = min(*out_edge, reverse(*in_edge));
  while (min_edge != sentinel) {
    // Gather all incoming and outgoing edges around vertex "v0".
    VertexId v0 = min_edge.first;
    for (; min_edge.first == v0; min_edge = min(*out_edge, reverse(*in_edge))) {
      VertexId v1 = min_edge.second;
      // Count the number of copies of "min_edge" in each direction.
      int out_begin = out, in_begin = in;
      while (*out_edge == min_edge) {
        out_edge = (++out == num_edges()) ? &sentinel : &edge(out);
      }
      while (reverse(*in_edge) == min_edge) {
        in_edge = (++in == num_edges()) ? &sentinel : &edge(in_edge_ids[in]);
      }
      if (v0 != v1) {
        AddVertexEdges(out_begin, out, in_begin, in, v1, &v0_edges);
      } else {
        // Each degenerate edge becomes its own loop.
        for (; in_begin < in; ++in_begin) {
          (*left_turn_map)[in_begin] = in_begin;
        }
      }
    }
    if (v0_edges.empty()) continue;

    // Sort the edges in clockwise order around "v0".
    VertexId min_endpoint = v0_edges.front().endpoint;
    std::sort(v0_edges.begin() + 1, v0_edges.end(),
              [v0, min_endpoint, this](const VertexEdge& a,
                                       const VertexEdge& b) {
        if (a.endpoint == b.endpoint) return a.rank < b.rank;
        if (a.endpoint == min_endpoint) return true;
        if (b.endpoint == min_endpoint) return false;
        return !s2pred::OrderedCCW(vertex(a.endpoint), vertex(b.endpoint),
                                   vertex(min_endpoint), vertex(v0));
      });
    // Match incoming with outgoing edges.  We do this by keeping a stack of
    // unmatched incoming edges.  We also keep a stack of outgoing edges with
    // no previous incoming edge, and match these at the end by wrapping
    // around circularly to the start of the edge ordering.
    for (const VertexEdge& e : v0_edges) {
      if (e.incoming) {
        e_in.push_back(in_edge_ids[e.index]);
      } else if (!e_in.empty()) {
        (*left_turn_map)[e_in.back()] = e.index;
        e_in.pop_back();
      } else {
        e_out.push_back(e.index);  // Matched below.
      }
    }
    // Pair up additional edges using the fact that the ordering is circular.
    std::reverse(e_out.begin(), e_out.end());
    for (; !e_out.empty() && !e_in.empty(); e_out.pop_back(), e_in.pop_back()) {
      (*left_turn_map)[e_in.back()] = e_out.back();
    }
    // We only need to process unmatched incoming edges, since we are only
    // responsible for creating left turn map entries for those edges.
    if (!e_in.empty() && error->ok()) {
      error->Init(S2Error::BUILDER_EDGES_DO_NOT_FORM_LOOPS,
                  "Given edges do not form loops (indegree != outdegree)");
    }
    e_in.clear();
    e_out.clear();
    v0_edges.clear();
  }
  return error->ok();
}

void Graph::CanonicalizeLoopOrder(const vector<InputEdgeId>& min_input_ids,
                                  vector<EdgeId>* loop) {
  if (loop->empty()) return;
  // Find the position of the element with the highest input edge id.  If
  // there are multiple such elements together (i.e., the edge was split
  // into several pieces by snapping it to several vertices), then we choose
  // the last such position in cyclic order (this attempts to preserve the
  // original loop order even when new vertices are added).  For example, if
  // the input edge id sequence is (7, 7, 4, 5, 6, 7) then we would rotate
  // it to obtain (4, 5, 6, 7, 7, 7).

  // The reason that we put the highest-numbered edge last, rather than the
  // lowest-numbered edge first, is that S2Loop::Invert() reverses the loop
  // edge order *except* for the last edge.  For example, the loop ABCD (with
  // edges AB, BC, CD, DA) becomes DCBA (with edges DC, CB, BA, AD).  Note
  // that the last edge is the same except for its direction (DA vs. AD).
  // This has the advantage that if an undirected loop is assembled with the
  // wrong orientation and later inverted (e.g. by S2Polygon::InitOriented),
  // we still end up preserving the original cyclic vertex order.
  int pos = 0;
  bool saw_gap = false;
  for (int i = 1; i < loop->size(); ++i) {
    int cmp = min_input_ids[(*loop)[i]] - min_input_ids[(*loop)[pos]];
    if (cmp < 0) {
      saw_gap = true;
    } else if (cmp > 0 || !saw_gap) {
      pos = i;
      saw_gap = false;
    }
  }
  if (++pos == loop->size()) pos = 0;  // Convert loop end to loop start.
  std::rotate(loop->begin(), loop->begin() + pos, loop->end());
}

void Graph::CanonicalizeVectorOrder(const vector<InputEdgeId>& min_input_ids,
                                    vector<vector<EdgeId>>* chains) {
  std::sort(chains->begin(), chains->end(),
    [&min_input_ids](const vector<EdgeId>& a, const vector<EdgeId>& b) {
      return min_input_ids[a[0]] < min_input_ids[b[0]];
    });
}

bool Graph::GetDirectedLoops(LoopType loop_type, vector<EdgeLoop>* loops,
                             S2Error* error) const {
  S2_DCHECK(options_.degenerate_edges() == DegenerateEdges::DISCARD ||
         options_.degenerate_edges() == DegenerateEdges::DISCARD_EXCESS);
  S2_DCHECK(options_.edge_type() == EdgeType::DIRECTED);

  vector<EdgeId> left_turn_map;
  if (!GetLeftTurnMap(GetInEdgeIds(), &left_turn_map, error)) return false;
  vector<InputEdgeId> min_input_ids = GetMinInputEdgeIds();

  // If we are breaking loops at repeated vertices, we maintain a map from
  // VertexId to its position in "path".
  vector<int> path_index;
  if (loop_type == LoopType::SIMPLE) path_index.assign(num_vertices(), -1);

  // Visit edges in arbitrary order, and try to build a loop from each edge.
  vector<EdgeId> path;
  for (EdgeId start = 0; start < num_edges(); ++start) {
    if (left_turn_map[start] < 0) continue;

    // Build a loop by making left turns at each vertex until we return to
    // "start".  We use "left_turn_map" to keep track of which edges have
    // already been visited by setting its entries to -1 as we go along.  If
    // we are building vertex cycles, then whenever we encounter a vertex that
    // is already part of the path, we "peel off" a loop by removing those
    // edges from the path so far.
    for (EdgeId e = start, next; left_turn_map[e] >= 0; e = next) {
      path.push_back(e);
      next = left_turn_map[e];
      left_turn_map[e] = -1;
      if (loop_type == LoopType::SIMPLE) {
        path_index[edge(e).first] = path.size() - 1;
        int loop_start = path_index[edge(e).second];
        if (loop_start < 0) continue;
        // Peel off a loop from the path.
        vector<EdgeId> loop(path.begin() + loop_start, path.end());
        path.erase(path.begin() + loop_start, path.end());
        for (EdgeId e2 : loop) path_index[edge(e2).first] = -1;
        CanonicalizeLoopOrder(min_input_ids, &loop);
        loops->push_back(std::move(loop));
      }
    }
    if (loop_type == LoopType::SIMPLE) {
      S2_DCHECK(path.empty());  // Invariant.
    } else {
      CanonicalizeLoopOrder(min_input_ids, &path);
      loops->push_back(std::move(path));
      path.clear();
    }
  }
  CanonicalizeVectorOrder(min_input_ids, loops);
  return true;
}

bool Graph::GetDirectedComponents(
    DegenerateBoundaries degenerate_boundaries,
    vector<DirectedComponent>* components, S2Error* error) const {
  S2_DCHECK(options_.degenerate_edges() == DegenerateEdges::DISCARD ||
         (options_.degenerate_edges() == DegenerateEdges::DISCARD_EXCESS &&
          degenerate_boundaries == DegenerateBoundaries::KEEP));
  S2_DCHECK(options_.sibling_pairs() == SiblingPairs::REQUIRE ||
         options_.sibling_pairs() == SiblingPairs::CREATE);
  S2_DCHECK(options_.edge_type() == EdgeType::DIRECTED);  // Implied by above.

  vector<EdgeId> sibling_map = GetInEdgeIds();
  vector<EdgeId> left_turn_map;
  if (!GetLeftTurnMap(sibling_map, &left_turn_map, error)) return false;
  MakeSiblingMap(&sibling_map);
  vector<InputEdgeId> min_input_ids = GetMinInputEdgeIds();
  vector<EdgeId> frontier;  // Unexplored sibling edges.

  // A map from EdgeId to the position of that edge in "path".  Only needed if
  // degenerate boundaries are being discarded.
  vector<int> path_index;
  if (degenerate_boundaries == DegenerateBoundaries::DISCARD) {
    path_index.assign(num_edges(), -1);
  }
  for (EdgeId min_start = 0; min_start < num_edges(); ++min_start) {
    if (left_turn_map[min_start] < 0) continue;  // Already used.

    // Build a connected component by keeping a stack of unexplored siblings
    // of the edges used so far.
    DirectedComponent component;
    frontier.push_back(min_start);
    while (!frontier.empty()) {
      EdgeId start = frontier.back();
      frontier.pop_back();
      if (left_turn_map[start] < 0) continue;  // Already used.

      // Build a path by making left turns at each vertex until we return to
      // "start".  Whenever we encounter an edge that is a sibling of an edge
      // that is already on the path, we "peel off" a loop consisting of any
      // edges that were between these two edges.
      vector<EdgeId> path;
      for (EdgeId e = start, next; left_turn_map[e] >= 0; e = next) {
        path.push_back(e);
        next = left_turn_map[e];
        left_turn_map[e] = -1;
        // If the sibling hasn't been visited yet, add it to the frontier.
        EdgeId sibling = sibling_map[e];
        if (left_turn_map[sibling] >= 0) {
          frontier.push_back(sibling);
        }
        if (degenerate_boundaries == DegenerateBoundaries::DISCARD) {
          path_index[e] = path.size() - 1;
          int sibling_index = path_index[sibling];
          if (sibling_index < 0) continue;

          // Common special case: the edge and its sibling are adjacent, in
          // which case we can simply remove them from the path and continue.
          if (sibling_index == path.size() - 2) {
            path.resize(sibling_index);
            // We don't need to update "path_index" for these two edges
            // because both edges of the sibling pair have now been used.
            continue;
          }
          // Peel off a loop from the path.
          vector<EdgeId> loop(path.begin() + sibling_index + 1, path.end() - 1);
          path.erase(path.begin() + sibling_index, path.end());
          // Mark the edges that are no longer part of the path.
          for (EdgeId e2 : loop) path_index[e2] = -1;
          CanonicalizeLoopOrder(min_input_ids, &loop);
          component.push_back(std::move(loop));
        }
      }
      // Mark the edges that are no longer part of the path.
      if (degenerate_boundaries == DegenerateBoundaries::DISCARD) {
        for (EdgeId e2 : path) path_index[e2] = -1;
      }
      CanonicalizeLoopOrder(min_input_ids, &path);
      component.push_back(std::move(path));
    }
    CanonicalizeVectorOrder(min_input_ids, &component);
    components->push_back(std::move(component));
  }
  // Sort the components to correspond to the input edge ordering.
  std::sort(components->begin(), components->end(),
            [&min_input_ids](const DirectedComponent& a,
                             const DirectedComponent& b) {
      return min_input_ids[a[0][0]] < min_input_ids[b[0][0]];
    });
  return true;
}

// Encodes the index of one of the two complements of each component
// (a.k.a. the "slot", either 0 or 1) as a negative EdgeId.
inline static Graph::EdgeId MarkEdgeUsed(int slot) { return -1 - slot; }

bool Graph::GetUndirectedComponents(LoopType loop_type,
                                    vector<UndirectedComponent>* components,
                                    S2Error* error) const {
  S2_DCHECK(options_.degenerate_edges() == DegenerateEdges::DISCARD ||
         options_.degenerate_edges() == DegenerateEdges::DISCARD_EXCESS);
  S2_DCHECK(options_.edge_type() == EdgeType::UNDIRECTED);

  vector<EdgeId> sibling_map = GetInEdgeIds();
  vector<EdgeId> left_turn_map;
  if (!GetLeftTurnMap(sibling_map, &left_turn_map, error)) return false;
  MakeSiblingMap(&sibling_map);
  vector<InputEdgeId> min_input_ids = GetMinInputEdgeIds();

  // A stack of unexplored sibling edges.  Each sibling edge has a "slot"
  // (0 or 1) that indicates which of the two complements it belongs to.
  vector<pair<EdgeId, int>> frontier;

  // If we are breaking loops at repeated vertices, we maintain a map from
  // VertexId to its position in "path".
  vector<int> path_index;
  if (loop_type == LoopType::SIMPLE) path_index.assign(num_vertices(), -1);

  for (EdgeId min_start = 0; min_start < num_edges(); ++min_start) {
    if (left_turn_map[min_start] < 0) continue;  // Already used.

    // Build a connected component by keeping a stack of unexplored siblings
    // of the edges used so far.
    UndirectedComponent component;
    frontier.push_back(make_pair(min_start, 0));
    while (!frontier.empty()) {
      EdgeId start = frontier.back().first;
      int slot = frontier.back().second;
      frontier.pop_back();
      if (left_turn_map[start] < 0) continue;  // Already used.

      // Build a path by making left turns at each vertex until we return to
      // "start".  We use "left_turn_map" to keep track of which edges have
      // already been visited, and which complement they were assigned to, by
      // setting its entries to negative values as we go along.
      vector<EdgeId> path;
      for (EdgeId e = start, next; left_turn_map[e] >= 0; e = next) {
        path.push_back(e);
        next = left_turn_map[e];
        left_turn_map[e] = MarkEdgeUsed(slot);
        // If the sibling hasn't been visited yet, add it to the frontier.
        EdgeId sibling = sibling_map[e];
        if (left_turn_map[sibling] >= 0) {
          frontier.push_back(make_pair(sibling, 1 - slot));
        } else if (left_turn_map[sibling] != MarkEdgeUsed(1 - slot)) {
          // Two siblings edges can only belong the same complement if the
          // given undirected edges do not form loops.
          error->Init(S2Error::BUILDER_EDGES_DO_NOT_FORM_LOOPS,
                      "Given undirected edges do not form loops");
          return false;
        }
        if (loop_type == LoopType::SIMPLE) {
          // Whenever we encounter a vertex that is already part of the path,
          // we "peel off" a loop by removing those edges from the path.
          path_index[edge(e).first] = path.size() - 1;
          int loop_start = path_index[edge(e).second];
          if (loop_start < 0) continue;
          vector<EdgeId> loop(path.begin() + loop_start, path.end());
          path.erase(path.begin() + loop_start, path.end());
          // Mark the vertices that are no longer part of the path.
          for (EdgeId e2 : loop) path_index[edge(e2).first] = -1;
          CanonicalizeLoopOrder(min_input_ids, &loop);
          component[slot].push_back(std::move(loop));
        }
      }
      if (loop_type == LoopType::SIMPLE) {
        S2_DCHECK(path.empty());  // Invariant.
      } else {
        CanonicalizeLoopOrder(min_input_ids, &path);
        component[slot].push_back(std::move(path));
      }
    }
    CanonicalizeVectorOrder(min_input_ids, &component[0]);
    CanonicalizeVectorOrder(min_input_ids, &component[1]);
    // To save some work in S2PolygonLayer, we swap the two loop sets of the
    // component so that the loop set whose first loop most closely follows
    // the input edge ordering is first.  (If the input was a valid S2Polygon,
    // then this component will contain normalized loops.)
    if (min_input_ids[component[0][0][0]] > min_input_ids[component[1][0][0]]) {
      component[0].swap(component[1]);
    }
    components->push_back(std::move(component));
  }
  // Sort the components to correspond to the input edge ordering.
  std::sort(components->begin(), components->end(),
       [&min_input_ids](const UndirectedComponent& a,
                        const UndirectedComponent& b) {
      return min_input_ids[a[0][0][0]] < min_input_ids[b[0][0][0]];
    });
  return true;
}

class Graph::PolylineBuilder {
 public:
  explicit PolylineBuilder(const Graph& g);
  vector<EdgePolyline> BuildPaths();
  vector<EdgePolyline> BuildWalks();

 private:
  bool is_interior(VertexId v);
  int excess_degree(VertexId v);
  EdgePolyline BuildPath(EdgeId e);
  EdgePolyline BuildWalk(VertexId v);
  void MaximizeWalk(EdgePolyline* polyline);

  const Graph& g_;
  Graph::VertexInMap in_;
  Graph::VertexOutMap out_;
  vector<EdgeId> sibling_map_;
  vector<InputEdgeId> min_input_ids_;
  bool directed_;
  int edges_left_;
  vector<bool> used_;
  // A map of (outdegree(v) - indegree(v)) considering used edges only.
  gtl::btree_map<VertexId, int> excess_used_;
};

vector<Graph::EdgePolyline> Graph::GetPolylines(
    PolylineType polyline_type) const {
  S2_DCHECK(options_.sibling_pairs() == SiblingPairs::DISCARD ||
         options_.sibling_pairs() == SiblingPairs::DISCARD_EXCESS ||
         options_.sibling_pairs() == SiblingPairs::KEEP);
  PolylineBuilder builder(*this);
  if (polyline_type == PolylineType::PATH) {
    return builder.BuildPaths();
  } else {
    return builder.BuildWalks();
  }
}

Graph::PolylineBuilder::PolylineBuilder(const Graph& g)
    : g_(g), in_(g), out_(g),
      min_input_ids_(g.GetMinInputEdgeIds()),
      directed_(g_.options().edge_type() == EdgeType::DIRECTED),
      edges_left_(g.num_edges() / (directed_ ? 1 : 2)),
      used_(g.num_edges(), false) {
  if (!directed_) {
    sibling_map_ = in_.in_edge_ids();
    g.MakeSiblingMap(&sibling_map_);
  }
}

inline bool Graph::PolylineBuilder::is_interior(VertexId v) {
  if (directed_) {
    return in_.degree(v) == 1 && out_.degree(v) == 1;
  } else {
    return out_.degree(v) == 2;
  }
}

inline int Graph::PolylineBuilder::excess_degree(VertexId v) {
  return directed_ ? out_.degree(v) - in_.degree(v) : out_.degree(v) % 2;
}

vector<Graph::EdgePolyline> Graph::PolylineBuilder::BuildPaths() {
  // First build polylines starting at all the vertices that cannot be in the
  // polyline interior (i.e., indegree != 1 or outdegree != 1 for directed
  // edges, or degree != 2 for undirected edges).  We consider the possible
  // starting edges in input edge id order so that we preserve the input path
  // direction even when undirected edges are used.  (Undirected edges are
  // represented by sibling pairs where only the edge in the input direction
  // is labeled with an input edge id.)
  vector<EdgePolyline> polylines;
  vector<EdgeId> edges = g_.GetInputEdgeOrder(min_input_ids_);
  for (EdgeId e : edges) {
    if (!used_[e] && !is_interior(g_.edge(e).first)) {
      polylines.push_back(BuildPath(e));
    }
  }
  // If there are any edges left, they form non-intersecting loops.  We build
  // each loop and then canonicalize its edge order.  We consider candidate
  // starting edges in input edge id order in order to preserve the input
  // direction of undirected loops.  Even so, we still need to canonicalize
  // the edge order to ensure that when an input edge is split into an edge
  // chain, the loop does not start in the middle of such a chain.
  for (EdgeId e : edges) {
    if (edges_left_ == 0) break;
    if (used_[e]) continue;
    EdgePolyline polyline = BuildPath(e);
    CanonicalizeLoopOrder(min_input_ids_, &polyline);
    polylines.push_back(std::move(polyline));
  }
  S2_DCHECK_EQ(0, edges_left_);

  // Sort the polylines to correspond to the input order (if possible).
  CanonicalizeVectorOrder(min_input_ids_, &polylines);
  return polylines;
}

Graph::EdgePolyline Graph::PolylineBuilder::BuildPath(EdgeId e) {
  // We simply follow edges until either we reach a vertex where there is a
  // choice about which way to go (where is_interior(v) is false), or we
  // return to the starting vertex (if the polyline is actually a loop).
  EdgePolyline polyline;
  VertexId start = g_.edge(e).first;
  for (;;) {
    polyline.push_back(e);
    S2_DCHECK(!used_[e]);
    used_[e] = true;
    if (!directed_) used_[sibling_map_[e]] = true;
    --edges_left_;
    VertexId v = g_.edge(e).second;
    if (!is_interior(v) || v == start) break;
    if (directed_) {
      S2_DCHECK_EQ(1, out_.degree(v));
      e = *out_.edge_ids(v).begin();
    } else {
      S2_DCHECK_EQ(2, out_.degree(v));
      for (EdgeId e2 : out_.edge_ids(v)) if (!used_[e2]) e = e2;
    }
  }
  return polyline;
}

vector<Graph::EdgePolyline> Graph::PolylineBuilder::BuildWalks() {
  // Note that some of this code is worst-case quadratic in the maximum vertex
  // degree.  This could be fixed with a few extra arrays, but it should not
  // be a problem in practice.

  // First, build polylines from all vertices where outdegree > indegree (or
  // for undirected edges, vertices whose degree is odd).  We consider the
  // possible starting edges in input edge id order, for idempotency in the
  // case where multiple input polylines share vertices or edges.
  vector<EdgePolyline> polylines;
  vector<EdgeId> edges = g_.GetInputEdgeOrder(min_input_ids_);
  for (EdgeId e : edges) {
    if (used_[e]) continue;
    VertexId v = g_.edge(e).first;
    int excess = excess_degree(v);
    if (excess <= 0) continue;
    excess -= excess_used_[v];
    if (directed_ ? (excess <= 0) : (excess % 2 == 0)) continue;
    ++excess_used_[v];
    polylines.push_back(BuildWalk(v));
    --excess_used_[g_.edge(polylines.back().back()).second];
  }
  // Now all vertices have outdegree == indegree (or even degree if undirected
  // edges are being used).  Therefore all remaining edges can be assembled
  // into loops.  We first try to expand the existing polylines if possible by
  // adding loops to them.
  if (edges_left_ > 0) {
    for (EdgePolyline& polyline : polylines) {
      MaximizeWalk(&polyline);
    }
  }
  // Finally, if there are still unused edges then we build loops.  If the
  // input is a polyline that forms a loop, then for idempotency we need to
  // start from the edge with minimum input edge id.  If the minimal input
  // edge was split into several edges, then we start from the first edge of
  // the chain.
  for (int i = 0; i < edges.size() && edges_left_ > 0; ++i) {
    EdgeId e = edges[i];
    if (used_[e]) continue;

    // Determine whether the origin of this edge is the start of an edge
    // chain.  To do this, we test whether (outdegree - indegree == 1) for the
    // origin, considering only unused edges with the same minimum input edge
    // id.  (Undirected edges have input edge ids in one direction only.)
    VertexId v = g_.edge(e).first;
    InputEdgeId id = min_input_ids_[e];
    int excess = 0;
    for (int j = i; j < edges.size() && min_input_ids_[edges[j]] == id; ++j) {
      EdgeId e2 = edges[j];
      if (used_[e2]) continue;
      if (g_.edge(e2).first == v) ++excess;
      if (g_.edge(e2).second == v) --excess;
    }
    // It is also acceptable to start a polyline from any degenerate edge.
    if (excess == 1 || g_.edge(e).second == v) {
      EdgePolyline polyline = BuildWalk(v);
      MaximizeWalk(&polyline);
      polylines.push_back(std::move(polyline));
    }
  }
  S2_DCHECK_EQ(0, edges_left_);

  // Sort the polylines to correspond to the input order (if possible).
  CanonicalizeVectorOrder(min_input_ids_, &polylines);
  return polylines;
}

Graph::EdgePolyline Graph::PolylineBuilder::BuildWalk(VertexId v) {
  EdgePolyline polyline;
  for (;;) {
    // Follow the edge with the smallest input edge id.
    EdgeId best_edge = -1;
    InputEdgeId best_out_id = std::numeric_limits<InputEdgeId>::max();
    for (EdgeId e : out_.edge_ids(v)) {
      if (used_[e] || min_input_ids_[e] >= best_out_id) continue;
      best_out_id = min_input_ids_[e];
      best_edge = e;
    }
    if (best_edge < 0) return polyline;
    // For idempotency when there are multiple input polylines, we stop the
    // walk early if "best_edge" might be a continuation of a different
    // incoming edge.
    int excess = excess_degree(v) - excess_used_[v];
    if (directed_ ? (excess < 0) : (excess % 2) == 1) {
      for (EdgeId e : in_.edge_ids(v)) {
        if (!used_[e] && min_input_ids_[e] <= best_out_id) {
          return polyline;
        }
      }
    }
    polyline.push_back(best_edge);
    used_[best_edge] = true;
    if (!directed_) used_[sibling_map_[best_edge]] = true;
    --edges_left_;
    v = g_.edge(best_edge).second;
  }
}

void Graph::PolylineBuilder::MaximizeWalk(EdgePolyline* polyline) {
  // Examine all vertices of the polyline and check whether there are any
  // unused outgoing edges.  If so, then build a loop starting at that vertex
  // and insert it into the polyline.  (The walk is guaranteed to be a loop
  // because this method is only called when all vertices have equal numbers
  // of unused incoming and outgoing edges.)
  for (int i = 0; i <= polyline->size(); ++i) {
    VertexId v = (i == 0 ? g_.edge((*polyline)[i]).first
                  : g_.edge((*polyline)[i - 1]).second);
    for (EdgeId e : out_.edge_ids(v)) {
      if (!used_[e]) {
        EdgePolyline loop = BuildWalk(v);
        S2_DCHECK_EQ(v, g_.edge(loop.back()).second);
        polyline->insert(polyline->begin() + i, loop.begin(), loop.end());
        S2_DCHECK(used_[e]);  // All outgoing edges from "v" are now used.
        break;
      }
    }
  }
}

class Graph::EdgeProcessor {
 public:
  EdgeProcessor(const GraphOptions& options,
                vector<Edge>* edges,
                vector<InputEdgeIdSetId>* input_ids,
                IdSetLexicon* id_set_lexicon);
  void Run(S2Error* error);

 private:
  void AddEdge(const Edge& edge, InputEdgeIdSetId input_edge_id_set_id);
  void AddEdges(int num_edges, const Edge& edge,
                InputEdgeIdSetId input_edge_id_set_id);
  void CopyEdges(int out_begin, int out_end);
  InputEdgeIdSetId MergeInputIds(int out_begin, int out_end);

  GraphOptions options_;
  vector<Edge>& edges_;
  vector<InputEdgeIdSetId>& input_ids_;
  IdSetLexicon* id_set_lexicon_;
  vector<EdgeId> out_edges_;
  vector<EdgeId> in_edges_;

  vector<Edge> new_edges_;
  vector<InputEdgeIdSetId> new_input_ids_;

  vector<InputEdgeId> tmp_ids_;
};

void Graph::ProcessEdges(
    GraphOptions* options, std::vector<Edge>* edges,
    std::vector<InputEdgeIdSetId>* input_ids, IdSetLexicon* id_set_lexicon,
    S2Error* error) {
  EdgeProcessor processor(*options, edges, input_ids, id_set_lexicon);
  processor.Run(error);
  // Certain values of sibling_pairs() discard half of the edges and change
  // the edge_type() to DIRECTED (see the description of GraphOptions).
  if (options->sibling_pairs() == SiblingPairs::REQUIRE ||
      options->sibling_pairs() == SiblingPairs::CREATE) {
    options->set_edge_type(EdgeType::DIRECTED);
  }
}

Graph::EdgeProcessor::EdgeProcessor(const GraphOptions& options,
                                    vector<Edge>* edges,
                                    vector<InputEdgeIdSetId>* input_ids,
                                    IdSetLexicon* id_set_lexicon)
    : options_(options), edges_(*edges),
      input_ids_(*input_ids), id_set_lexicon_(id_set_lexicon),
      out_edges_(edges_.size()), in_edges_(edges_.size()) {
  // Sort the outgoing and incoming edges in lexigraphic order.  We use a
  // stable sort to ensure that each undirected edge becomes a sibling pair,
  // even if there are multiple identical input edges.
  std::iota(out_edges_.begin(), out_edges_.end(), 0);
  std::sort(out_edges_.begin(), out_edges_.end(), [this](EdgeId a, EdgeId b) {
      return StableLessThan(edges_[a], edges_[b], a, b);
    });
  std::iota(in_edges_.begin(), in_edges_.end(), 0);
  std::sort(in_edges_.begin(), in_edges_.end(), [this](EdgeId a, EdgeId b) {
      return StableLessThan(reverse(edges_[a]), reverse(edges_[b]), a, b);
    });
  new_edges_.reserve(edges_.size());
  new_input_ids_.reserve(edges_.size());
}

inline void Graph::EdgeProcessor::AddEdge(
    const Edge& edge, InputEdgeIdSetId input_edge_id_set_id) {
  new_edges_.push_back(edge);
  new_input_ids_.push_back(input_edge_id_set_id);
}

void Graph::EdgeProcessor::AddEdges(int num_edges, const Edge& edge,
                                    InputEdgeIdSetId input_edge_id_set_id) {
  for (int i = 0; i < num_edges; ++i) {
    AddEdge(edge, input_edge_id_set_id);
  }
}

void Graph::EdgeProcessor::CopyEdges(int out_begin, int out_end) {
  for (int i = out_begin; i < out_end; ++i) {
    AddEdge(edges_[out_edges_[i]], input_ids_[out_edges_[i]]);
  }
}

S2Builder::InputEdgeIdSetId Graph::EdgeProcessor::MergeInputIds(
    int out_begin, int out_end) {
  if (out_end - out_begin == 1) {
    return input_ids_[out_edges_[out_begin]];
  }
  tmp_ids_.clear();
  for (int i = out_begin; i < out_end; ++i) {
    for (auto id : id_set_lexicon_->id_set(input_ids_[out_edges_[i]])) {
      tmp_ids_.push_back(id);
    }
  }
  return id_set_lexicon_->Add(tmp_ids_);
}

void Graph::EdgeProcessor::Run(S2Error* error) {
  int num_edges = edges_.size();
  if (num_edges == 0) return;

  // Walk through the two sorted arrays performing a merge join.  For each
  // edge, gather all the duplicate copies of the edge in both directions
  // (outgoing and incoming).  Then decide what to do based on "options_" and
  // how many copies of the edge there are in each direction.
  int out = 0, in = 0;
  const Edge* out_edge = &edges_[out_edges_[out]];
  const Edge* in_edge = &edges_[in_edges_[in]];
  Edge sentinel(std::numeric_limits<VertexId>::max(),
                std::numeric_limits<VertexId>::max());
  for (;;) {
    Edge edge = min(*out_edge, reverse(*in_edge));
    if (edge == sentinel) break;

    int out_begin = out, in_begin = in;
    while (*out_edge == edge) {
      out_edge = (++out == num_edges) ? &sentinel : &edges_[out_edges_[out]];
    }
    while (reverse(*in_edge) == edge) {
      in_edge = (++in == num_edges) ? &sentinel : &edges_[in_edges_[in]];
    }
    int n_out = out - out_begin;
    int n_in = in - in_begin;
    if (edge.first == edge.second) {
      S2_DCHECK_EQ(n_out, n_in);
      if (options_.degenerate_edges() == DegenerateEdges::DISCARD) {
        continue;
      }
      if (options_.degenerate_edges() == DegenerateEdges::DISCARD_EXCESS &&
          ((out_begin > 0 &&
            edges_[out_edges_[out_begin - 1]].first == edge.first) ||
           (out < num_edges && edges_[out_edges_[out]].first == edge.first) ||
           (in_begin > 0 &&
            edges_[in_edges_[in_begin - 1]].second == edge.first) ||
           (in < num_edges && edges_[in_edges_[in]].second == edge.first))) {
        continue;  // There were non-degenerate incident edges, so discard.
      }
      if (options_.edge_type() == EdgeType::UNDIRECTED &&
          (options_.sibling_pairs() == SiblingPairs::REQUIRE ||
           options_.sibling_pairs() == SiblingPairs::CREATE)) {
        // When we have undirected edges and are guaranteed to have siblings,
        // we cut the number of edges in half (see s2builder.h).
        S2_DCHECK_EQ(0, n_out & 1);  // Number of edges is always even.
        AddEdges(options_.duplicate_edges() == DuplicateEdges::MERGE ?
                 1 : (n_out / 2), edge, MergeInputIds(out_begin, out));
      } else if (options_.duplicate_edges() == DuplicateEdges::MERGE) {
        AddEdges(options_.edge_type() == EdgeType::UNDIRECTED ? 2 : 1,
                 edge, MergeInputIds(out_begin, out));
      } else if (options_.sibling_pairs() == SiblingPairs::DISCARD ||
                 options_.sibling_pairs() == SiblingPairs::DISCARD_EXCESS) {
        // Any SiblingPair option that discards edges causes the labels of all
        // duplicate edges to be merged together (see s2builder.h).
        AddEdges(n_out, edge, MergeInputIds(out_begin, out));
      } else {
        CopyEdges(out_begin, out);
      }
    } else if (options_.sibling_pairs() == SiblingPairs::KEEP) {
      if (n_out > 1 && options_.duplicate_edges() == DuplicateEdges::MERGE) {
        AddEdge(edge, MergeInputIds(out_begin, out));
      } else {
        CopyEdges(out_begin, out);
      }
    } else if (options_.sibling_pairs() == SiblingPairs::DISCARD) {
      if (options_.edge_type() == EdgeType::DIRECTED) {
        // If n_out == n_in: balanced sibling pairs
        // If n_out < n_in:  unbalanced siblings, in the form AB, BA, BA
        // If n_out > n_in:  unbalanced siblings, in the form AB, AB, BA
        if (n_out <= n_in) continue;
        // Any option that discards edges causes the labels of all duplicate
        // edges to be merged together (see s2builder.h).
        AddEdges(options_.duplicate_edges() == DuplicateEdges::MERGE ?
                 1 : (n_out - n_in), edge, MergeInputIds(out_begin, out));
      } else {
        if ((n_out & 1) == 0) continue;
        AddEdge(edge, MergeInputIds(out_begin, out));
      }
    } else if (options_.sibling_pairs() == SiblingPairs::DISCARD_EXCESS) {
      if (options_.edge_type() == EdgeType::DIRECTED) {
        // See comments above.  The only difference is that if there are
        // balanced sibling pairs, we want to keep one such pair.
        if (n_out < n_in) continue;
        AddEdges(options_.duplicate_edges() == DuplicateEdges::MERGE ?
                 1 : max(1, n_out - n_in), edge, MergeInputIds(out_begin, out));
      } else {
        AddEdges((n_out & 1) ? 1 : 2, edge, MergeInputIds(out_begin, out));
      }
    } else {
      S2_DCHECK(options_.sibling_pairs() == SiblingPairs::REQUIRE ||
             options_.sibling_pairs() == SiblingPairs::CREATE);
      if (error->ok() && options_.sibling_pairs() == SiblingPairs::REQUIRE &&
          (options_.edge_type() == EdgeType::DIRECTED ? (n_out != n_in)
                                                      : ((n_out & 1) != 0))) {
        error->Init(S2Error::BUILDER_MISSING_EXPECTED_SIBLING_EDGES,
                    "Expected all input edges to have siblings, "
                    "but some were missing");
      }
      if (options_.duplicate_edges() == DuplicateEdges::MERGE) {
        AddEdge(edge, MergeInputIds(out_begin, out));
      } else if (options_.edge_type() == EdgeType::UNDIRECTED) {
        AddEdges((n_out + 1) / 2, edge, MergeInputIds(out_begin, out));
      } else {
        CopyEdges(out_begin, out);
        if (n_in > n_out) {
          AddEdges(n_in - n_out, edge, MergeInputIds(out, out));
        }
      }
    }
  }
  edges_.swap(new_edges_);
  edges_.shrink_to_fit();
  input_ids_.swap(new_input_ids_);
  input_ids_.shrink_to_fit();
}

vector<S2Point> Graph::FilterVertices(const vector<S2Point>& vertices,
                                      std::vector<Edge>* edges,
                                      vector<VertexId>* tmp) {
  // Gather the vertices that are actually used.
  vector<VertexId> used;
  used.reserve(2 * edges->size());
  for (const Edge& e : *edges) {
    used.push_back(e.first);
    used.push_back(e.second);
  }
  // Sort the vertices and find the distinct ones.
  std::sort(used.begin(), used.end());
  used.erase(std::unique(used.begin(), used.end()), used.end());

  // Build the list of new vertices, and generate a map from old vertex id to
  // new vertex id.
  vector<VertexId>& vmap = *tmp;
  vmap.resize(vertices.size());
  vector<S2Point> new_vertices(used.size());
  for (int i = 0; i < used.size(); ++i) {
    new_vertices[i] = vertices[used[i]];
    vmap[used[i]] = i;
  }
  // Update the edges.
  for (Edge& e : *edges) {
    e.first = vmap[e.first];
    e.second = vmap[e.second];
  }
  return new_vertices;
}
