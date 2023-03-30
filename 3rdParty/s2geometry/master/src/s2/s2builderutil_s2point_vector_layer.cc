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

#include "s2/s2builderutil_s2point_vector_layer.h"

#include "s2/s2builder_graph.h"

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

S2PointVectorLayer::S2PointVectorLayer(vector<S2Point>* points,
                                       const Options& options)
    : S2PointVectorLayer(points, nullptr, nullptr, options) {}

S2PointVectorLayer::S2PointVectorLayer(vector<S2Point>* points,
                                       LabelSetIds* label_set_ids,
                                       IdSetLexicon* label_set_lexicon,
                                       const Options& options)
    : points_(points),
      label_set_ids_(label_set_ids),
      label_set_lexicon_(label_set_lexicon),
      options_(options) {}

void S2PointVectorLayer::Build(const Graph& g, S2Error* error) {
  Graph::LabelFetcher fetcher(g, EdgeType::DIRECTED);

  vector<Label> labels;  // Temporary storage for labels.
  for (EdgeId edge_id = 0; edge_id < g.edges().size(); edge_id++) {
    auto& edge = g.edge(edge_id);
    if (edge.first != edge.second) {
      error->Init(S2Error::INVALID_ARGUMENT, "Found non-degenerate edges");
      continue;
    }
    points_->push_back(g.vertex(edge.first));
    if (label_set_ids_) {
      fetcher.Fetch(edge_id, &labels);
      int set_id = label_set_lexicon_->Add(labels);
      label_set_ids_->push_back(set_id);
    }
  }
}

GraphOptions S2PointVectorLayer::graph_options() const {
  return GraphOptions(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                      options_.duplicate_edges(), SiblingPairs::KEEP);
}

}  // namespace s2builderutil
