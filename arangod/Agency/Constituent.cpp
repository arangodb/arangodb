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

#include "Cluster/ClusterComm.h"
#include "Basics/Logger.h"

#include "Constituent.h"
#include "Agent.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::rest;

void Constituent::configure(Agent* agent) {
  _agent = agent;
}

Constituent::Constituent() : Thread("Constituent"), _term(0),
    _gen(std::random_device()()), _mode(LEADER), _run(true), _agent(0) {}

Constituent::~Constituent() {}

Constituent::duration_t Constituent::sleepFor () {
  dist_t dis(_agent->config().min_ping, _agent->config().max_ping);
  return Constituent::duration_t(dis(_gen));
}

Constituent::term_t Constituent::term() const {
  return _term;
}

void Constituent::runForLeaderShip (bool b) {
}

Constituent::state_t Constituent::state () const {
  return _state;
}

Constituent::mode_t Constituent::mode () const {
  return _mode;
}

bool Constituent::vote (id_t id, term_t term) {
	LOG(WARN) << _id << "," << _term << " " << id << "," << term;
	if (id == _id)
		return false;
	else {
		if (term > _term) {
			_state = FOLLOWER;
			_term = term;
			return true;
		} else {
			return false;
		}
	}
}

void Constituent::gossip (const Constituent::constituency_t& constituency) {
  // Talk to all peers i have not talked to
  // Answer by sending my complete list of peers
}

const Constituent::constituency_t& Constituent::gossip () {
  return _constituency;
}

bool Constituent::leader() const {
  return _mode==LEADER;
}

void Constituent::callElection() {
  
  LOG(WARN) << "Calling for election " << _agent->config().end_points.size();
  std::string body;
  arangodb::velocypack::Options opts;
	std::unique_ptr<std::map<std::string, std::string>> headerFields;
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());

	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) {
	  LOG(WARN) << "+++ " << i << " +++ " << _agent->config().end_points[i] ;
		results[i] = arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
      "/_api/agency/vote?id=0&term=3", std::make_shared<std::string>(body),
      headerFields, nullptr, .75*_agent->config().min_ping, true);
    LOG(WARN) << "+++ " << i << " +++ " << results[i].operationID ;
	}

/*
	std::this_thread::sleep_for(Constituent::duration_t(
	  .85*_agent->config().min_ping));

/*	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) {
	  ClusterCommResult res = arangodb::ClusterComm::instance()->
	      enquire(results[i].operationID);

	  //TRI_ASSERT(res.status == CL_COMM_SENT);
	  if (res.status == CL_COMM_SENT) {
	    res = arangodb::ClusterComm::instance()->wait("1", 1, results[i].operationID, "1");
	    //std::shared_ptr< arangodb::velocypack::Builder > answer = res.answer->toVelocyPack(&opts);
	    //LOG(WARN) << answer->toString();
	  }
  }*/
/*	for (auto& i : _agent->config().end_points) {
	  ClusterCommResult res = arangodb::ClusterComm::instance()->
	      enquire(results[i.id].operationID);
	  //LOG(WARN) << res.answer_code << "\n";
	}*/


	// time out
	//count votes
}

void Constituent::run() {
  //while (_run) {
    if (_state == FOLLOWER) {
      LOG(WARN) << "sleeping ... ";
      std::this_thread::sleep_for(sleepFor());
    } else {
      callElection();
    }
  //}
};


