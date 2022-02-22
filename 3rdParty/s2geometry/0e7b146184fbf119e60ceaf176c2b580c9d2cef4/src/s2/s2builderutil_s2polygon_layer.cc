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
#include <memory>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2debug.h"

using absl::make_unique;
using std::make_pair;
using std::pair;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using Label = S2Builder::Label;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using LoopType = Graph::LoopType;

namespace s2builderutil {

S2PolygonLayer::S2PolygonLayer(S2Polygon* polygon, const Options& options) {
  Init(polygon, nullptr, nullptr, options);
}

S2PolygonLayer::S2PolygonLayer(
    S2Polygon* polygon, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  Init(polygon, label_set_ids, label_set_lexicon, options);
}

void S2PolygonLayer::Init(
    S2Polygon* polygon, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  S2_DCHECK_EQ(label_set_ids == nullptr, label_set_lexicon == nullptr);
  polygon_ = polygon;
  label_set_ids_ = label_set_ids;
  label_set_lexicon_ = label_set_lexicon;
  options_ = options;

  if (options_.validate()) {
    polygon_->set_s2debug_override(S2Debug::DISABLE);
  }
}

GraphOptions S2PolygonLayer::graph_options() const {
  // Prevent degenerate edges and sibling edge pairs.  There should not be any
  // duplicate edges if the input is valid, but if there are then we keep them
  // since this tends to produce more comprehensible errors.
  return GraphOptions(options_.edge_type(), DegenerateEdges::DISCARD,
                      DuplicateEdges::KEEP, SiblingPairs::DISCARD);
}

void S2PolygonLayer::AppendS2Loops(const Graph& g,
                                   const vector<Graph::EdgeLoop>& edge_loops,
                                   vector<unique_ptr<S2Loop>>* loops) const {
  vector<S2Point> vertices;
  for (const auto& edge_loop : edge_loops) {
    vertices.reserve(edge_loop.size());
    for (auto edge_id : edge_loop) {
      vertices.push_back(g.vertex(g.edge(edge_id).first));
    }
    loops->push_back(
        make_unique<S2Loop>(vertices, polygon_->s2debug_override()));
    vertices.clear();
  }
}

void S2PolygonLayer::AppendEdgeLabels(
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

void S2PolygonLayer::InitLoopMap(const vector<unique_ptr<S2Loop>>& loops,
                                 LoopMap* loop_map) const {
  if (!label_set_ids_) return;
  for (const auto& loop : loops) {
    (*loop_map)[&*loop] =
        make_pair(&loop - &loops[0], loop->contains_origin());
  }
}

void S2PolygonLayer::ReorderEdgeLabels(const LoopMap& loop_map) {
  if (!label_set_ids_) return;
  LabelSetIds new_ids(label_set_ids_->size());
  for (int i = 0; i < polygon_->num_loops(); ++i) {
    S2Loop* loop = polygon_->loop(i);
    const pair<int, bool>& old = loop_map.find(loop)->second;
    new_ids[i].swap((*label_set_ids_)[old.first]);
    if (loop->contains_origin() != old.second) {
      // S2Loop::Invert() reverses the order of the vertices, which leaves
      // the last edge unchanged.  For example, the loop ABCD (with edges
      // AB, BC, CD, DA) becomes the loop DCBA (with edges DC, CB, BA, AD).
      std::reverse(new_ids[i].begin(), new_ids[i].end() - 1);
    }
  }
  label_set_ids_->swap(new_ids);
}

void S2PolygonLayer::Build(const Graph& g, S2Error* error) {
  if (label_set_ids_) label_set_ids_->clear();

  // It's tricky to compute the edge labels for S2Polygons because the
  // S2Polygon::Init methods can reorder and/or invert the loops.  We handle
  // this by remembering the original vector index of each loop and whether or
  // not the loop contained S2::Origin().  By comparing this with the final
  // S2Polygon loops we can fix up the edge labels appropriately.
  LoopMap loop_map;
  if (g.num_edges() == 0) {
    // The polygon is either full or empty.
    if (g.IsFullPolygon(error)) {
      polygon_->Init(make_unique<S2Loop>(S2Loop::kFull()));
    } else {
      polygon_->InitNested(vector<unique_ptr<S2Loop>>{});
    }
  } else if (g.options().edge_type() == EdgeType::DIRECTED) {
    vector<Graph::EdgeLoop> edge_loops;
    if (!g.GetDirectedLoops(LoopType::SIMPLE, &edge_loops, error)) {
      return;
    }
    vector<unique_ptr<S2Loop>> loops;
    AppendS2Loops(g, edge_loops, &loops);
    AppendEdgeLabels(g, edge_loops);
    vector<Graph::EdgeLoop>().swap(edge_loops);  // Release memory
    InitLoopMap(loops, &loop_map);
    polygon_->InitOriented(std::move(loops));
  } else {
    vector<Graph::UndirectedComponent> components;
    if (!g.GetUndirectedComponents(LoopType::SIMPLE, &components, error)) {
      return;
    }
    // It doesn't really matter which complement of each component we use,
    // since below we normalize all the loops so that they enclose at most
    // half of the sphere (to ensure that the loops can always be nested).
    //
    // The only reason to prefer one over the other is that when there are
    // multiple loops that touch, only one of the two complements matches the
    // structure of the input loops.  GetUndirectedComponents() tries to
    // ensure that this is always complement 0 of each component.
    vector<unique_ptr<S2Loop>> loops;
    for (const auto& component : components) {
      AppendS2Loops(g, component[0], &loops);
      AppendEdgeLabels(g, component[0]);
    }
    vector<Graph::UndirectedComponent>().swap(components);  // Release memory
    InitLoopMap(loops, &loop_map);
    for (const auto& loop : loops) loop->Normalize();
    polygon_->InitNested(std::move(loops));
  }
  ReorderEdgeLabels(loop_map);
  if (options_.validate()) {
    polygon_->FindValidationError(error);
  }
}

}  // namespace s2builderutil
