// Copyright 2020 Google Inc. All Rights Reserved.
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

#ifndef S2_S2BUILDERUTIL_LAX_POLYLINE_LAYER_H_
#define S2_S2BUILDERUTIL_LAX_POLYLINE_LAYER_H_

#include <memory>
#include <vector>

#include "s2/base/logging.h"
#include "absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2error.h"
#include "s2/s2lax_polyline_shape.h"

namespace s2builderutil {

// A layer type that assembles edges (directed or undirected) into an
// S2LaxPolylineShape.  Returns an error if the edges cannot be assembled into
// a single unbroken polyline.
//
// Duplicate edges are handled correctly (e.g., if a polyline backtracks on
// itself, or loops around and retraces some of its previous edges.)  The
// implementation attempts to preserve the order of input edges whenever
// possible, so that if the input is a polyline and it is not modified by
// S2Builder, then the output will be the same polyline (even if the polyline
// backtracks on itself or forms a loop).
//
// LaxPolylineLayer does not support options such as discarding sibling pairs
// or merging duplicate edges because these options can split the polyline
// into several pieces.  TODO(ericv): Implement LaxPolylineVectorLayer.
class LaxPolylineLayer : public S2Builder::Layer {
 public:
  class Options {
   public:
    // Constructor that uses the default options (listed below).
    Options();

    // Constructor that specifies the edge type.
    explicit Options(S2Builder::EdgeType edge_type);

    // Indicates whether the input edges provided to S2Builder are directed or
    // undirected.  Directed edges should be used whenever possible to avoid
    // ambiguity.
    //
    // DEFAULT: S2Builder::EdgeType::DIRECTED
    S2Builder::EdgeType edge_type() const;
    void set_edge_type(S2Builder::EdgeType edge_type);

   private:
    S2Builder::EdgeType edge_type_;
  };

  // Specifies that a polyline should be constructed using the given options.
  explicit LaxPolylineLayer(S2LaxPolylineShape* polyline,
                            const Options& options = Options());

  // Specifies that a polyline should be constructed using the given options,
  // and that any labels attached to the input edges should be returned in
  // "label_set_ids" and "label_set_lexicion".
  //
  // The labels associated with the edge "polyline.vertex({j, j+1})" can be
  // retrieved as follows:
  //
  //   for (int32 label : label_set_lexicon.id_set(label_set_ids[j])) {...}
  using LabelSetIds = std::vector<LabelSetId>;
  LaxPolylineLayer(S2LaxPolylineShape* polyline, LabelSetIds* label_set_ids,
                   IdSetLexicon* label_set_lexicon,
                   const Options& options = Options());

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  void Init(S2LaxPolylineShape* polyline, LabelSetIds* label_set_ids,
            IdSetLexicon* label_set_lexicon, const Options& options);

  S2LaxPolylineShape* polyline_;
  LabelSetIds* label_set_ids_;
  IdSetLexicon* label_set_lexicon_;
  Options options_;
};

// Like LaxPolylineLayer, but adds the polyline to a MutableS2ShapeIndex (if
// the polyline is non-empty).
class IndexedLaxPolylineLayer : public S2Builder::Layer {
 public:
  using Options = LaxPolylineLayer::Options;
  explicit IndexedLaxPolylineLayer(MutableS2ShapeIndex* index,
                                   const Options& options = Options())
      : index_(index), polyline_(new S2LaxPolylineShape),
        layer_(polyline_.get(), options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok() && polyline_->num_vertices() > 0) {
      index_->Add(std::move(polyline_));
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::unique_ptr<S2LaxPolylineShape> polyline_;
  LaxPolylineLayer layer_;
};


//////////////////   Implementation details follow   ////////////////////


inline LaxPolylineLayer::Options::Options()
    : edge_type_(S2Builder::EdgeType::DIRECTED) {
}

inline LaxPolylineLayer::Options::Options(S2Builder::EdgeType edge_type)
    : edge_type_(edge_type) {
}

inline S2Builder::EdgeType LaxPolylineLayer::Options::edge_type() const {
  return edge_type_;
}

inline void LaxPolylineLayer::Options::set_edge_type(
    S2Builder::EdgeType edge_type) {
  edge_type_ = edge_type;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_LAX_POLYLINE_LAYER_H_
