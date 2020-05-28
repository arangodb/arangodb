////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CONSENSUS_AGENT_CALLBACK_H
#define ARANGOD_CONSENSUS_AGENT_CALLBACK_H 1

#include "Agency/AgencyCommon.h"

namespace arangodb {
namespace network {
struct Response;
}

namespace consensus {

class Agent;

class AgentCallback final {
 public:
  AgentCallback();

  AgentCallback(Agent*, std::string const&, index_t, size_t);

  bool operator()(network::Response const& r);

  void shutdown();

 private:
  application_features::ApplicationServer* _server;
  Agent* _agent;
  index_t _last;
  std::string _slaveID;
  size_t _toLog;
  double _startTime;
};
}  // namespace consensus
}  // namespace arangodb

#endif
