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

#include "Agent.h"
#include "StoreCallback.h"

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

StoreCallback::StoreCallback(
  std::string const& url, query_t const& body, Agent* agent)
  : _url(url), _body(body), _agent(agent) {}

bool StoreCallback::operator()(arangodb::ClusterCommResult* res) {

  if (res->status == CL_COMM_ERROR) {
    LOG_TOPIC("9sdbf0", TRACE, Logger::AGENCY)
      << _url << "(" << res->status << ", " << res->errorMessage
      << "): " << _body->toJson();

    if (res->result->getHttpReturnCode() == 404 && _agent != nullptr) {
      LOG_TOPIC("9sdbf0", DEBUG, Logger::AGENCY) << "dropping dead callback at " << _url;
      _agent->trashStoreCallback(_url, _body);
    }
  }

  return true;
}
