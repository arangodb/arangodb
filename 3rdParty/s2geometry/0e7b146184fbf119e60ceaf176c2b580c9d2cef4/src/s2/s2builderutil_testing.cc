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

namespace s2builderutil {

void GraphClone::Init(const S2Builder::Graph& g) {
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

}  // namespace s2builderutil
