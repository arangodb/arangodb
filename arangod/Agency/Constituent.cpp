
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
#include "Logger/Logger.h"
#include "Basics/ConditionLocker.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/collection.h"
#include "VocBase/vocbase.h"

#include "Constituent.h"
#include "Agent.h"

#include <velocypack/Iterator.h>    
#include <velocypack/velocypack-aliases.h> 

#include <chrono>
#include <iomanip>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::rest;
using namespace arangodb::velocypack;
using namespace arangodb;

// Configure with agent's configuration
void Constituent::configure(Agent* agent) {

  _agent = agent;

  if (size() == 1) {
    _role = LEADER;
  } else {
    _id = _agent->config().id;
    if (_agent->config().notify) {// (notify everyone) 
      notifyAll();
    }
  }
  
}

// Default ctor
Constituent::Constituent() :
  Thread("Constituent"),
  _vocbase(nullptr),
  _applicationV8(nullptr),
  _queryRegistry(nullptr),
  _term(0),
  _leader_id(0),
  _id(0),
  _gen(std::random_device()()),
  _role(FOLLOWER),
  _agent(0),
  _voted_for(0) {}

// Shutdown if not already
Constituent::~Constituent() {
  shutdown();
}

// Configuration
config_t const& Constituent::config () const {
  return _agent->config();
}

// Wait for sync
bool Constituent::waitForSync() const {
  return _agent->config().wait_for_sync;
}

// Random sleep times in election process
duration_t Constituent::sleepFor (double min_t, double max_t) {
  dist_t dis(min_t, max_t);
  return duration_t((long)std::round(dis(_gen)*1000.0));
}

// Get my term
term_t Constituent::term() const {
  return _term;
}

// Update my term
void Constituent::term(term_t t) {

  if (_term != t) {

    _term  = t;

    LOG_TOPIC(INFO, Logger::AGENCY) << "Updating term to " << t;

    Builder body;
    body.add(VPackValue(VPackValueType::Object));
    std::ostringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << _term;
    body.add("_key", Value(i_str.str()));
    body.add("term", Value(_term));
    body.add("voted_for", Value((uint32_t)_voted_for));
    body.close();
    
    TRI_ASSERT(_vocbase != nullptr);
    auto transactionContext =
      std::make_shared<StandaloneTransactionContext>(_vocbase);
    SingleCollectionTransaction trx(transactionContext, "election",
                                    TRI_TRANSACTION_WRITE);
    
    int res = trx.begin();
    
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    
    OperationOptions options;
    options.waitForSync = waitForSync(); 
    options.silent = true;
    
    OperationResult result = trx.insert("election", body.slice(), options);
    res = trx.finish(result.code);
    
  }
  
}

/// @brief My role
role_t Constituent::role () const {
  return _role;
}

/// @brief Become follower in term 
void Constituent::follow (term_t t) {
  if (_role != FOLLOWER) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Role change: Converted to follower in term " << t;
  }
  this->term(t);
  _role = FOLLOWER;
}

/// @brief Become leader
void Constituent::lead () {
  if (_role != LEADER) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Role change: Converted to leader in term " << _term ;
    _agent->lead(); // We need to rebuild spear_head and read_db;
  }
  _role = LEADER;
  _leader_id = _id;
}

/// @brief Become follower
void Constituent::candidate () {
  if (_role != CANDIDATE)
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Role change: Converted to candidate in term " << _term ;
  _role = CANDIDATE;
}

/// @brief Leading?
bool Constituent::leading () const {
  return _role == LEADER;
}

/// @brief Following?
bool Constituent::following () const {
  return _role == FOLLOWER;
}

/// @brief Runnig as candidate?
bool Constituent::running () const {
  return _role == CANDIDATE;
}

/// @brief Get current leader's id
id_t Constituent::leaderID ()  const {
  return _leader_id;
}

/// @brief Agency size
size_t Constituent::size() const {
  return config().size();
}

/// @brief Get endpoint to an id 
std::string const& Constituent::end_point(id_t id) const {
  return config().end_points[id];
}

/// @brief Get all endpoints
std::vector<std::string> const& Constituent::end_points() const {
  return config().end_points;
}

/// @brief Notify peers of updated endpoints
size_t Constituent::notifyAll () {

  // Last process notifies everyone 
  std::stringstream path;
  
  path << "/_api/agency_priv/notifyAll?term=" << _term << "&agencyId=" << _id;

  // Body contains endpoints list
  Builder body;
  body.openObject();
  body.add("endpoints", VPackValue(VPackValueType::Array));
  for (auto const& i : end_points()) {
    body.add(Value(i));
  }
  body.close();
  body.close();
  
  // Send request to all but myself
  for (id_t i = 0; i < size(); ++i) {
    if (i != _id) {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, end_point(i), GeneralRequest::RequestType::POST, path.str(),
        std::make_shared<std::string>(body.toString()), headerFields, nullptr,
        0.0, true);
    }
  }
  
  return size()-1;
}

/// @brief Vote
bool Constituent::vote (
  term_t term, id_t id, index_t prevLogIndex, term_t prevLogTerm) {
  if (term > _term || (_term==term&&_leader_id==id)) {
    this->term(term);
    _cast = true;      // Note that I voted this time around.
    _voted_for = id;   // The guy I voted for I assume leader.
    _leader_id = id;
    if (_role>FOLLOWER)
      follow (_term);
    _agent->persist(_term,_voted_for);
    _cv.signal();
    return true;
  } else {             // Myself running or leading
    return false;
  }
}

/// @brief Implementation of a gossip protocol
void Constituent::gossip (const constituency_t& constituency) {
  // TODO: Replace lame notification by gossip protocol
}

/// @brief Implementation of a gossip protocol
const constituency_t& Constituent::gossip () {
  // TODO: Replace lame notification by gossip protocol
  return _constituency;
}

/// @brief Call to election
void Constituent::callElection() {

  std::vector<bool> votes(size(),false);

  votes.at(_id) = true; // vote for myself
  _cast = true;
  if(_role == CANDIDATE) {
    this->term(_term+1);            // raise my term
  }
  
  std::string body;
  std::vector<ClusterCommResult> results(config().end_points.size());
  std::stringstream path;
  
  path << "/_api/agency_priv/requestVote?term=" << _term << "&candidateId="
       << _id << "&prevLogIndex=" << _agent->lastLog().index << "&prevLogTerm="
       << _agent->lastLog().term;

  // Ask everyone for their vote
  for (id_t i = 0; i < config().end_points.size(); ++i) { 
    if (i != _id && end_point(i) != "") {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      results[i] = arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, config().end_points[i], GeneralRequest::RequestType::GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        config().min_ping, true);
    }
  }

  // Wait randomized timeout
  std::this_thread::sleep_for(
    sleepFor(.5*config().min_ping, .8*config().min_ping));

  // Collect votes
  for (id_t i = 0; i < config().end_points.size(); ++i) { 
    if (i != _id && end_point(i) != "") {
      ClusterCommResult res = arangodb::ClusterComm::instance()->
        enquire(results[i].operationID);
      
      if (res.status == CL_COMM_SENT) { // Request successfully sent 
        res = arangodb::ClusterComm::instance()->wait(
          "1", 1, results[i].operationID, "1");
        std::shared_ptr<Builder > body = res.result->getBodyVelocyPack();
        if (body->isEmpty()) {                                     // body empty
          continue;
        } else {
          if (body->slice().isObject()) {                          // body 
            VPackSlice slc = body->slice();
            if (slc.hasKey("term") && slc.hasKey("voteGranted")) { // OK
              term_t t = slc.get("term").getUInt();
              if (t > _term) {                                     // follow?
                follow(t);
                break;
              }
              votes[i] = slc.get("voteGranted").getBool();        // Get vote
            } 
          }
        }
      } else { // Request failed
        votes[i] = false;
      }
    }
  }

  // Count votes
  size_t yea = 0;
  for (size_t i = 0; i < size(); ++i) {
    if (votes[i]){
      yea++;
    }    
  }

  // Evaluate election results
  if (yea > size()/2){
    lead();
  } else {
    follow(_term);
  }
}

void Constituent::beginShutdown() {
  Thread::beginShutdown();
}


bool Constituent::start (TRI_vocbase_t* vocbase,
                         ApplicationV8* applicationV8,
                         aql::QueryRegistry* queryRegistry) {

  _vocbase = vocbase;
  _applicationV8 = applicationV8;
  _queryRegistry = queryRegistry;

  return Thread::start();
}


void Constituent::run() {

  TRI_ASSERT(_vocbase != nullptr);
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();
  
  TRI_ASSERT(_applicationV8 != nullptr);
  TRI_ASSERT(_queryRegistry != nullptr);

  // Query
  std::string const aql ("FOR l IN election SORT l._key DESC LIMIT 1 RETURN l");
  arangodb::aql::Query query(_applicationV8, false, _vocbase,
                             aql.c_str(), aql.size(), bindVars, nullptr,
                             arangodb::aql::PART_MAIN);
  
  auto queryResult = query.execute(_queryRegistry);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }
  
  VPackSlice result = queryResult.result->slice();
  
  if (result.isArray()) {
    for (auto const& i : VPackArrayIterator(result)) {
      try {
        _term = i.get("term").getUInt();
        _voted_for = i.get("voted_for").getUInt();
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Persisted election entries corrupt! Defaulting term,vote (0,0)";
      }
    }
  }
  
  // Always start off as follower
  while (!this->isStopping() && size() > 1) { 
    if (_role == FOLLOWER) {
      _cast = false;                           // New round set not cast vote
      std::this_thread::sleep_for(             // Sleep for random time
        sleepFor(config().min_ping, config().max_ping)); 
      if (!_cast) {
        candidate();                           // Next round, we are running
      }
    } else {
      callElection();                          // Run for office
    }
  }
  
}


