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
using namespace arangodb::velocypack;

void Constituent::configure(Agent* agent) {
  _agent = agent;
  _votes.resize(size());
  _id = _agent->config().id;
  LOG(WARN) << " +++ my id is " << _id << "agency size is " << size();
  if (_id == (size()-1)) // Last will (notify everyone)
    notifyAll(); 
}

Constituent::Constituent() : Thread("Constituent"), _term(0), _id(0),
  _gen(std::random_device()()), _role(FOLLOWER), _agent(0) {}

Constituent::~Constituent() {
  shutdown();
}

duration_t Constituent::sleepFor (double min_t, double max_t) {
  dist_t dis(min_t, max_t);
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
  _agent->lead(); // We need to rebuild spear_head and read_db;
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

size_t Constituent::size() const {
  return _agent->config().size();
}

std::string const& Constituent::end_point(id_t id) const {
  return _agent->config().end_points[id];
}

std::vector<std::string> const& Constituent::end_points() const {
  return _agent->config().end_points;
}

size_t Constituent::notifyAll () {

  // Last process notifies everyone 
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  
  path << "/_api/agency_priv/notifyAll?term=" << _term << "&agencyId=" << _id;

  // Body contains endpoints
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  for (auto const& i : end_points()) {
    body.add("endpoint", Value(i));
  }
  body.close();
  LOG(INFO) << body.toString();

  // Send request to all but myself
	for (size_t i = 0; i < size(); ++i) {
    if (i != _id) {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      LOG(INFO) << i << " notify " << end_point(i) << path.str() ;
      results[i] = arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, end_point(i), rest::HttpRequest::HTTP_REQUEST_POST, path.str(),
        std::make_shared<std::string>(body.toString()), headerFields, nullptr,
        0.0, true);
    }
	}
}

bool Constituent::vote (
  term_t term, id_t leaderId, index_t prevLogIndex, term_t prevLogTerm) {
 	if (leaderId == _id) {       // Won't vote for myself should never happen.
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
      _leader_id = leaderId;   // The guy I voted for I assume leader.
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
	std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;

  path << "/_api/agency_priv/requestVote?term=" << _term << "&candidateId=" << _id
       << "&prevLogIndex=" << _agent->lastLog().index << "&prevLogTerm="
       << _agent->lastLog().term;
  
	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) { // Ask everyone for their vote
    if (i != _id && end_point(i) != "") {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      results[i] = arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, _agent->config().end_points[i], rest::HttpRequest::HTTP_REQUEST_GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        _agent->config().min_ping, true);
      LOG(INFO) << _agent->config().end_points[i];
    }
	}

	std::this_thread::sleep_for(sleepFor(0., .9*_agent->config().min_ping)); // Wait timeout

	for (size_t i = 0; i < _agent->config().end_points.size(); ++i) { // Collect votes
    if (i != _id && end_point(i) != "") {
      ClusterCommResult res = arangodb::ClusterComm::instance()->
	      enquire(results[i].operationID);
      
      if (res.status == CL_COMM_SENT) { // Request successfully sent 
        res = arangodb::ClusterComm::instance()->wait("1", 1, results[i].operationID, "1");
        std::shared_ptr<Builder > body = res.result->getBodyVelocyPack();
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

void Constituent::beginShutdown() {
  Thread::beginShutdown();
}

void Constituent::run() {

  // Always start off as follower
  while (!this->isStopping()) { 
    if (_role == FOLLOWER) { 
      _cast = false;                           // New round set not cast vote
      std::this_thread::sleep_for(             // Sleep for random time
        sleepFor(_agent->config().min_ping, _agent->config().max_ping));
      if (!_cast)
        candidate();                           // Next round, we are running
    } else {
      callElection();                          // Run for office
    }
  }

};


