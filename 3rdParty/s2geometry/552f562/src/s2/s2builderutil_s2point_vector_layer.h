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

#ifndef S2_S2BUILDERUTIL_S2POINT_VECTOR_LAYER_H_
#define S2_S2BUILDERUTIL_S2POINT_VECTOR_LAYER_H_

#include <memory>
#include <vector>
#include "s2/base/logging.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builder_graph.h"
#include "s2/s2builder_layer.h"
#include "s2/s2error.h"
#include "s2/s2point_vector_shape.h"

namespace s2builderutil {

// A layer type that collects degenerate edges as points.
// This layer expects all edges to be degenerate. In case of finding
// non-degenerate edges it sets S2Error but it still generates the
// output with degenerate edges.
class S2PointVectorLayer : public S2Builder::Layer {
 public:
  class Options {
   public:
    using DuplicateEdges = GraphOptions::DuplicateEdges;
    Options();
    explicit Options(DuplicateEdges duplicate_edges);

    // DEFAULT: DuplicateEdges::MERGE
    DuplicateEdges duplicate_edges() const;
    void set_duplicate_edges(DuplicateEdges duplicate_edges);

   private:
    DuplicateEdges duplicate_edges_;
  };

  explicit S2PointVectorLayer(std::vector<S2Point>* points,
                              const Options& options = Options());

  using LabelSetIds = std::vector<LabelSetId>;
  S2PointVectorLayer(std::vector<S2Point>* points, LabelSetIds* label_set_ids,
                     IdSetLexicon* label_set_lexicon,
                     const Options& options = Options());

  // Layer interface:
  GraphOptions graph_options() const override;
  void Build(const Graph& g, S2Error* error) override;

 private:
  std::vector<S2Point>* points_;
  LabelSetIds* label_set_ids_;
  IdSetLexicon* label_set_lexicon_;
  Options options_;
};

// Like S2PointVectorLayer, but adds the points to a MutableS2ShapeIndex (if
// the point vector is non-empty).
class IndexedS2PointVectorLayer : public S2Builder::Layer {
 public:
  using Options = S2PointVectorLayer::Options;
  explicit IndexedS2PointVectorLayer(MutableS2ShapeIndex* index,
                                     const Options& options = Options())
      : index_(index), layer_(&points_, options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok() && !points_.empty()) {
      index_->Add(absl::make_unique<S2PointVectorShape>(std::move(points_)));
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::vector<S2Point> points_;
  S2PointVectorLayer layer_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2PointVectorLayer::Options::Options()
    : duplicate_edges_(DuplicateEdges::MERGE) {}

inline S2PointVectorLayer::Options::Options(DuplicateEdges duplicate_edges)
    : duplicate_edges_(duplicate_edges) {}

inline S2Builder::GraphOptions::DuplicateEdges
S2PointVectorLayer::Options::duplicate_edges() const {
  return duplicate_edges_;
}

inline void S2PointVectorLayer::Options::set_duplicate_edges(
    S2Builder::GraphOptions::DuplicateEdges duplicate_edges) {
  duplicate_edges_ = duplicate_edges;
}

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_S2POINT_VECTOR_LAYER_H_
