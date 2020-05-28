////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_ALGOS_LINERANK_H
#define ARANGODB_PREGEL_ALGOS_LINERANK_H 1

#include <velocypack/Slice.h>
#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// LineRank from "Centralities in Large Networks: Algorithms and Observations"
/// 2011:
/// Given a directed graph G, the LINERANK score of a node v ∈ G is computed by
/// aggregating the stationary probabilities of its incident edges on the line
/// graph L(G).
/// Implementation based on
/// github.com/JananiC/NetworkCentralities/blob/master/src/main/java/linerank/LineRank.java
struct LineRank : public SimpleAlgorithm<float, float, float> {
 public:
  explicit LineRank(application_features::ApplicationServer& server,
                    arangodb::velocypack::Slice params);

  GraphFormat<float, float>* inputFormat() const override {
    return new VertexGraphFormat<float, float>(_server, _resultField, 0);
  }
  MessageFormat<float>* messageFormat() const override {
    return new NumberMessageFormat<float>();
  }

  MessageCombiner<float>* messageCombiner() const override {
    return new SumCombiner<float>();
  }

  WorkerContext* workerContext(velocypack::Slice params) const override;
  MasterContext* masterContext(velocypack::Slice) const override;

  VertexComputation<float, float, float>* createComputation(WorkerConfig const*) const override;

  IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
