////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Pregel/Algorithm.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/SenderMessageFormat.h"

namespace arangodb::pregel::algos {

/// Finds strongly connected components of the graph.
///
/// 1. Each vertex starts with its vertex id as its "color".
/// 2. Remove vertices which cannot be in a SCC (no incoming or no outgoing
/// edges)
/// 3. Propagate the color forward from each vertex, accept a neighbor's color
/// if it's smaller than yours.
///    At convergence, vertices with the same color represents all nodes that
///    are visitable from the root of that color.
/// 4. Reverse the graph.
/// 5. Start at all roots, walk the graph. Visit a neighbor if it has the same
/// color as you.
///    All nodes visited belongs to the SCC identified by the root color.

struct HITS : public SimpleAlgorithm<HITSValue, int8_t, SenderMessage<double>> {
 public:
  explicit HITS(application_features::ApplicationServer& server,
                VPackSlice userParams)
      : SimpleAlgorithm<HITSValue, int8_t, SenderMessage<double>>(
            server, "hits", userParams) {}

  [[nodiscard]] GraphFormat<HITSValue, int8_t>* inputFormat() const override;
  [[nodiscard]] MessageFormat<SenderMessage<double>>* messageFormat()
      const override {
    return new SenderMessageFormat<double>();
  }

  VertexComputation<HITSValue, int8_t, SenderMessage<double>>*
  createComputation(WorkerConfig const*) const override;

  [[nodiscard]] WorkerContext* workerContext(
      VPackSlice userParams) const override;
  [[nodiscard]] MasterContext* masterContext(
      VPackSlice userParams) const override;

  [[nodiscard]] IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace arangodb::pregel::algos
