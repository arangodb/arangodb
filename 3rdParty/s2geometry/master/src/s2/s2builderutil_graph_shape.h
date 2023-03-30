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

#ifndef S2_S2BUILDERUTIL_GRAPH_SHAPE_H_
#define S2_S2BUILDERUTIL_GRAPH_SHAPE_H_

#include <vector>

#include "s2/s2builder_graph.h"

namespace s2builderutil {

// An S2Shape representing the edges in an S2Builder::Graph.
class GraphShape final : public S2Shape {
 public:
  using Graph = S2Builder::Graph;
  explicit GraphShape(const Graph* g) : g_(*g) {}
  int num_edges() const override { return g_.num_edges(); }
  Edge edge(int e) const override {
    Graph::Edge g_edge = g_.edge(e);
    return Edge(g_.vertex(g_edge.first), g_.vertex(g_edge.second));
  }
  int dimension() const override { return 1; }
  ReferencePoint GetReferencePoint() const override {
    return ReferencePoint::Contained(false);
  }
  int num_chains() const override { return g_.num_edges(); }
  Chain chain(int i) const override { return Chain(i, 1); }
  Edge chain_edge(int i, int j) const override {
    S2_DCHECK_EQ(j, 0);
    return edge(i);
  }
  ChainPosition chain_position(int e) const override {
    return ChainPosition(e, 0);
  }

 private:
  const Graph& g_;
};

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_GRAPH_SHAPE_H_
