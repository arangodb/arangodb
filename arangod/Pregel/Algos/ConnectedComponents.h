////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_ALGOS_CONNECTED_COMPONENTS_H
#define ARANGODB_PREGEL_ALGOS_CONNECTED_COMPONENTS_H 1

#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// The idea behind the algorithm is very simple: propagate the smallest
/// vertex id along the edges to all vertices of a connected component. The
/// number of supersteps necessary is equal to the length of the maximum
/// diameter of all components + 1
/// doesn't necessarily leads to a correct result on unidirected graphs
struct ConnectedComponents : public SimpleAlgorithm<uint64_t, uint8_t, uint64_t> {
 public:
  explicit ConnectedComponents(application_features::ApplicationServer& server, VPackSlice userParams)
      : SimpleAlgorithm(server, "ConnectedComponents", userParams) {}

  bool supportsAsyncMode() const override { return true; }
  bool supportsCompensation() const override { return true; }

  GraphFormat<uint64_t, uint8_t>* inputFormat() const override;

  MessageFormat<uint64_t>* messageFormat() const override {
    return new IntegerMessageFormat<uint64_t>();
  }
  MessageCombiner<uint64_t>* messageCombiner() const override {
    return new MinCombiner<uint64_t>();
  }
  VertexComputation<uint64_t, uint8_t, uint64_t>* createComputation(WorkerConfig const*) const override;
  VertexCompensation<uint64_t, uint8_t, uint64_t>* createCompensation(WorkerConfig const*) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
