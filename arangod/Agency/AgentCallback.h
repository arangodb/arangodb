////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

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

  AgentCallback(Agent*, std::string, index_t, size_t);

  bool operator()(network::Response const& r) const;

 private:
  Agent* _agent;
  index_t _last;
  std::string _followerId;
  size_t _toLog;
  double _startTime;
};
}  // namespace consensus
}  // namespace arangodb
