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

#ifndef S2_S2BUILDERUTIL_CLOSED_SET_NORMALIZER_H_
#define S2_S2BUILDERUTIL_CLOSED_SET_NORMALIZER_H_

#include <vector>
#include "s2/id_set_lexicon.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builderutil_find_polygon_degeneracies.h"

namespace s2builderutil {

// The purpose of this class is to allow S2Builder::Layer implementations to
// remove polygon and polyline degeneracies by converting them to polylines or
// points.  Note that most clients should not use ClosedSetNormalizer itself,
// but should instead call NormalizeClosedSet defined below.
//
// A polyline degeneracy is a polyline consisting of a single degenerate edge.
// A polygon degeneracy is either a single-vertex loop (a degenerate edge from
// a vertex to itself) or a sibling edge pair (consisting of an edge and its
// corresponding reverse edge).  Polygon degeneracies are further classified
// as shells or holes depending on whether they are located in the exterior or
// interior of the polygon respectively.  For example, a single-vertex loop
// contained within a polygon shell would be classified as a hole.
//
// All objects are modeled as closed, i.e. polygons contain their boundaries
// and polylines contain their endpoints.  Note that under this model,
// degenerate polygon shells and holes need to be handled differently.
// Degenerate shells are converted to polylines or points, whereas degenerate
// holes do not affect the set of points contained by the polygon and are
// simply discarded.
//
// Specifically, given three S2Builder::Graphs (corresponding to points,
// polylines, and polygons), this class makes the following transformations:
//
//  - Polygon sibling edge pairs are either discarded (for holes) or converted
//    to a pair of polyline edges (for shells).
//  - Degenerate polygon edges are either discarded (for holes) or converted
//    to points (for shells).
//  - Degenerate polyline edges are converted to points.
//
// Optionally, this class further normalize the graphs by suppressing edges
// that are duplicates of higher-dimensional edges.  In other words:
//
//  - Polyline edges that coincide with polygon edges are discarded.
//  - Points that coincide with polyline or polygon vertices are discarded.
//
// (When edges are discarded, any labels attached to those edges are discarded
// as well.)
//
// This class takes three graphs as input and yields three graphs as output.
// However note that the output graphs are *not* independent objects; they may
// point to data in the input graphs or data owned by the ClosedSetNormalizer
// itself.  For this reason the input graphs and ClosedSetNormalizer must
// persist until the output graphs are no longer needed.
//
// Finally, note that although this class may be necessary in some situations
// (e.g., to implement the OGC Simple Features Access spec), in general the
// recommended approach to degeneracies is simply to keep them (by using a
// representation such as S2LaxPolygonShape or S2LaxPolylineShape).
// Keeping degeneracies has many advantages, such as not needing to deal with
// geometry of multiple dimensions, and being able to preserve polygon
// boundaries accurately (including degenerate holes).
class ClosedSetNormalizer {
 public:
  class Options {
   public:
    Options();

    // If "suppress_lower_dimensions" is true, then the graphs are further
    // normalized by discarding lower-dimensional edges that coincide with
    // higher-dimensional edges.
    //
    // DEFAULT: true
    bool suppress_lower_dimensions() const;
    void set_suppress_lower_dimensions(bool suppress_lower_dimensions);

   private:
    bool suppress_lower_dimensions_;
  };

  // Constructs a ClosedSetNormalizer whose output will be three
  // S2Builder::Graphs with the given "graph_options_out".
  //
  // REQUIRES: graph_options_out.size() == 3
  // REQUIRES: graph_options_out[0].edge_type() == DIRECTED
  // REQUIRES: graph_options_out[1].sibling_pairs() != {CREATE, REQUIRE}
  // REQUIRES: graph_options_out[2].edge_type() == DIRECTED
  ClosedSetNormalizer(
      const Options& options,
      const std::vector<S2Builder::GraphOptions>& graph_options_out);

  // Returns the ClosedSetNormalizer options.
  const Options& options() const { return options_; }

  // Returns the GraphOptions that should be used to construct the input
  // S2Builder::Graph of each dimension.
  inline const std::vector<S2Builder::GraphOptions>& graph_options() const;

  // Normalizes the input graphs and returns a new set of graphs where
  // degeneracies have been discarded or converted to objects of lower
  // dimension.  input[d] is the graph representing edges of dimension "d".
  //
  // Note that the input graphs, their contents, and the ClosedSetNormalizer
  // itself must persist until the output of this class is no longer needed.
  // (To emphasize this requirement, a const reference is returned.)
  const std::vector<S2Builder::Graph>& Run(
      const std::vector<S2Builder::Graph>& input, S2Error* error);

 private:
  S2Builder::Graph::Edge Advance(
      const S2Builder::Graph& g, S2Builder::Graph::EdgeId* id) const;
  S2Builder::Graph::Edge AdvanceIncoming(
      const S2Builder::Graph& g,
      const std::vector<S2Builder::Graph::EdgeId>& in_edges, int* i) const;
  void NormalizeEdges(const std::vector<S2Builder::Graph>& g, S2Error* error);
  void AddEdge(int new_dim, const S2Builder::Graph& g,
               S2Builder::Graph::EdgeId e);
  bool is_suppressed(S2Builder::Graph::VertexId v) const;

  Options options_;

  // Requested options for the output graphs.
  std::vector<S2Builder::GraphOptions> graph_options_out_;

  // Options to be used to construct the input graphs.
  std::vector<S2Builder::GraphOptions> graph_options_in_;

  // A sentinel value that compares larger than any valid edge.
  const S2Builder::Graph::Edge sentinel_;

  // is_suppressed_[i] is true if vertex[i] belongs to a non-degenerate edge,
  // and therefore should be suppressed from the output graph for points.
  std::vector<bool> is_suppressed_;

  // A vector of incoming polygon edges sorted in lexicographic order.  This
  // is used to suppress directed polyline edges that match a polygon edge in
  // the reverse direction.
  std::vector<S2Builder::Graph::EdgeId> in_edges2_;

  // Output data.
  std::vector<S2Builder::Graph> new_graphs_;
  std::vector<S2Builder::Graph::Edge> new_edges_[3];
  std::vector<S2Builder::Graph::InputEdgeIdSetId> new_input_edge_ids_[3];
  IdSetLexicon new_input_edge_id_set_lexicon_;
};

// A LayerVector represents a set of layers that comprise a single object.
// Such objects are typically assembled by gathering the S2Builder::Graphs
// from all of the individual layers and processing them all at once.
using LayerVector = std::vector<std::unique_ptr<S2Builder::Layer>>;

// Given a set of three output layers (one each for dimensions 0, 1, and 2),
// returns a new set of layers that preprocess the input graphs using a
// ClosedSetNormalizer with the given options.  This can be used to ensure
// that the graphs passed to "output_layers" do not contain any polyline or
// polygon degeneracies.
//
// Example showing how to compute the union of two S2ShapeIndexes containing
// points, polylines, and/or polygons, and save the result as a collection of
// S2Points, S2Polylines, and S2Polygons in another S2ShapeIndex (where
// degeneracies have been normalized to objects of lower dimension, and
// maximal polylines are constructed from undirected edges):
//
// bool ComputeUnion(const S2ShapeIndex& a, const S2ShapeIndex& b,
//                   MutableS2ShapeIndex* index, S2Error* error) {
//   IndexedS2PolylineVectorLayer::Options polyline_options;
//   polyline_options.set_edge_type(EdgeType::UNDIRECTED);
//   polyline_options.set_polyline_type(Graph::PolylineType::WALK);
//   polyline_options.set_duplicate_edges(DuplicateEdges::MERGE);
//   LayerVector layers(3);
//   layers[0] = absl::make_unique<IndexedS2PointVectorLayer>(index);
//   layers[1] = absl::make_unique<IndexedS2PolylineVectorLayer>(
//       index, polyline_options);
//   layers[2] = absl::make_unique<IndexedS2PolygonLayer>(index);
//   S2BooleanOperation op(S2BooleanOperation::OpType::UNION,
//                         NormalizeClosedSet(std::move(layers)));
//   return op.Build(a, b, error);
// }
LayerVector NormalizeClosedSet(LayerVector output_layers,
                               const ClosedSetNormalizer::Options& options =
                               ClosedSetNormalizer::Options());


//////////////////   Implementation details follow   ////////////////////

inline ClosedSetNormalizer::Options::Options()
    : suppress_lower_dimensions_(true) {}

inline bool ClosedSetNormalizer::Options::suppress_lower_dimensions() const {
  return suppress_lower_dimensions_;
}

inline void ClosedSetNormalizer::Options::set_suppress_lower_dimensions(
    bool suppress_lower_dimensions) {
  suppress_lower_dimensions_ = suppress_lower_dimensions;
}

inline const std::vector<S2Builder::GraphOptions>&
ClosedSetNormalizer::graph_options() const {
  return graph_options_in_;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_CLOSED_SET_NORMALIZER_H_
