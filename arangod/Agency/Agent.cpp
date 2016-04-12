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

#include "Agent.h"
#include "Basics/ConditionLocker.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>    
#include <velocypack/velocypack-aliases.h> 

#include <chrono>
#include <iostream>

using namespace arangodb::velocypack;

namespace arangodb {
namespace consensus {

//  Agent configuration
Agent::Agent (TRI_server_t* server, config_t const& config, ApplicationV8* applicationV8, aql::QueryRegistry* queryRegistry) 
    : Thread ("Agent"), 
      _server(server), 
      _vocbase(nullptr), 
      _applicationV8(applicationV8), 
      _queryRegistry(queryRegistry), 
      _config(config), 
      _last_commit_index(0) {

  _state.setEndPoint(_config.end_point);
  _constituent.configure(this);
  _confirmed.resize(size(),0); // agency's size and reset to 0
}

//  This agent's id
id_t Agent::id() const {
  return _config.id;
}

//  Shutdown
Agent::~Agent () {
  shutdown();
}

//  State machine
State const& Agent::state () const {
  return _state;
}

//  Start all agent thread
bool Agent::start() {
  LOG_TOPIC(INFO, Logger::AGENCY) << "Starting agency comm worker.";
  Thread::start();
  return true;
}

//  This agent's term
term_t Agent::term () const {
  return _constituent.term();
}

//  Agency size
inline size_t Agent::size() const {
  return _config.size();
}

//  Handle vote request
priv_rpc_ret_t Agent::requestVote(term_t t, id_t id, index_t lastLogIndex,
                                  index_t lastLogTerm, query_t const& query) {

  /// Are we receiving new endpoints
  if (query != nullptr) { // record new endpoints
    if (query->slice().hasKey("endpoints") && 
        query->slice().get("endpoints").isArray()) {
      size_t j = 0;
      for (auto const& i : VPackArrayIterator(query->slice().get("endpoints"))) {
        _config.end_points[j++] = i.copyString();
      }
    }
  }

  /// Constituent handles this
  return priv_rpc_ret_t( 
    _constituent.vote(t, id, lastLogIndex, lastLogTerm), this->term());
  
}

//  Get configuration
config_t const& Agent::config () const {
  return _config;
}

//  Leader's id
id_t Agent::leaderID () const {
  return _constituent.leaderID();
}

// Are we leading?
bool Agent::leading() const {
  return _constituent.leading();
}

// Persist term and id we vote for
void Agent::persist(term_t t, id_t i) {
//  _state.persist(t, i);
}

// Waits here for confirmation of log's commits up to index.
// Timeout in seconds
bool Agent::waitFor (index_t index, double timeout) {

  if (size() == 1) // single host agency
    return true;
    
  CONDITION_LOCKER(guard, _rest_cv);

  // Wait until woken up through AgentCallback 
  while (true) {

    std::cout << _last_commit_index << std::endl;
    /// success?
    if (_last_commit_index >= index) {
      return true;
    }
    // timeout
    if (_rest_cv.wait(static_cast<uint64_t>(1.0e6*timeout))) {
      return false;
    }
    
    // shutting down
    if (this->isStopping()) {      
      return false;
    }

  }
  // We should never get here
  TRI_ASSERT(false);
}

//  AgentCallback reports id of follower and its highest processed index
void Agent::reportIn (id_t id, index_t index) {
  MUTEX_LOCKER(mutexLocker, _ioLock);

  if (index > _confirmed[id])      // progress this follower?
    _confirmed[id] = index;
  
  if(index > _last_commit_index) { // progress last commit?
    size_t n = 0;
    for (size_t i = 0; i < size(); ++i) {
      n += (_confirmed[i]>=index);
    }
    if (n>size()/2) { // catch up read database and commit index
      LOG_TOPIC(INFO, Logger::AGENCY) << "Critical mass for commiting " <<
        _last_commit_index+1 << " through " << index << " to read db";
      
      _read_db.apply(_state.slices(_last_commit_index+1, index));
      _last_commit_index = index;
    }
  }

  CONDITION_LOCKER(guard, _rest_cv);
  _rest_cv.broadcast();            // wake up REST handlers
}

//  Followers' append entries
bool Agent::recvAppendEntriesRPC (term_t term, id_t leaderId, index_t prevIndex,
  term_t prevTerm, index_t leaderCommitIndex, query_t const& queries) {
  //Update commit index

  if (queries->slice().type() != VPackValueType::Array) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Received malformed entries for appending. Discarting!";  
    return false;
  }

  MUTEX_LOCKER(mutexLocker, _ioLock);

  index_t last_commit_index = _last_commit_index;
  // 1. Reply false if term < currentTerm (§5.1)
  if (this->term() > term) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "I have a higher term than RPC caller.";
    return false;
  }

  // 2. Reply false if log doesn’t contain an entry at prevLogIndex
  //    whose term matches prevLogTerm (§5.3)
  if (!_state.find(prevIndex,prevTerm)) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Unable to find matching entry to previous entry (index,term) = ("
      << prevIndex << "," << prevTerm << ")";
    //return false;
  }

  // 3. If an existing entry conflicts with a new one (same index
  //    but different terms), delete the existing entry and all that
  //    follow it (§5.3)
  // 4. Append any new entries not already in the log
  if (queries->slice().length()) {
    LOG_TOPIC(INFO, Logger::AGENCY) << "Appending "<< queries->slice().length()
                                    << " entries to state machine.";
    /* bool success = */_state.log (queries, term, leaderId, prevIndex, prevTerm);
  } else { 
    // heart-beat
  }
  
  // appendEntries 5. If leaderCommit > commitIndex, set commitIndex =
  //min(leaderCommit, index of last new entry)
  if (leaderCommitIndex > last_commit_index) {
    _last_commit_index = (std::min)(leaderCommitIndex,last_commit_index);
  }

  return true;

}

// Leader's append entries
append_entries_t Agent::sendAppendEntriesRPC (id_t follower_id) {

  index_t last_confirmed = _confirmed[follower_id];
  std::vector<log_t> unconfirmed = _state.get(last_confirmed);

  MUTEX_LOCKER(mutexLocker, _ioLock);

  term_t t = this->term();

  // RPC path
  std::stringstream path;
  path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId="
       << id() << "&prevLogIndex=" << unconfirmed[0].index << "&prevLogTerm="
       << unconfirmed[0].term << "&leaderCommit=" << _last_commit_index;

  // Headers
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();

  // Body
  Builder builder;
  index_t last = unconfirmed[0].index;
  builder.add(VPackValue(VPackValueType::Array));
  for (size_t i = 1; i < unconfirmed.size(); ++i) {
    builder.add (VPackValue(VPackValueType::Object));
    builder.add ("index", VPackValue(unconfirmed[i].index));
    builder.add ("query", VPackSlice(unconfirmed[i].entry->data()));
    builder.close();
    last = unconfirmed[i].index;
  }
  builder.close();

  // Send request
  if (unconfirmed.size() > 1) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Appending " << unconfirmed.size()-1 << " entries up to index " << last
      << " to follower " << follower_id;
  }

  arangodb::ClusterComm::instance()->asyncRequest
    ("1", 1, _config.end_points[follower_id],
     arangodb::GeneralRequest::RequestType::POST,
     path.str(), std::make_shared<std::string>(builder.toJson()), headerFields,
     std::make_shared<AgentCallback>(this, follower_id, last),
     0, true);

  return append_entries_t(t, true);
  
}

// @brief load persistent state
bool Agent::load () {
  TRI_vocbase_t* vocbase =
      TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    LOG(FATAL) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  _vocbase = vocbase;

  LOG_TOPIC(INFO, Logger::AGENCY) << "Loading persistent state.";
  if (!_state.loadCollections(_vocbase, _applicationV8, _queryRegistry)) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Failed to load persistent state on statup.";
  }

  LOG_TOPIC(INFO, Logger::AGENCY) << "Reassembling spearhead and read stores.";
//  _read_db.apply(_state.slices());
  _spearhead.apply(_state.slices(_last_commit_index+1));

  LOG_TOPIC(INFO, Logger::AGENCY) << "Starting spearhead worker.";
  _spearhead.start(this);
  _read_db.start(this);

  LOG_TOPIC(INFO, Logger::AGENCY) << "Starting constituent personality.";
  _constituent.start(_vocbase, _applicationV8, _queryRegistry);
  
  return true;
}

// Write new entries to replicated state and store
write_ret_t Agent::write (query_t const& query)  {

  if (_constituent.leading()) {                    // Only working as leader

    std::vector<bool> applied;
    std::vector<index_t> indices;
    index_t maxind = 0;

    {
      MUTEX_LOCKER(mutexLocker, _ioLock);
      applied = _spearhead.apply(query);             // Apply to spearhead
      indices = _state.log (query, applied, term(), id()); // Log w/ indicies
      if (!indices.empty()) {
        maxind = *std::max_element(indices.begin(), indices.end());
      }
      _cv.signal();                                  // Wake up run
    }

    reportIn(0,maxind);
    
    return write_ret_t(true,id(),applied,indices); // Indices to wait for to rest
    
  } else {                                         // Else we redirect
    return write_ret_t(false,_constituent.leaderID());
  }
}

// Read from store
read_ret_t Agent::read (query_t const& query) const {
  if (_constituent.leading()) {     // Only working as leaer
    query_t result = std::make_shared<arangodb::velocypack::Builder>();
    std::vector<bool> success = _read_db.read (query, result);
    return read_ret_t(true, _constituent.leaderID(), success, result);
  } else {                          // Else We redirect
    return read_ret_t(false, _constituent.leaderID());
  }
}

// Repeated append entries
void Agent::run() {

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping()) {

    if (leading())
      _cv.wait(250000); // Only if leading
    else
      _cv.wait();       // Just sit there doing nothing

    // Collect all unacknowledged
    for (id_t i = 0; i < size(); ++i) {
      if (i != id()) {
        sendAppendEntriesRPC(i);
      }
    }
  }

}

// Orderly shutdown
void Agent::beginShutdown() {

  // Personal hygiene
  Thread::beginShutdown();
  
  // Stop constituent and key value stores
  _constituent.beginShutdown();
  _spearhead.beginShutdown();
  _read_db.beginShutdown();
  
  // Wake up all waiting REST handler (waitFor)
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
  
  if (_vocbase != nullptr) {
    TRI_ReleaseDatabaseServer(_server, _vocbase);
  }
  
}

// Becoming leader 
bool Agent::lead () {

  // Key value stores
  rebuildDBs();

  // Wake up run
  _cv.signal();
  
  return true;
}

// Rebuild key value stores
bool Agent::rebuildDBs() {
  MUTEX_LOCKER(mutexLocker, _ioLock);
  _spearhead.apply(_state.slices());
  _read_db.apply(_state.slices());
  return true;
}

// Last log entry
log_t const& Agent::lastLog() const {
  return _state.lastLog();
}

// Get spearhead
Store const& Agent::spearhead () const {
  return _spearhead;
}

// Get readdb
Store const& Agent::readDB () const {
  return _read_db;
}

}}
