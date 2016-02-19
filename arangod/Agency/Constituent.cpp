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
  _votes.resize(_agent->config().end_points.size());
  if (_agent->config().id == (_votes.size()-1)) // Last will notify eveyone
    notifyAll();
}

Constituent::Constituent() : Thread("Constituent"), _term(0), _id(0),
  _gen(std::random_device()()), _mode(APPRENTICE), _run(true), _agent(0) {}

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

void Constituent::follow() {
  _votes.assign(_votes.size(),false); // void all votes
  _mode = FOLLOWER;
}

void Constituent::lead() {
  _mode = LEADER;
}

void Constituent::candidate() {
  _mode = CANDIDATE;
}

size_t Constituent::notifyAll () {
}

// Vote for the requester if and only if I am follower
bool Constituent::vote(id_t id, term_t term) {
	if (id == _id) {       // Won't vote for myself
		return false;
	} else {
	  if (term > _term) {  // Candidate with higher term: ALWAYS turn follower
      if (_mode > FOLLOWER) {
        LOG(WARN) << "Cadidate with higher term. Becoming follower";
      }
	    follow ();
      _term = term;      // Raise term
			return true;
		} else {             // Myself running or leading
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

  _votes[_id] = true; // vote for myself
  _term++;
  
  std::string body;
  arangodb::velocypack::Options opts;
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  path << "/_api/agency/vote?id=" << _id << "&term=" << _term;

	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) {
    if (i != _id) {
      results[i] = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
        _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        .75*_agent->config().min_ping, true);
      LOG(WARN) << _agent->config().end_points[i];
    }
	}

	std::this_thread::sleep_for(Constituent::duration_t(
	  .85*_agent->config().min_ping));

	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) {
    if (i != _id) {
      ClusterCommResult res = arangodb::ClusterComm::instance()->
	      enquire(results[i].operationID);
      
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
        LOG(WARN) << body->toString();
      } else { // Request failed
        _votes[i] = false;
      }
    }
  }
}

void Constituent::run() {
  while (true) {
    LOG(WARN) << _mode;
    if (_mode == FOLLOWER || _mode == APPRENTICE) {
      LOG(WARN) << "sleeping ... ";
      std::this_thread::sleep_for(sleepFor());
    } else {
      LOG(WARN) << "calling ... ";
      callElection();
    }
  }
};


