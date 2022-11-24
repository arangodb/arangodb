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

#ifndef S2_S2BUILDER_LAYER_H_
#define S2_S2BUILDER_LAYER_H_

#include "s2/s2builder_graph.h"

// This class is not needed by ordinary S2Builder clients.  It is only
// necessary if you wish to implement a new S2Builder::Layer subtype.
class S2Builder::Layer {
 public:
  // Convenience declarations for layer subtypes.
  using EdgeType = S2Builder::EdgeType;
  using GraphOptions = S2Builder::GraphOptions;
  using Graph = S2Builder::Graph;
  using Label = S2Builder::Label;
  using LabelSetId = S2Builder::LabelSetId;

  virtual ~Layer() {}

  // Defines options for building the edge graph that is passed to Build().
  virtual GraphOptions graph_options() const = 0;

  // Assembles a graph of snapped edges into the geometry type implemented by
  // this layer.  If an error is encountered, sets "error" appropriately.
  //
  // Note that when there are multiple layers, the Graph objects passed to all
  // layers are guaranteed to be valid until the last Build() method returns.
  // This makes it easier to write algorithms that gather the output graphs
  // from several layers and process them all at once (such as
  // s2builderutil::ClosedSetNormalizer).
  virtual void Build(const Graph& g, S2Error* error) = 0;
};

#endif  // S2_S2BUILDER_LAYER_H_
