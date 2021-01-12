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

#ifndef ARANGODB_PREGEL_ALGOS_DMID_H
#define ARANGODB_PREGEL_ALGOS_DMID_H 1

#include "Pregel/Algorithm.h"
#include "Pregel/CommonFormats.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// https://github.com/Rofti/DMID
struct DMID : public SimpleAlgorithm<DMIDValue, float, DMIDMessage> {
  unsigned _maxCommunities = 1;

 public:
  explicit DMID(application_features::ApplicationServer& server, VPackSlice userParams)
      : SimpleAlgorithm<DMIDValue, float, DMIDMessage>(server, "DMID", userParams) {
    arangodb::velocypack::Slice val = userParams.get("maxCommunities");
    if (val.isInteger()) {
      _maxCommunities =
          (unsigned)std::min((uint64_t)32, std::max(val.getUInt(), (uint64_t)0));
    }
  }

  GraphFormat<DMIDValue, float>* inputFormat() const override;
  MessageFormat<DMIDMessage>* messageFormat() const override;

  VertexComputation<DMIDValue, float, DMIDMessage>* createComputation(WorkerConfig const*) const override;

  MasterContext* masterContext(VPackSlice userParams) const override;

  IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
