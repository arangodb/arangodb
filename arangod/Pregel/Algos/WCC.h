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

#ifndef ARANGODB_PREGEL_ALGOS_WCC_H
#define ARANGODB_PREGEL_ALGOS_WCC_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// The idea behind the algorithm is very simple: propagate the smallest
/// vertex id along the edges to all vertices of a connected component. The
/// number of supersteps necessary is equal to the length of the maximum
/// diameter of all components + 1
/// doesn't necessarily leads to a correct result on unidirected graphs
struct WCC : public SimpleAlgorithm<uint64_t, uint64_t, SenderMessage<uint64_t>> {
 public:
  explicit WCC(application_features::ApplicationServer& server, VPackSlice userParams)
      : SimpleAlgorithm(server, "WCC", userParams) {}

  bool supportsAsyncMode() const override { return false; }
  bool supportsCompensation() const override { return false; }

  GraphFormat<uint64_t, uint64_t>* inputFormat() const override;

  MessageFormat<SenderMessage<uint64_t>>* messageFormat() const override {
    return new SenderMessageFormat<uint64_t>();
  }
  MessageCombiner<SenderMessage<uint64_t>>* messageCombiner() const override {
    return nullptr;
  }
  VertexComputation<uint64_t, uint64_t, SenderMessage<uint64_t>>* createComputation(WorkerConfig const*) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
