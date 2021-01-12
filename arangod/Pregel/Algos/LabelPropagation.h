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

#ifndef ARANGODB_PREGEL_ALGOS_LABELPROP_H
#define ARANGODB_PREGEL_ALGOS_LABELPROP_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// Finds communities in a network
/// Tries to assign every vertex to the community in which most of it's
/// neighbours are.
/// Each vertex sends the community ID to all neighbours. Stores the ID he
/// received
/// most frequently. Tries to avoid osscilation, usually won't converge so
/// specify a
/// maximum superstep number.
struct LabelPropagation : public SimpleAlgorithm<LPValue, int8_t, uint64_t> {
 public:
  explicit LabelPropagation(application_features::ApplicationServer& server, VPackSlice userParams)
      : SimpleAlgorithm<LPValue, int8_t, uint64_t>(server, "LabelPropagation", userParams) {}

  GraphFormat<LPValue, int8_t>* inputFormat() const override;
  MessageFormat<uint64_t>* messageFormat() const override {
    return new NumberMessageFormat<uint64_t>();
  }

  VertexComputation<LPValue, int8_t, uint64_t>* createComputation(WorkerConfig const*) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
