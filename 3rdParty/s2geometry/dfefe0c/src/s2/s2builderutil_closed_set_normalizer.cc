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

#include "s2/s2builderutil_closed_set_normalizer.h"

#include <memory>

#include "s2/third_party/absl/memory/memory.h"
#include "s2/s2builder_layer.h"

using absl::make_unique;
using std::shared_ptr;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using Edge = Graph::Edge;
using EdgeId = Graph::EdgeId;
using VertexId = Graph::VertexId;

namespace s2builderutil {

ClosedSetNormalizer::ClosedSetNormalizer(
    const Options& options, const vector<GraphOptions>& graph_options_out)
    : options_(options),
      graph_options_out_(graph_options_out),
      graph_options_in_(graph_options_out_),
      sentinel_(std::numeric_limits<VertexId>::max(),
                std::numeric_limits<VertexId>::max()) {
  S2_DCHECK_EQ(graph_options_out_.size(), 3);
  S2_DCHECK(graph_options_out_[0].edge_type() == EdgeType::DIRECTED);
  S2_DCHECK(graph_options_out_[2].edge_type() == EdgeType::DIRECTED);

  // NOTE(ericv): Supporting these options would require some extra code in
  // order to handle undirected edges, and they are not useful for building
  // polylines anyway (they are intended for polygon meshes).
  S2_DCHECK(graph_options_out_[1].sibling_pairs() != SiblingPairs::CREATE);
  S2_DCHECK(graph_options_out_[1].sibling_pairs() != SiblingPairs::REQUIRE);

  // Set the GraphOptions for the input graphs to ensure that (1) they share a
  // common set of vertices, (2) degenerate edges are kept only if they are
  // isolated, and (3) multiple copies of siblings pairs are discarded.  (Note
  // that there may be multiple copies of isolated degenerate edges; clients
  // can eliminate them if desired using DuplicateEdges::MERGE.)
  for (int dim = 0; dim < 3; ++dim) {
    graph_options_in_[dim].set_allow_vertex_filtering(false);
  }
  graph_options_in_[1].set_degenerate_edges(DegenerateEdges::DISCARD_EXCESS);
  graph_options_in_[2].set_degenerate_edges(DegenerateEdges::DISCARD_EXCESS);
  graph_options_in_[2].set_sibling_pairs(SiblingPairs::DISCARD_EXCESS);
}

const vector<Graph>& ClosedSetNormalizer::Run(
    const vector<Graph>& g, S2Error* error) {
  // Ensure that the input graphs were built with our requested options.
  for (int dim = 0; dim < 3; ++dim) {
    S2_DCHECK(g[dim].options() == graph_options_in_[dim]);
  }
  if (options_.suppress_lower_dimensions()) {
    // Build the auxiliary data needed to suppress lower-dimensional edges.
    in_edges2_ = g[2].GetInEdgeIds();
    is_suppressed_.resize(g[0].vertices().size());
    for (int dim = 1; dim <= 2; ++dim) {
      for (int e = 0; e < g[dim].num_edges(); ++e) {
        Edge edge = g[dim].edge(e);
        if (edge.first != edge.second) {
          is_suppressed_[edge.first] = true;
          is_suppressed_[edge.second] = true;
        }
      }
    }
  }

  // Compute the edges that belong in the output graphs.
  NormalizeEdges(g, error);

  // If any edges were added or removed, we need to run Graph::ProcessEdges to
  // ensure that the edges satisfy the requested GraphOptions.  Note that
  // since edges are never added to dimension 2, we can use the edge count to
  // test whether any edges were removed.  If no edges were removed from
  // dimension 2, then no edges were added to dimension 1, and so we can again
  // use the edge count to test whether any edges were removed, etc.
  bool modified[3];
  bool any_modified = false;
  for (int dim = 2; dim >= 0; --dim) {
    if (new_edges_[dim].size() != g[dim].num_edges()) any_modified = true;
    modified[dim] = any_modified;
  }
  if (!any_modified) {
    for (int dim = 0; dim < 3; ++dim) {
      // Copy the graphs to ensure that they have the GraphOptions that were
      // originally requested.
      new_graphs_.push_back(Graph(
          graph_options_out_[dim], &g[dim].vertices(), &g[dim].edges(),
          &g[dim].input_edge_id_set_ids(), &g[dim].input_edge_id_set_lexicon(),
          &g[dim].label_set_ids(), &g[dim].label_set_lexicon(),
          g[dim].is_full_polygon_predicate()));
    }
  } else {
    // Make a copy of input_edge_id_set_lexicon() so that ProcessEdges can
    // merge edges if necessary.
    new_input_edge_id_set_lexicon_ = g[0].input_edge_id_set_lexicon();
    for (int dim = 0; dim < 3; ++dim) {
      if (modified[dim]) {
        Graph::ProcessEdges(&graph_options_out_[dim], &new_edges_[dim],
                            &new_input_edge_ids_[dim],
                            &new_input_edge_id_set_lexicon_, error);
      }
      new_graphs_.push_back(Graph(
          graph_options_out_[dim], &g[dim].vertices(), &new_edges_[dim],
          &new_input_edge_ids_[dim], &new_input_edge_id_set_lexicon_,
          &g[dim].label_set_ids(), &g[dim].label_set_lexicon(),
          g[dim].is_full_polygon_predicate()));
    }
  }
  return new_graphs_;
}

// Helper function that advances to the next edge in the given graph,
// returning a sentinel value once all edges are exhausted.
inline Edge ClosedSetNormalizer::Advance(const Graph& g, EdgeId* e) const {
  return (++*e == g.num_edges()) ? sentinel_ : g.edge(*e);
}

// Helper function that advances to the next incoming edge in the given graph,
// returning a sentinel value once all edges are exhausted.
inline Edge ClosedSetNormalizer::AdvanceIncoming(
    const Graph& g, const vector<EdgeId>& in_edges, int* i) const {
  return ((++*i == in_edges.size()) ? sentinel_ :
          Graph::reverse(g.edge(in_edges[*i])));
  }

void ClosedSetNormalizer::NormalizeEdges(const vector<Graph>& g,
                                         S2Error* error) {
  // Find the degenerate polygon edges and sibling pairs, and classify each
  // edge as belonging to either a shell or a hole.
  auto degeneracies = FindPolygonDegeneracies(g[2], error);
  auto degeneracy = degeneracies.begin();

  // Walk through the three edge vectors performing a merge join.  We also
  // maintain positions in two other auxiliary vectors: the vector of sorted
  // polygon degeneracies (degeneracies), and the vector of incoming polygon
  // edges (if we are suppressing lower-dimensional duplicate edges).
  EdgeId e0 = -1, e1 = -1, e2 = -1;  // Current position in g[dim].edges()
  int in_e2 = -1;  // Current position in in_edges2_
  Edge edge0 = Advance(g[0], &e0);
  Edge edge1 = Advance(g[1], &e1);
  Edge edge2 = Advance(g[2], &e2);
  Edge in_edge2 = AdvanceIncoming(g[2], in_edges2_, &in_e2);
  for (;;) {
    if (edge2 <= edge1 && edge2 <= edge0) {
      if (edge2 == sentinel_) break;
      if (degeneracy == degeneracies.end() || degeneracy->edge_id != e2) {
        // Normal polygon edge (not part of a degeneracy).
        AddEdge(2, g[2], e2);
        while (options_.suppress_lower_dimensions() && edge1 == edge2) {
          edge1 = Advance(g[1], &e1);
        }
      } else if (!(degeneracy++)->is_hole) {
        // Edge belongs to a degenerate shell.
        if (edge2.first != edge2.second) {
          AddEdge(1, g[2], e2);
          // Since this edge was demoted, make sure that it does not suppress
          // any coincident polyline edge(s).
          while (edge1 == edge2) {
            AddEdge(1, g[1], e1);
            edge1 = Advance(g[1], &e1);
          }
        } else {
          // The test below is necessary because a single-vertex polygon shell
          // can be discarded by a polyline edge incident to that vertex.
          if (!is_suppressed(edge2.first)) AddEdge(0, g[2], e2);
        }
      }
      edge2 = Advance(g[2], &e2);
    } else if (edge1 <= edge0) {
      if (edge1.first != edge1.second) {
        // Non-degenerate polyline edge.  (Note that in_edges2_ is empty
        // whenever "suppress_lower_dimensions" is false.)
        while (in_edge2 < edge1) {
          in_edge2 = AdvanceIncoming(g[2], in_edges2_, &in_e2);
        }
        if (edge1 != in_edge2) AddEdge(1, g[1], e1);
      } else {
        // Degenerate polyline edge.
        if (!is_suppressed(edge1.first)) AddEdge(0, g[1], e1);
        if (g[1].options().edge_type() == EdgeType::UNDIRECTED) ++e1;
      }
      edge1 = Advance(g[1], &e1);
    } else {
      // Input point.
      if (!is_suppressed(edge0.first)) AddEdge(0, g[0], e0);
      edge0 = Advance(g[0], &e0);
    }
  }
}

inline void ClosedSetNormalizer::AddEdge(int new_dim, const Graph& g,
                                         EdgeId e) {
  new_edges_[new_dim].push_back(g.edge(e));
  new_input_edge_ids_[new_dim].push_back(g.input_edge_id_set_id(e));
}

inline bool ClosedSetNormalizer::is_suppressed(VertexId v) const {
  return options_.suppress_lower_dimensions() && is_suppressed_[v];
}

// This method implements the NormalizeClosedSet function.  The Create()
// method allocates a single object of this class whose ownership is shared
// (using shared_ptr) among the three returned S2Builder::Layers.  Here is how
// the process works:
//
//  - The returned layers are passed to a class (such as S2Builder or
//    S2BooleanOperation) that calls their Build methods.  We call these the
//    "input layers" because they provide the input to ClosedSetNormalizer.
//
//  - When Build() is called on the first two layers, pointers to the
//    corresponding Graph arguments are saved.
//
//  - When Build() is called on the third layer, ClosedSetNormalizer is used
//    to normalize the graphs, and then the Build() method of each of the
//    three output layers is called.
//
// TODO(ericv): Consider generalizing this technique as a public class.
class NormalizeClosedSetImpl {
 public:
  static LayerVector Create(LayerVector output_layers,
                            const ClosedSetNormalizer::Options& options) {
    using Impl = NormalizeClosedSetImpl;
    shared_ptr<Impl> impl(new Impl(std::move(output_layers), options));
    LayerVector result;
    for (int dim = 0; dim < 3; ++dim) {
      result.push_back(make_unique<DimensionLayer>(
          dim, impl->normalizer_.graph_options()[dim], impl));
    }
    return result;
  }

 private:
  NormalizeClosedSetImpl(LayerVector output_layers,
                         const ClosedSetNormalizer::Options& options)
      : output_layers_(std::move(output_layers)),
        normalizer_(options, vector<GraphOptions>{
            output_layers_[0]->graph_options(),
            output_layers_[1]->graph_options(),
            output_layers_[2]->graph_options()}),
        graphs_(3), graphs_left_(3) {
    S2_DCHECK_EQ(3, output_layers_.size());
  }

  class DimensionLayer : public S2Builder::Layer {
   public:
    DimensionLayer(int dimension, const GraphOptions& graph_options,
                   shared_ptr<NormalizeClosedSetImpl> impl)
        : dimension_(dimension), graph_options_(graph_options),
          impl_(std::move(impl)) {}

    GraphOptions graph_options() const override { return graph_options_; }

    void Build(const Graph& g, S2Error* error) override {
      impl_->Build(dimension_, g, error);
    }

   private:
    int dimension_;
    GraphOptions graph_options_;
    shared_ptr<NormalizeClosedSetImpl> impl_;
  };

  void Build(int dimension, const Graph& g, S2Error* error) {
    // Errors are reported only on the last layer built.
    graphs_[dimension] = g;
    if (--graphs_left_ > 0) return;

    vector<Graph> output = normalizer_.Run(graphs_, error);
    for (int dim = 0; dim < 3; ++dim) {
      output_layers_[dim]->Build(output[dim], error);
    }
  }

 private:
  vector<unique_ptr<S2Builder::Layer>> output_layers_;
  ClosedSetNormalizer normalizer_;
  vector<Graph> graphs_;
  int graphs_left_;
};

LayerVector NormalizeClosedSet(LayerVector output_layers,
                               const ClosedSetNormalizer::Options& options) {
  return NormalizeClosedSetImpl::Create(std::move(output_layers), options);
}

}  // namespace s2builderutil
