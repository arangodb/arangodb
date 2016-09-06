////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "AgentCallback.h"

#include "Agent.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

AgentCallback::AgentCallback() : _agent(0), _last(0) {}

AgentCallback::AgentCallback(Agent* agent, std::string const& slaveID,
                             index_t last)
    : _agent(agent), _last(last), _slaveID(slaveID) {}

void AgentCallback::shutdown() { _agent = 0; }

bool AgentCallback::operator()(arangodb::ClusterCommResult* res) {
  if (res->status == CL_COMM_SENT) {
    if (_agent) {
      _agent->reportIn(_slaveID, _last);
    }
  } else {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "comm_status(" << res->status
                                     << "), last(" << _last << "), follower("
                                     << _slaveID << ")";
  }
  return true;
}
