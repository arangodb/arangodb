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

#include "s2/s2builderutil_testing.h"

#include <string>
#include <vector>
#include <memory>
#include <gtest/gtest.h>
#include "absl/strings/str_format.h"
#include "s2/s2builder.h"
#include "s2/s2text_format.h"

using std::string;
using std::vector;

using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;
using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

namespace s2builderutil {

void GraphClone::Init(const Graph& g) {
  options_ = g.options();
  vertices_ = g.vertices();
  edges_ = g.edges();
  input_edge_id_set_ids_ = g.input_edge_id_set_ids();
  input_edge_id_set_lexicon_ = g.input_edge_id_set_lexicon();
  label_set_ids_ = g.label_set_ids();
  label_set_lexicon_ = g.label_set_lexicon();
  is_full_polygon_predicate_ = g.is_full_polygon_predicate();
  g_ = S2Builder::Graph(
      options_, &vertices_, &edges_, &input_edge_id_set_ids_,
      &input_edge_id_set_lexicon_, &label_set_ids_, &label_set_lexicon_,
      is_full_polygon_predicate_);
}

string IndexMatchingLayer::ToString(const EdgeVector& edges) {
  string msg;
  for (const auto& edge : edges) {
    vector<S2Point> vertices{edge.v0, edge.v1};
    if (!msg.empty()) msg += "; ";
    msg += s2textformat::ToString(vertices);
  }
  return msg;
}

void IndexMatchingLayer::Build(const Graph& g, S2Error* error) {
  vector<S2Shape::Edge> actual, expected;
  for (int e = 0; e < g.num_edges(); ++e) {
    const Graph::Edge& edge = g.edge(e);
    actual.push_back(S2Shape::Edge(g.vertex(edge.first),
                                   g.vertex(edge.second)));
  }
  for (S2Shape* shape : index_) {
    if (shape == nullptr) continue;
    if (dimension_ >= 0 && shape->dimension() != dimension_) continue;
    for (int e = shape->num_edges(); --e >= 0; ) {
      expected.push_back(shape->edge(e));
    }
  }
  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());

  // The edges are a multiset, so we can't use std::set_difference.
  vector<S2Shape::Edge> missing, extra;
  for (auto ai = actual.begin(), ei = expected.begin();
       ai != actual.end() || ei != expected.end(); ) {
    if (ei == expected.end() || (ai != actual.end() && *ai < *ei)) {
      extra.push_back(*ai++);
    } else if (ai == actual.end() || *ei < *ai) {
      missing.push_back(*ei++);
    } else {
      ++ai;
      ++ei;
    }
  }
  if (!missing.empty() || !extra.empty()) {
    // There may be errors in more than one dimension, so we append to the
    // existing error text.
    string label;
    if (dimension_ >= 0) label = absl::StrFormat("Dimension %d: ", dimension_);
    error->Init(S2Error::FAILED_PRECONDITION,
                "%s%sMissing edges: %s Extra edges: %s\n",
                error->text().c_str(), label.c_str(),
                ToString(missing).c_str(), ToString(extra).c_str());
  }
}

}  // namespace s2builderutil
