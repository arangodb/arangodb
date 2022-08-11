// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "s2/s2builderutil_lax_polygon_layer.h"

#include <algorithm>
#include <memory>
#include "absl/memory/memory.h"
#include "s2/s2builderutil_find_polygon_degeneracies.h"
#include "s2/s2debug.h"

using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using Label = S2Builder::Label;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using Edge = Graph::Edge;
using EdgeId = Graph::EdgeId;
using InputEdgeIdSetId = Graph::InputEdgeIdSetId;
using LoopType = Graph::LoopType;

namespace s2builderutil {

using DegenerateBoundaries = LaxPolygonLayer::Options::DegenerateBoundaries;

LaxPolygonLayer::LaxPolygonLayer(S2LaxPolygonShape* polygon,
                                 const Options& options) {
  Init(polygon, nullptr, nullptr, options);
}

LaxPolygonLayer::LaxPolygonLayer(
    S2LaxPolygonShape* polygon, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  Init(polygon, label_set_ids, label_set_lexicon, options);
}

void LaxPolygonLayer::Init(
    S2LaxPolygonShape* polygon, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  S2_DCHECK_EQ(label_set_ids == nullptr, label_set_lexicon == nullptr);
  polygon_ = polygon;
  label_set_ids_ = label_set_ids;
  label_set_lexicon_ = label_set_lexicon;
  options_ = options;
}

GraphOptions LaxPolygonLayer::graph_options() const {
  if (options_.degenerate_boundaries() == DegenerateBoundaries::DISCARD) {
    // There should not be any duplicate edges, but if there are then we keep
    // them since this yields more comprehensible error messages.
    return GraphOptions(options_.edge_type(), DegenerateEdges::DISCARD,
                        DuplicateEdges::KEEP, SiblingPairs::DISCARD);
  } else {
    // Keep at most one copy of each sibling pair and each isolated vertex.
    return GraphOptions(options_.edge_type(), DegenerateEdges::DISCARD_EXCESS,
                        DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  }
}

void LaxPolygonLayer::AppendPolygonLoops(
    const Graph& g, const vector<Graph::EdgeLoop>& edge_loops,
    vector<vector<S2Point>>* loops) const {
  for (const auto& edge_loop : edge_loops) {
    vector<S2Point> vertices;
    vertices.reserve(edge_loop.size());
    for (auto edge_id : edge_loop) {
      vertices.push_back(g.vertex(g.edge(edge_id).first));
    }
    loops->push_back(std::move(vertices));
  }
}

void LaxPolygonLayer::AppendEdgeLabels(
    const Graph& g,
    const vector<Graph::EdgeLoop>& edge_loops) {
  if (!label_set_ids_) return;

  vector<Label> labels;  // Temporary storage for labels.
  Graph::LabelFetcher fetcher(g, options_.edge_type());
  for (const auto& edge_loop : edge_loops) {
    vector<LabelSetId> loop_label_set_ids;
    loop_label_set_ids.reserve(edge_loop.size());
    for (auto edge_id : edge_loop) {
      fetcher.Fetch(edge_id, &labels);
      loop_label_set_ids.push_back(label_set_lexicon_->Add(labels));
    }
    label_set_ids_->push_back(std::move(loop_label_set_ids));
  }
}

// Returns all edges of "g" except for those identified by "edges_to_discard".
static void DiscardEdges(const Graph& g, const vector<EdgeId>& edges_to_discard,
                         vector<Edge>* new_edges,
                         vector<InputEdgeIdSetId>* new_input_edge_id_set_ids) {
  S2_DCHECK(std::is_sorted(edges_to_discard.begin(), edges_to_discard.end()));
  new_edges->clear();
  new_input_edge_id_set_ids->clear();
  new_edges->reserve(g.num_edges());
  new_input_edge_id_set_ids->reserve(g.num_edges());
  auto it = edges_to_discard.begin();
  for (int e = 0; e < g.num_edges(); ++e) {
    if (it != edges_to_discard.end() && e == *it) {
      ++it;
    } else {
      new_edges->push_back(g.edge(e));
      new_input_edge_id_set_ids->push_back(g.input_edge_id_set_id(e));
    }
  }
  S2_DCHECK(it == edges_to_discard.end());
}

static void MaybeAddFullLoop(
    const Graph& g, vector<vector<S2Point>>* loops, S2Error* error) {
  if (g.IsFullPolygon(error)) {
    loops->push_back({});  // Full loop.
  }
}
void LaxPolygonLayer::BuildDirected(Graph g, S2Error* error) {
  // Some cases are implemented by constructing a new graph with certain
  // degenerate edges removed (overwriting "g").  "new_edges" is where the
  // edges for the new graph are stored.
  vector<Edge> new_edges;
  vector<InputEdgeIdSetId> new_input_edge_id_set_ids;
  vector<vector<S2Point>> loops;
  auto degenerate_boundaries = options_.degenerate_boundaries();
  if (degenerate_boundaries == DegenerateBoundaries::DISCARD) {
    // This is the easiest case, since there are no degeneracies.
    if (g.num_edges() == 0) MaybeAddFullLoop(g, &loops, error);
  } else if (degenerate_boundaries == DegenerateBoundaries::KEEP) {
    // S2LaxPolygonShape doesn't need to distinguish degenerate shells from
    // holes except when the entire graph is degenerate, in which case we need
    // to decide whether it represents an empty polygons possibly with
    // degenerate shells, or a full polygon possibly with degenerate holes.
    if (s2builderutil::IsFullyDegenerate(g)) {
      MaybeAddFullLoop(g, &loops, error);
    }
  } else {
    // For DISCARD_SHELLS and DISCARD_HOLES we first determine whether any
    // degenerate loops of the given type exist, and if so we construct a new
    // graph with those edges removed (overwriting "g").
    bool discard_holes =
        (degenerate_boundaries == DegenerateBoundaries::DISCARD_HOLES);
    auto degeneracies = s2builderutil::FindPolygonDegeneracies(g, error);
    if (!error->ok()) return;
    if (degeneracies.size() == g.num_edges()) {
      if (degeneracies.empty()) {
        MaybeAddFullLoop(g, &loops, error);
      } else if (degeneracies[0].is_hole) {
        loops.push_back({});  // Full loop.
      }
    }
    vector<EdgeId> edges_to_discard;
    for (auto degeneracy : degeneracies) {
      if (degeneracy.is_hole == discard_holes) {
        edges_to_discard.push_back(degeneracy.edge_id);
      }
    }
    if (!edges_to_discard.empty()) {
      // Construct a new graph that discards the unwanted edges.
      std::sort(edges_to_discard.begin(), edges_to_discard.end());
      DiscardEdges(g, edges_to_discard, &new_edges, &new_input_edge_id_set_ids);
      g = Graph(g.options(), &g.vertices(),
                &new_edges, &new_input_edge_id_set_ids,
                &g.input_edge_id_set_lexicon(), &g.label_set_ids(),
                &g.label_set_lexicon(), g.is_full_polygon_predicate());
    }
  }
  vector<Graph::EdgeLoop> edge_loops;
  if (!g.GetDirectedLoops(LoopType::CIRCUIT, &edge_loops, error)) {
    return;
  }
  AppendPolygonLoops(g, edge_loops, &loops);
  AppendEdgeLabels(g, edge_loops);
  vector<Graph::EdgeLoop>().swap(edge_loops);  // Release memory
  vector<Edge>().swap(new_edges);
  vector<InputEdgeIdSetId>().swap(new_input_edge_id_set_ids);
  polygon_->Init(loops);
}

void LaxPolygonLayer::Build(const Graph& g, S2Error* error) {
  if (label_set_ids_) label_set_ids_->clear();
  if (g.options().edge_type() == EdgeType::DIRECTED) {
    BuildDirected(g, error);
  } else {
    error->Init(S2Error::UNIMPLEMENTED, "Undirected edges not supported yet");
  }
}

}  // namespace s2builderutil
