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

#include "State.h"
#include "Cluster/ClusterComm.h"

using namespace arangodb::consensus;

State::State() {}
State::~State() {}

bool State::write (store_t const&) {

  /*
  std::string body;
  arangodb::velocypack::Options opts;
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  path << DATABASE_PATH << store_t.toString();
  
  ClusterCommResult result = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
    _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
    path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
    1.0, true);

  if (res.status == CL_COMM_SENT) { // Request successfully sent 
    res = arangodb::ClusterComm::instance()->wait("1", 1, results[i].operationID, "1");
    std::shared_ptr< arangodb::velocypack::Builder > body = res.result->getBodyVelocyPack();
    if (body->isEmpty()) {
      continue;
    } else {
      if (!body->slice().hasKey("vote")) { // Answer has no vote. What happened?
        _votes[i] = false;
        continue;
      } else {
        _votes[i] = (body->slice().get("vote").isEqualString("TRUE")); // Record vote
      }
    }
    }*/
  return OK;
  
};

store_t State::get (std::string const&) const {
/*
  std::string body;
  arangodb::velocypack::Options opts;
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  path << DATABASE_PATH << store_t.toString();
  
  ClusterCommResult result = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
    _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
    path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
    1.0, true);

  if (res.status == CL_COMM_SENT) { // Request successfully sent 
    res = arangodb::ClusterComm::instance()->wait("1", 1, results[i].operationID, "1");
    std::shared_ptr< arangodb::velocypack::Builder > body = res.result->getBodyVelocyPack();
    if (body->isEmpty()) {
      continue;
    } else {
      if (!body->slice().hasKey("vote")) { // Answer has no vote. What happened?
        _votes[i] = false;
        continue;
      } else {
        _votes[i] = (body->slice().get("vote").isEqualString("TRUE")); // Record vote
      }
    }
  }
*/
};


