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
  _gen(std::random_device()()), _role(APPRENTICE), _agent(0) {}

Constituent::~Constituent() {}

duration_t Constituent::sleepFor () {
  dist_t dis(_agent->config().min_ping, _agent->config().max_ping);
  return duration_t(dis(_gen));
}

term_t Constituent::term() const {
  return _term;
}

role_t Constituent::role () const {
  return _role;
}

void Constituent::follow(term_t term) {
  _term = term;
  _votes.assign(_votes.size(),false); // void all votes
  _role = FOLLOWER;
}

void Constituent::lead() {
  _role = LEADER;
}

void Constituent::candidate() {
  _role = CANDIDATE;
}

bool Constituent::leading () const {
  return _role == LEADER;
}

bool Constituent::following () const {
  return _role == FOLLOWER;
}

bool Constituent::running () const {
  return _role == CANDIDATE;
}

id_t Constituent::leaderID ()  const {
  return _leader_id;
}

size_t Constituent::notifyAll () {
  // Last process notifies everyone 
  std::string body;
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  path << "/_api/agency/notify?agency_size=" << _id << "&term=" << _term;
  
	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) {
    if (i != _id) {
      results[i] = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
        _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        .75*_agent->config().min_ping, true);
      LOG(WARN) << _agent->config().end_points[i];
    }
	}
}


bool Constituent::vote(term_t term, id_t leaderId, index_t prevLogIndex, term_t prevLogTerm) {
 	if (id == _id) {       // Won't vote for myself should never happen.
		return false;        // TODO: Assertion?
	} else {
	  if (term > _term) {  // Candidate with higher term: ALWAYS turn follower if not already
      switch(_role) {
      case(LEADER):      // I was leading. What happened?
        LOG(WARN) << "Cadidate with higher term. Becoming follower.";
        _agent->report(RESIGNED_LEADERSHIP_FOR_HIGHER_TERM);
        break;
      case(CANDIDATE):   // I was candidate. What happened?
        LOG(WARN) << "Cadidate with higher term. Becoming follower. Something bad has happened?";
        _agent->report(RETRACTED_CANDIDACY_FOR_HIGHER_TERM);
        break;
      default:
        break;
      }
      _cast = true;      // Note that I voted this time around.
      _leader_id = id;   // The guy I voted for I assume leader.
      follow (term);     
			return true;
		} else {             // Myself running or leading
			return false;
		}
	}
}

void Constituent::gossip (const constituency_t& constituency) {
  // TODO: Replace lame notification by gossip protocol
}

const constituency_t& Constituent::gossip () {
  // TODO: Replace lame notification by gossip protocol
  return _constituency;
}

void Constituent::callElection() {

  _votes[_id] = true; // vote for myself
  _term++;            // raise my term
  
  std::string body;
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;

  path << "/_api/agency/requestVote?term=" << _term << "&candidateId=" << _id
       << "&lastLogIndex=" << _agent.lastLogIndex() << "&lastLogTerm="
       << _agent.LastLogTerm();
    
	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) { // Ask everyone for their vote
    if (i != _id) {
      results[i] = arangodb::ClusterComm::instance()->asyncRequest("1", 1,
        _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        _agent->config().min_ping, true);
      LOG(INFO) << _agent->config().end_points[i];
    }
	}

	std::this_thread::sleep_for(.9*_agent->config().min_ping); // Wait timeout

	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) { // Collect votes
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
/*
  while (true) { 
    if (_role == FOLLOWER) { 
      _cast = false;                           // New round set not cast vote
      std::this_thread::sleep_for(sleepFor()); // Sleep for random time
      if (!_cast)
        candidate();                           // Next round, we are running
    } else {
      callElection();                          // Run for office
    }
  }
*/
};


