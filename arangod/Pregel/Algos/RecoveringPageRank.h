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

#ifndef ARANGODB_PREGEL_ALGOS_RECO_PAGERANK_H
#define ARANGODB_PREGEL_ALGOS_RECO_PAGERANK_H 1

#include <velocypack/Slice.h>
#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// PageRank
struct RecoveringPageRank : public SimpleAlgorithm<float, float, float> {
  explicit RecoveringPageRank(application_features::ApplicationServer& server,
                              arangodb::velocypack::Slice params)
      : SimpleAlgorithm(server, "PageRank", params) {}

  bool supportsCompensation() const override { return true; }
  MasterContext* masterContext(VPackSlice userParams) const override;

  GraphFormat<float, float>* inputFormat() const override {
    return new VertexGraphFormat<float, float>(_server, _resultField, 0);
  }

  MessageFormat<float>* messageFormat() const override {
    return new NumberMessageFormat<float>();
  }

  MessageCombiner<float>* messageCombiner() const override {
    return new SumCombiner<float>();
  }

  VertexComputation<float, float, float>* createComputation(WorkerConfig const*) const override;
  VertexCompensation<float, float, float>* createCompensation(WorkerConfig const*) const override;
  IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
