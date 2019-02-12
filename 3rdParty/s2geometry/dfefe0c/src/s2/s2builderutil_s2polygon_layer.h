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
//
// Note that there are two supported output types for polygons: S2Polygon and
// S2LaxPolygonShape.  Use S2Polygon if you need the full range of operations
// that S2Polygon implements.  Use S2LaxPolygonShape if you want to represent
// polygons with zero-area degenerate regions, or if you need a type that has
// low memory overhead and fast initialization.  However, be aware that to
// convert from a S2LaxPolygonShape to an S2Polygon you will need to use
// S2Builder again.
//
// Similarly, there are two supported output formats for polygon meshes:
// S2LaxPolygonShapeVector and S2PolygonMesh.  Use S2PolygonMesh if you need
// to be able to determine which polygons are adjacent to each edge or vertex;
// otherwise use S2LaxPolygonShapeVector, which uses less memory and is faster
// to construct.

#ifndef S2_S2BUILDERUTIL_S2POLYGON_LAYER_H_
#define S2_S2BUILDERUTIL_S2POLYGON_LAYER_H_

#include <memory>
#include <utility>
#include <vector>
#include "s2/base/logging.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2error.h"
#include "s2/s2loop.h"
#include "s2/s2polygon.h"
#include "s2/util/gtl/btree_map.h"

namespace s2builderutil {

// A layer type that assembles edges (directed or undirected) into an
// S2Polygon.  Returns an error if the edges cannot be assembled into loops.
//
// If the input edges are directed, they must be oriented such that the
// polygon interior is to the left of all edges.  Directed edges are always
// preferred (see S2Builder::EdgeType).
//
// Before the edges are assembled into loops, "sibling pairs" consisting of an
// edge and its reverse edge are automatically removed.  Such edge pairs
// represent zero-area degenerate regions, which S2Polygon does not allow.
// (If you need to build polygons with degeneracies, use LaxPolygonLayer
// instead.)
//
// S2PolygonLayer is implemented such that if the input to S2Builder is a
// polygon and is not modified, then the output has the same cyclic ordering
// of loop vertices and the same loop ordering as the input polygon.
//
// If the polygon has no edges, then the graph's IsFullPolygonPredicate is
// called to determine whether the output polygon should be empty (containing
// no points) or full (containing all points).  This predicate can be
// specified as part of the S2Builder input geometry.
class S2PolygonLayer : public S2Builder::Layer {
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

    // If true, calls FindValidationError() on the output polygon.  If any
    // error is found, it will be returned by S2Builder::Build().
    //
    // Note that this option calls set_s2debug_override(S2Debug::DISABLE) in
    // order to turn off the default error checking in debug builds.
    //
    // DEFAULT: false
    bool validate() const;
    void set_validate(bool validate);

   private:
    S2Builder::EdgeType edge_type_;
    bool validate_;
  };

  // Specifies that a polygon should be constructed using the given options.
  explicit S2PolygonLayer(S2Polygon* polygon,
                          const Options& options = Options());

  // Specifies that a polygon should be constructed using the given options,
  // and that any labels attached to the input edges should be returned in
  // "label_set_ids" and "label_set_lexicion".
  //
  // The labels associated with the edge "polygon.loop(i).vertex({j, j+1})"
  // can be retrieved as follows:
  //
  //   for (int32 label : label_set_lexicon.id_set(label_set_ids[i][j])) {...}
  using LabelSetIds = std::vector<std::vector<LabelSetId>>;
  S2PolygonLayer(S2Polygon* polygon, LabelSetIds* label_set_ids,
                 IdSetLexicon* label_set_lexicon,
                 const Options& options = Options());

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  void Init(S2Polygon* polygon, LabelSetIds* label_set_ids,
            IdSetLexicon* label_set_lexicon, const Options& options);
  void AppendS2Loops(const Graph& g,
                     const std::vector<Graph::EdgeLoop>& edge_loops,
                     std::vector<std::unique_ptr<S2Loop>>* loops) const;
  void AppendEdgeLabels(const Graph& g,
                        const std::vector<Graph::EdgeLoop>& edge_loops);
  using LoopMap = gtl::btree_map<S2Loop*, std::pair<int, bool>>;
  void InitLoopMap(const std::vector<std::unique_ptr<S2Loop>>& loops,
                   LoopMap* loop_map) const;
  void ReorderEdgeLabels(const LoopMap& loop_map);

  S2Polygon* polygon_;
  LabelSetIds* label_set_ids_;
  IdSetLexicon* label_set_lexicon_;
  Options options_;
};

// Like S2PolygonLayer, but adds the polygon to a MutableS2ShapeIndex (if the
// polygon is non-empty).
class IndexedS2PolygonLayer : public S2Builder::Layer {
 public:
  using Options = S2PolygonLayer::Options;
  explicit IndexedS2PolygonLayer(MutableS2ShapeIndex* index,
                                 const Options& options = Options())
      : index_(index), polygon_(new S2Polygon),
        layer_(polygon_.get(), options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok() && !polygon_->is_empty()) {
      index_->Add(
          absl::make_unique<S2Polygon::OwningShape>(std::move(polygon_)));
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::unique_ptr<S2Polygon> polygon_;
  S2PolygonLayer layer_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2PolygonLayer::Options::Options()
    : edge_type_(S2Builder::EdgeType::DIRECTED), validate_(false) {
}

inline S2PolygonLayer::Options::Options(S2Builder::EdgeType edge_type)
    : edge_type_(edge_type), validate_(false) {
}

inline S2Builder::EdgeType S2PolygonLayer::Options::edge_type() const {
  return edge_type_;
}

inline void S2PolygonLayer::Options::set_edge_type(
    S2Builder::EdgeType edge_type) {
  edge_type_ = edge_type;
}

inline bool S2PolygonLayer::Options::validate() const {
  return validate_;
}

inline void S2PolygonLayer::Options::set_validate(bool validate) {
  validate_ = validate;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_S2POLYGON_LAYER_H_
