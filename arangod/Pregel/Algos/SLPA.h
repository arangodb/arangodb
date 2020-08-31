////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_ALGOS_SLPA_H
#define ARANGODB_PREGEL_ALGOS_SLPA_H 1

#include <cmath>
#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// SLPA algorithm:
/// Overlap is one of the characteristics of social
/// networks, in which a person may belong to more than one social
/// group. For this reason, discovering overlapping structure
/// is  necessary  for  realistic  social  analysis.  In the SLPA algorithm
/// nodes  exchange  labels  according  to  dynamic
/// interaction  rules.  It has excellent performance in identifying both
/// overlapping
/// nodes and  overlapping  communities  with  different  degrees  of diversity.
struct SLPA : public SimpleAlgorithm<SLPAValue, int8_t, uint64_t> {
  double _threshold = 0.15;
  unsigned _maxCommunities = 1;

 public:
  explicit SLPA(application_features::ApplicationServer& server, VPackSlice userParams)
      : SimpleAlgorithm<SLPAValue, int8_t, uint64_t>(server, "slpa", userParams) {
    arangodb::velocypack::Slice val = userParams.get("threshold");
    if (val.isNumber()) {
      _threshold = std::min(1.0, std::max(val.getDouble(), 0.0));
    }
    val = userParams.get("maxCommunities");
    if (val.isInteger()) {
      _maxCommunities =
          (unsigned)std::min((uint64_t)32, std::max(val.getUInt(), (uint64_t)0));
    }
  }

  GraphFormat<SLPAValue, int8_t>* inputFormat() const override;
  MessageFormat<uint64_t>* messageFormat() const override {
    return new NumberMessageFormat<uint64_t>();
  }

  VertexComputation<SLPAValue, int8_t, uint64_t>* createComputation(WorkerConfig const*) const override;
  WorkerContext* workerContext(velocypack::Slice userParams) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
