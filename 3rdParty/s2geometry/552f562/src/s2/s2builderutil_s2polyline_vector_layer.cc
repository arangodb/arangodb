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

#include "s2/s2builderutil_s2polyline_vector_layer.h"

#include <memory>

using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using Label = S2Builder::Label;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using EdgeId = Graph::EdgeId;

namespace s2builderutil {

S2PolylineVectorLayer::S2PolylineVectorLayer(
    vector<unique_ptr<S2Polyline>>* polylines,
    const S2PolylineVectorLayer::Options& options) {
  Init(polylines, nullptr, nullptr, options);
}

S2PolylineVectorLayer::S2PolylineVectorLayer(
    vector<unique_ptr<S2Polyline>>* polylines, LabelSetIds* label_set_ids,
    IdSetLexicon* label_set_lexicon, const Options& options) {
  Init(polylines, label_set_ids, label_set_lexicon, options);
}

void S2PolylineVectorLayer::Init(vector<unique_ptr<S2Polyline>>* polylines,
                                 LabelSetIds* label_set_ids,
                                 IdSetLexicon* label_set_lexicon,
                                 const Options& options) {
  S2_DCHECK_EQ(label_set_ids == nullptr, label_set_lexicon == nullptr);
  polylines_ = polylines;
  label_set_ids_ = label_set_ids;
  label_set_lexicon_ = label_set_lexicon;
  options_ = options;
}

GraphOptions S2PolylineVectorLayer::graph_options() const {
  return GraphOptions(options_.edge_type(), DegenerateEdges::DISCARD,
                      options_.duplicate_edges(), options_.sibling_pairs());
}

void S2PolylineVectorLayer::Build(const Graph& g, S2Error* error) {
  vector<Graph::EdgePolyline> edge_polylines = g.GetPolylines(
      options_.polyline_type());
  polylines_->reserve(edge_polylines.size());
  if (label_set_ids_) label_set_ids_->reserve(edge_polylines.size());
  vector<S2Point> vertices;  // Temporary storage for vertices.
  vector<Label> labels;  // Temporary storage for labels.
  for (const auto& edge_polyline : edge_polylines) {
    vertices.push_back(g.vertex(g.edge(edge_polyline[0]).first));
    for (EdgeId e : edge_polyline) {
      vertices.push_back(g.vertex(g.edge(e).second));
    }
    S2Polyline* polyline = new S2Polyline(vertices,
                                          options_.s2debug_override());
    vertices.clear();
    if (options_.validate()) {
      polyline->FindValidationError(error);
    }
    polylines_->emplace_back(polyline);
    if (label_set_ids_) {
      Graph::LabelFetcher fetcher(g, options_.edge_type());
      vector<LabelSetId> polyline_labels;
      polyline_labels.reserve(edge_polyline.size());
      for (EdgeId e : edge_polyline) {
        fetcher.Fetch(e, &labels);
        polyline_labels.push_back(label_set_lexicon_->Add(labels));
      }
      label_set_ids_->push_back(std::move(polyline_labels));
    }
  }
}

}  // namespace s2builderutil
