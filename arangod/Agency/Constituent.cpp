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
#include "Basics/ConditionLocker.h"

#include "Constituent.h"
#include "Agent.h"

#include <velocypack/Iterator.h>    
#include <velocypack/velocypack-aliases.h> 

#include <chrono>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::rest;
using namespace arangodb::velocypack;

void Constituent::configure(Agent* agent) {
  _agent = agent;
  if (size() == 1) {
    _role = LEADER;
  } else { 
    _votes.resize(size());
    _id = _agent->config().id;
    LOG(WARN) << " +++ my id is " << _id << "agency size is " << size();
    if (_agent->config().notify) {// (notify everyone) 
      notifyAll();
    }
  }
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

double Constituent::sleepFord (double min_t, double max_t) {
  dist_t dis(min_t, max_t);
  return dis(_gen);
}

term_t Constituent::term() const {
  return _term;
}

role_t Constituent::role () const {
  return _role;
}

void Constituent::follow (term_t term) {
  if (_role > FOLLOWER)
    LOG(WARN) << "Converted to follower in term " << _term ;
  _term = term;
  _votes.assign(_votes.size(),false); // void all votes
  _role = FOLLOWER;
}

void Constituent::lead () {
  if (_role < LEADER)
    LOG(WARN) << "Converted to leader in term " << _term ;
  _role = LEADER;
  _agent->lead(); // We need to rebuild spear_head and read_db;
}

void Constituent::candidate () {
  if (_role != CANDIDATE)
    LOG(WARN) << "Converted to candidate in term " << _term ;
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
  
  return size()-1;
}

bool Constituent::vote (
  term_t term, id_t leaderId, index_t prevLogIndex, term_t prevLogTerm) {

  LOG(WARN) << "term (" << term << "," << _term << ")" ;
  
 	if (leaderId == _id) {       // Won't vote for myself should never happen.
		return false;        // TODO: Assertion?
	} else {
	  if (term > _term || (_term==term&&_leader_id==leaderId)) {
      _term = term;
      _cast = true;      // Note that I voted this time around.
      _leader_id = leaderId;   // The guy I voted for I assume leader.
      if (_role>FOLLOWER)
        follow (term);
      _cv.signal();
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
  _cast = true;
  if(_role == CANDIDATE)
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

  std::this_thread::sleep_for(sleepFor(0.0, .5*_agent->config().min_ping)); // Wait timeout

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
          if (body->slice().isArray() || body->slice().isObject()) {
            for (auto const& it : VPackObjectIterator(body->slice())) {
              std::string const key(it.key.copyString());
              if (key == "term") {
                LOG(WARN) << key << " " <<it.value.getUInt();
                if (it.value.isUInt()) {
                  if (it.value.getUInt() > _term) { // follow?
                    follow(it.value.getUInt());
                    break;
                  }
                }
              } else if (key == "voteGranted") {
                if (it.value.isBool()) {
                  _votes[i] = it.value.getBool();
                }
              }
            }
          }
          LOG(WARN) << body->toJson();
        }
      } else { // Request failed
        _votes[i] = false;
      }
    }
  }

  size_t yea = 0;
  for (size_t i = 0; i < size(); ++i) {
    if (_votes[i]){
      yea++;
    }    
  }
  LOG(WARN) << "votes for me" << yea;
  if (yea > size()/2){
    lead();
  } else {
    candidate();
  }
}

void Constituent::beginShutdown() {
  Thread::beginShutdown();
}

void Constituent::run() {

  // Always start off as follower
  while (!this->isStopping() && size() > 1) { 
    if (_role == FOLLOWER) {
      bool cast;
      {
        CONDITION_LOCKER (guard, _cv);
        _cast = false;                           // New round set not cast vote
        _cv.wait(             // Sleep for random time
          sleepFord(_agent->config().min_ping, _agent->config().max_ping)*1000000);
        cast = _cast;
      }
      if (!cast) {
        candidate();                           // Next round, we are running
      }
    } else {
      callElection();                          // Run for office
    }
  }

};


