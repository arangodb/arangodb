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
//
// Note that there are two supported output types for polygons: S2Polygon and
// S2LaxPolygonShape.  Use S2Polygon if you need the full range of operations
// that S2Polygon implements.  Use S2LaxPolygonShape if you want to represent
// polygons with zero-area degenerate regions, or if you need a type that has
// low memory overhead and fast initialization.  However, be aware that to
// convert from S2LaxPolygonShape to S2Polygon you will need to use S2Builder
// again.
//
// Similarly, there are two supported output formats for polygon meshes:
// S2PolygonMesh and S2LaxPolygonShapeVector.  Use S2PolygonMesh if you need
// to be able to determine which polygons are adjacent to each edge or vertex;
// otherwise use S2LaxPolygonShapeVector, which uses less memory and is faster
// to construct.

#ifndef S2_S2BUILDERUTIL_LAX_POLYGON_LAYER_H_
#define S2_S2BUILDERUTIL_LAX_POLYGON_LAYER_H_

#include <memory>
#include <utility>
#include <vector>

#include "s2/base/logging.h"
#include "absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2error.h"
#include "s2/s2lax_polygon_shape.h"

namespace s2builderutil {

// A layer type that assembles edges (directed or undirected) into an
// S2LaxPolygonShape.  Returns an error if the edges cannot be assembled into
// loops.
//
// If the input edges are directed, they must be oriented such that the
// polygon interior is to the left of all edges.  Directed edges are always
// preferred (see S2Builder::EdgeType).
//
// LaxPolygonLayer is implemented such that if the input to S2Builder is a
// polygon and is not modified, then the output has the same cyclic ordering
// of loop vertices and the same loop ordering as the input polygon.
//
// If the given edge graph is degenerate (i.e., it consists entirely of
// degenerate edges and sibling pairs), then the IsFullPolygonPredicate
// associated with the edge graph is called to determine whether the output
// polygon should be empty (possibly with degenerate shells) or full (possibly
// with degenerate holes).  This predicate can be specified as part of the
// S2Builder input geometry.
class LaxPolygonLayer : public S2Builder::Layer {
 public:
  class Options {
   public:
    // Constructor that uses the default options (listed below).
    Options();

    // Constructor that specifies the edge type.
    explicit Options(S2Builder::EdgeType edge_type);

    // Indicates whether the input edges provided to S2Builder are directed or
    // undirected.  Directed edges should be used whenever possible (see
    // S2Builder::EdgeType for details).
    //
    // If the input edges are directed, they should be oriented so that the
    // polygon interior is to the left of all edges.  This means that for a
    // polygon with holes, the outer loops ("shells") should be directed
    // counter-clockwise while the inner loops ("holes") should be directed
    // clockwise.  Note that S2Builder::AddPolygon() does this automatically.
    //
    // DEFAULT: S2Builder::EdgeType::DIRECTED
    S2Builder::EdgeType edge_type() const;
    void set_edge_type(S2Builder::EdgeType edge_type);

    // Specifies whether degenerate boundaries should be discarded or kept.
    // (A degenerate boundary consists of either a sibling edge pair or an
    // edge from a vertex to itself.)  Optionally, degenerate boundaries may
    // be kept only if they represent shells, or only if they represent holes.
    //
    // This option is useful for normalizing polygons with various boundary
    // conditions.  For example, DISCARD_HOLES can be used to normalize closed
    // polygons (those that include their boundary), since degenerate holes do
    // not affect the set of points contained by such polygons.  Similarly,
    // DISCARD_SHELLS can be used to normalize polygons with open boundaries.
    // DISCARD is used to normalize polygons with semi-open boundaries (since
    // degenerate loops do not affect point containment in that case), and
    // finally KEEP is useful for working with any type of polygon where
    // degeneracies are assumed to contain an infinitesmal interior.  (This
    // last model is the most useful for working with simplified geometry,
    // since it maintains the closest fidelity to the original geometry.)
    //
    // DEFAULT: DegenerateBoundaries::KEEP
    enum class DegenerateBoundaries : uint8 {
      DISCARD, DISCARD_HOLES, DISCARD_SHELLS, KEEP
    };
    DegenerateBoundaries degenerate_boundaries() const;
    void set_degenerate_boundaries(DegenerateBoundaries degenerate_boundaries);

   private:
    S2Builder::EdgeType edge_type_;
    DegenerateBoundaries degenerate_boundaries_;
  };

  // Specifies that a polygon should be constructed using the given options.
  explicit LaxPolygonLayer(S2LaxPolygonShape* polygon,
                           const Options& options = Options());

  // Specifies that a polygon should be constructed using the given options,
  // and that any labels attached to the input edges should be returned in
  // "label_set_ids" and "label_set_lexicion".
  //
  // The labels associated with the edge "polygon.chain_edge(i, j)"
  // can be retrieved as follows:
  //
  //   for (int32 label : label_set_lexicon.id_set(label_set_ids[i][j])) {...}
  using LabelSetIds = std::vector<std::vector<LabelSetId>>;
  LaxPolygonLayer(S2LaxPolygonShape* polygon, LabelSetIds* label_set_ids,
                  IdSetLexicon* label_set_lexicon,
                  const Options& options = Options());

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  void Init(S2LaxPolygonShape* polygon, LabelSetIds* label_set_ids,
            IdSetLexicon* label_set_lexicon, const Options& options);
  void AppendPolygonLoops(const Graph& g,
                          const std::vector<Graph::EdgeLoop>& edge_loops,
                          std::vector<std::vector<S2Point>>* loops) const;
  void AppendEdgeLabels(const Graph& g,
                        const std::vector<Graph::EdgeLoop>& edge_loops);
  void BuildDirected(Graph g, S2Error* error);

  S2LaxPolygonShape* polygon_;
  LabelSetIds* label_set_ids_;
  IdSetLexicon* label_set_lexicon_;
  Options options_;
};

// Like LaxPolygonLayer, but adds the polygon to a MutableS2ShapeIndex (if the
// polygon is non-empty).
class IndexedLaxPolygonLayer : public S2Builder::Layer {
 public:
  using Options = LaxPolygonLayer::Options;
  explicit IndexedLaxPolygonLayer(MutableS2ShapeIndex* index,
                                 const Options& options = Options())
      : index_(index), polygon_(new S2LaxPolygonShape),
        layer_(polygon_.get(), options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok() && !polygon_->is_empty()) {
      index_->Add(std::move(polygon_));
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::unique_ptr<S2LaxPolygonShape> polygon_;
  LaxPolygonLayer layer_;
};


//////////////////   Implementation details follow   ////////////////////


inline LaxPolygonLayer::Options::Options()
    : Options(S2Builder::EdgeType::DIRECTED) {
}

inline LaxPolygonLayer::Options::Options(S2Builder::EdgeType edge_type)
    : edge_type_(edge_type),
      degenerate_boundaries_(DegenerateBoundaries::KEEP) {
}

inline S2Builder::EdgeType LaxPolygonLayer::Options::edge_type() const {
  return edge_type_;
}

inline void LaxPolygonLayer::Options::set_edge_type(
    S2Builder::EdgeType edge_type) {
  edge_type_ = edge_type;
}

inline LaxPolygonLayer::Options::DegenerateBoundaries
LaxPolygonLayer::Options::degenerate_boundaries() const {
  return degenerate_boundaries_;
}

inline void LaxPolygonLayer::Options::set_degenerate_boundaries(
    DegenerateBoundaries degenerate_boundaries) {
  degenerate_boundaries_ = degenerate_boundaries;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_LAX_POLYGON_LAYER_H_
