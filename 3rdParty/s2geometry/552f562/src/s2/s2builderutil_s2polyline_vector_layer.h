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

#ifndef S2_S2BUILDERUTIL_S2POLYLINE_VECTOR_LAYER_H_
#define S2_S2BUILDERUTIL_S2POLYLINE_VECTOR_LAYER_H_

#include <memory>
#include <vector>
#include "s2/base/logging.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2debug.h"
#include "s2/s2error.h"
#include "s2/s2polyline.h"

namespace s2builderutil {

// A layer type that assembles edges (directed or undirected) into multiple
// S2Polylines.  Returns an error if S2Builder found any problem with the
// input edges; this layer type does not generate any errors of its own.
//
// Duplicate edges are handled correctly (e.g., if a polyline backtracks on
// itself, or loops around and retraces some of its previous edges.)  The
// implementation attempts to preserve the order of the input edges whenever
// possible, so that if the input is a polyline and it is not modified by
// S2Builder, then the output will be the same polyline even if the polyline
// forms a loop.  However, note that this is not guaranteed when undirected
// edges are used: for example, if the input consists of a single undirected
// edge, then either directed edge may be returned.
class S2PolylineVectorLayer : public S2Builder::Layer {
 public:
  class Options {
   public:
    // Constructor that uses the default options (listed below).
    Options();

    // Constructor that specifies the edge type.
    explicit Options(S2Builder::EdgeType edge_type);

    // Indicates whether the input edges provided to S2Builder are directed or
    // undirected.
    //
    // Directed edges should be used whenever possible to avoid ambiguity.
    // The implementation attempts to preserve the structure of directed input
    // edges whenever possible, so that if the input is a vector of disjoint
    // polylines and none of them need to be modified then the output will be
    // the same polylines in the same order.  With undirected edges, there are
    // no such guarantees.
    //
    // DEFAULT: S2Builder::EdgeType::DIRECTED
    S2Builder::EdgeType edge_type() const;
    void set_edge_type(S2Builder::EdgeType edge_type);

    // Indicates whether polylines should be "paths" (which don't allow
    // duplicate vertices, except possibly the first and last vertex) or
    // "walks" (which allow duplicate vertices and edges).
    //
    // If your input consists of polylines, and you want to split them into
    // separate pieces whenever they self-intersect or cross each other, then
    // use PolylineType::PATH (and probably use split_crossing_edges()).  If
    // you don't mind if your polylines backtrack or contain loops, then use
    // PolylineType::WALK.
    //
    // DEFAULT: PolylineType::PATH
    using PolylineType = S2Builder::Graph::PolylineType;
    PolylineType polyline_type() const;
    void set_polyline_type(PolylineType polyline_type);

    // Indicates whether duplicate edges in the input should be kept (KEEP) or
    // merged together (MERGE).  Note you can use edge labels to determine
    // which input edges were merged into a given output edge.
    //
    // DEFAULT: DuplicateEdges::KEEP
    using DuplicateEdges = GraphOptions::DuplicateEdges;
    DuplicateEdges duplicate_edges() const;
    void set_duplicate_edges(DuplicateEdges duplicate_edges);

    // Indicates whether sibling edge pairs (i.e., pairs consisting of an edge
    // and its reverse edge) should be kept (KEEP) or discarded (DISCARD).
    // For example, if a polyline backtracks on itself, the DISCARD option
    // would cause this section of the polyline to be removed.  Note that this
    // option may cause a single polyline to split into several pieces (e.g.,
    // if a polyline has a "lollipop" shape).
    //
    // REQUIRES: sibling_pairs == { DISCARD, KEEP }
    //           (the CREATE and REQUIRE options are not allowed)
    //
    // DEFAULT: SiblingPairs::KEEP
    using SiblingPairs = GraphOptions::SiblingPairs;
    SiblingPairs sibling_pairs() const;
    void set_sibling_pairs(SiblingPairs sibling_pairs);

    // If true, calls FindValidationError() on each output polyline.  If any
    // error is found, it will be returned by S2Builder::Build().
    //
    // Note that this option calls set_s2debug_override(S2Debug::DISABLE) in
    // order to turn off the default error checking in debug builds.
    //
    // DEFAULT: false
    bool validate() const;
    void set_validate(bool validate);

    // This method can turn off the automatic validity checks triggered by the
    // --s2debug flag (which is on by default in debug builds).  The main
    // reason to do this is if your code already does its own error checking,
    // or if you need to work with invalid geometry for some reason.
    //
    // In any case, polylines have very few restrictions so they are unlikely
    // to have errors.  Errors include vertices that aren't unit length (which
    // can only happen if they are present in the input data), or adjacent
    // vertices that are at antipodal points on the sphere (unlikely with real
    // data).  The other possible error is adjacent identical vertices, but
    // this can't happen because S2Builder does not generate such polylines.
    //
    // DEFAULT: S2Debug::ALLOW
    S2Debug s2debug_override() const;
    void set_s2debug_override(S2Debug override);

   private:
    S2Builder::EdgeType edge_type_;
    PolylineType polyline_type_;
    DuplicateEdges duplicate_edges_;
    SiblingPairs sibling_pairs_;
    bool validate_;
    S2Debug s2debug_override_;
  };

  // Specifies that a vector of polylines should be constructed using the
  // given options.
  explicit S2PolylineVectorLayer(
      std::vector<std::unique_ptr<S2Polyline>>* polylines,
      const Options& options = Options());

  // Specifies that a vector of polylines should be constructed using the
  // given options, and that any labels attached to the input edges should be
  // returned in "label_set_ids" and "label_set_lexicion".
  //
  // The labels associated with the edge "polyline[i].vertex({j, j+1})" can be
  // retrieved as follows:
  //
  //   for (int32 label : label_set_lexicon.id_set(label_set_ids[i][j])) {...}
  using LabelSetIds = std::vector<std::vector<LabelSetId>>;
  S2PolylineVectorLayer(std::vector<std::unique_ptr<S2Polyline>>* polylines,
                        LabelSetIds* label_set_ids,
                        IdSetLexicon* label_set_lexicon,
                        const Options& options = Options());

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  void Init(std::vector<std::unique_ptr<S2Polyline>>* polylines,
            LabelSetIds* label_set_ids, IdSetLexicon* label_set_lexicon,
            const Options& options);

  std::vector<std::unique_ptr<S2Polyline>>* polylines_;
  LabelSetIds* label_set_ids_;
  IdSetLexicon* label_set_lexicon_;
  Options options_;
};

// Like S2PolylineVectorLayer, but adds the polylines to a MutableS2ShapeIndex.
class IndexedS2PolylineVectorLayer : public S2Builder::Layer {
 public:
  using Options = S2PolylineVectorLayer::Options;
  explicit IndexedS2PolylineVectorLayer(MutableS2ShapeIndex* index,
                                        const Options& options = Options())
      : index_(index), layer_(&polylines_, options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok()) {
      for (auto& polyline : polylines_) {
        index_->Add(
            absl::make_unique<S2Polyline::OwningShape>(std::move(polyline)));
      }
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::vector<std::unique_ptr<S2Polyline>> polylines_;
  S2PolylineVectorLayer layer_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2PolylineVectorLayer::Options::Options()
    : edge_type_(S2Builder::EdgeType::DIRECTED),
      polyline_type_(PolylineType::PATH),
      duplicate_edges_(DuplicateEdges::KEEP),
      sibling_pairs_(SiblingPairs::KEEP),
      validate_(false),
      s2debug_override_(S2Debug::ALLOW) {
}

inline S2PolylineVectorLayer::Options::Options(S2Builder::EdgeType edge_type)
    : edge_type_(edge_type),
      polyline_type_(PolylineType::PATH),
      duplicate_edges_(DuplicateEdges::KEEP),
      sibling_pairs_(SiblingPairs::KEEP),
      validate_(false),
      s2debug_override_(S2Debug::ALLOW) {
}

inline S2Builder::EdgeType S2PolylineVectorLayer::Options::edge_type() const {
  return edge_type_;
}

inline void S2PolylineVectorLayer::Options::set_edge_type(
    S2Builder::EdgeType edge_type) {
  edge_type_ = edge_type;
}

inline S2PolylineVectorLayer::Options::PolylineType
S2PolylineVectorLayer::Options::polyline_type() const {
  return polyline_type_;
}

inline void S2PolylineVectorLayer::Options::set_polyline_type(
    PolylineType polyline_type) {
  polyline_type_ = polyline_type;
}

inline S2PolylineVectorLayer::Options::DuplicateEdges
S2PolylineVectorLayer::Options::duplicate_edges() const {
  return duplicate_edges_;
}

inline void S2PolylineVectorLayer::Options::set_duplicate_edges(
    DuplicateEdges duplicate_edges) {
  duplicate_edges_ = duplicate_edges;
}

inline S2PolylineVectorLayer::Options::SiblingPairs
S2PolylineVectorLayer::Options::sibling_pairs() const {
  return sibling_pairs_;
}

inline void S2PolylineVectorLayer::Options::set_sibling_pairs(
    SiblingPairs sibling_pairs) {
  S2_DCHECK(sibling_pairs == SiblingPairs::KEEP ||
         sibling_pairs == SiblingPairs::DISCARD);
  sibling_pairs_ = sibling_pairs;
}

inline bool S2PolylineVectorLayer::Options::validate() const {
  return validate_;
}

inline void S2PolylineVectorLayer::Options::set_validate(bool validate) {
  validate_ = validate;
  set_s2debug_override(S2Debug::DISABLE);
}

inline S2Debug S2PolylineVectorLayer::Options::s2debug_override() const {
  return s2debug_override_;
}

inline void S2PolylineVectorLayer::Options::set_s2debug_override(
    S2Debug s2debug_override) {
  s2debug_override_ = s2debug_override;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_S2POLYLINE_VECTOR_LAYER_H_
