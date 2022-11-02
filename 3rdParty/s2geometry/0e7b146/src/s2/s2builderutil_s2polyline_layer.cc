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

#include "s2/s2builderutil_s2polyline_layer.h"

#include "s2/s2debug.h"

using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using Label = S2Builder::Label;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using EdgeId = Graph::EdgeId;
using PolylineType = Graph::PolylineType;

namespace s2builderutil {

S2PolylineLayer::S2PolylineLayer(S2Polyline* polyline,
                                 const S2PolylineLayer::Options& options) {
  Init(polyline, nullptr, nullptr, options);
}

S2PolylineLayer::S2PolylineLayer(
    S2Polyline* polyline, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  Init(polyline, label_set_ids, label_set_lexicon, options);
}

void S2PolylineLayer::Init(S2Polyline* polyline, LabelSetIds* label_set_ids,
                           IdSetLexicon* label_set_lexicon,
                           const Options& options) {
  S2_DCHECK_EQ(label_set_ids == nullptr, label_set_lexicon == nullptr);
  polyline_ = polyline;
  label_set_ids_ = label_set_ids;
  label_set_lexicon_ = label_set_lexicon;
  options_ = options;

  if (options_.validate()) {
    polyline_->set_s2debug_override(S2Debug::DISABLE);
  }
}

GraphOptions S2PolylineLayer::graph_options() const {
  // Remove edges that collapse to a single vertex, but keep duplicate and
  // sibling edges, since merging duplicates or discarding siblings can make
  // it impossible to assemble the edges into a single polyline.
  return GraphOptions(options_.edge_type(), DegenerateEdges::DISCARD,
                      DuplicateEdges::KEEP, SiblingPairs::KEEP);
}

void S2PolylineLayer::Build(const Graph& g, S2Error* error) {
  if (g.num_edges() == 0) {
    polyline_->Init(vector<S2Point>{});
    return;
  }
  vector<Graph::EdgePolyline> edge_polylines =
      g.GetPolylines(PolylineType::WALK);
  if (edge_polylines.size() != 1) {
    error->Init(S2Error::BUILDER_EDGES_DO_NOT_FORM_POLYLINE,
                "Input edges cannot be assembled into polyline");
    return;
  }
  const Graph::EdgePolyline& edge_polyline = edge_polylines[0];
  vector<S2Point> vertices;  // Temporary storage for vertices.
  vertices.reserve(edge_polyline.size());
  vertices.push_back(g.vertex(g.edge(edge_polyline[0]).first));
  for (EdgeId e : edge_polyline) {
    vertices.push_back(g.vertex(g.edge(e).second));
  }
  if (label_set_ids_) {
    Graph::LabelFetcher fetcher(g, options_.edge_type());
    vector<Label> labels;  // Temporary storage for labels.
    label_set_ids_->reserve(edge_polyline.size());
    for (EdgeId e : edge_polyline) {
      fetcher.Fetch(e, &labels);
      label_set_ids_->push_back(label_set_lexicon_->Add(labels));
    }
  }
  polyline_->Init(vertices);
  if (options_.validate()) {
    polyline_->FindValidationError(error);
  }
}

}  // namespace s2builderutil
