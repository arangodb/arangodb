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

#include <velocypack/Iterator.h>    
#include <velocypack/velocypack-aliases.h> 

#include <chrono>
#include <iostream>

using namespace arangodb::velocypack;

namespace arangodb {
namespace consensus {

Agent::Agent () : Thread ("Agent"), _stopping(false) {}

Agent::Agent (config_t const& config) : Thread ("Agent"), _config(config) {
  _state.setEndPoint(_config.end_points[this->id()]);
  _constituent.configure(this);
  _confirmed.resize(size(),0);
}

id_t Agent::id() const { return _config.id;}

Agent::~Agent () {
//  shutdown();
}

State const& Agent::state() const {
  return _state;
}

bool Agent::start() {
  _constituent.start();
  std::cout << "constituent started" << std::endl;
  _spear_head.start();
  std::cout << "spear_head started" << std::endl;
  Thread::start();
  return true;
}

term_t Agent::term () const {
  return _constituent.term();
}

inline size_t Agent::size() const {
  return _config.size();
}

priv_rpc_ret_t Agent::requestVote(term_t t, id_t id, index_t lastLogIndex,
                                  index_t lastLogTerm, query_t const& query) {

  if (query != nullptr) { // record new endpoints
    if (query->slice().hasKey("endpoints") && 
        query->slice().get("endpoints").isArray()) {
      size_t j = 0;
      for (auto const& i : VPackArrayIterator(query->slice().get("endpoints"))) {
        _config.end_points[j++] = i.copyString();
      }
    }
  }
    
  return priv_rpc_ret_t(  // vote
    _constituent.vote(t, id, lastLogIndex, lastLogTerm), this->term());
}

config_t const& Agent::config () const {
  return _config;
}

void Agent::report(status_t status) {
  //_status = status;
}

id_t Agent::leaderID () const {
  return _constituent.leaderID();
}

bool Agent::leading() const {
  return _constituent.leading();
}

bool Agent::waitFor (index_t index, duration_t timeout) {

  if (size() == 1) // single host agency
    return true;
    
  CONDITION_LOCKER(guard, _rest_cv);
  auto start = std::chrono::system_clock::now();
  
  while (true) {

    _rest_cv.wait();

    // shutting down
    if (this->isStopping()) {      
      return false;
    }
    // timeout?
    if (std::chrono::system_clock::now() - start > timeout) {
      return false;
    }
    if (_last_commit_index >= index) {
      return true;
    }
  }
  // We should never get here
  TRI_ASSERT(false);
}

void Agent::reportIn (id_t id, index_t index) {
  MUTEX_LOCKER(mutexLocker, _confirmedLock);

  if (index > _confirmed[id])      // progress this follower?
    _confirmed[id] = index;

  if(index > _last_commit_index) { // progress last commit?
    size_t n = 0;
    for (size_t i = 0; i < size(); ++i) {
      n += (_confirmed[i]>=index);
    }

    if (n>size()/2) { // catch up read database and commit index
      _read_db.apply(_state.get(_last_commit_index+1, index));
      _last_commit_index = index;
    }
  }

  _rest_cv.broadcast();            // wake up REST handlers
}

bool Agent::recvAppendEntriesRPC (term_t term, id_t leaderId, index_t prevIndex,
  term_t prevTerm, index_t leaderCommitIndex, query_t const& queries) {
  //Update commit index
  _last_commit_index = leaderCommitIndex;

  // Sanity
  if (this->term() > term)
    throw LOWER_TERM_APPEND_ENTRIES_RPC; // (§5.1)
  if (!_state.findit(prevIndex, prevTerm))
    throw NO_MATCHING_PREVLOG;           // (§5.3)
  
  // Delete conflits and append (§5.3)
  _state.log (queries, term, leaderId, prevIndex, prevTerm);
  
  return true;
}

append_entries_t Agent::sendAppendEntriesRPC (
  id_t slave_id, collect_ret_t const& entries) {

  // RPC path
  std::stringstream path;
  path << "/_api/agency_priv/appendEntries?term=" << term() << "&leaderId="
       << id() << "&prevLogIndex=" << entries.prev_log_index << "&prevLogTerm="
       << entries.prev_log_term << "&leaderCommit=" << _last_commit_index;

  // Headers
	std::unique_ptr<std::map<std::string, std::string>> headerFields =
	  std::make_unique<std::map<std::string, std::string> >();

  // Body
  Builder builder;
  builder.add(VPackValue(VPackValueType::Array));
  index_t last;
  for (size_t i = 0; i < entries.size(); ++i) {
    builder.add (VPackValue(VPackValueType::Object));
    builder.add ("index", Value(std::to_string(entries.indices[i])));
    builder.add ("query", Builder(*_state[entries.indices[i]].entry).slice());
    builder.close();
    last = entries.indices[i];
  }
  builder.close();

  // Send
  arangodb::ClusterComm::instance()->asyncRequest
    ("1", 1, _config.end_points[slave_id],
     rest::HttpRequest::HTTP_REQUEST_POST,
     path.str(), std::make_shared<std::string>(builder.toString()), headerFields,
     std::make_shared<AgentCallback>(this, slave_id, last),
     0, true);

  return append_entries_t(this->term(), true);
  
}

bool Agent::load () {
  
  LOG(INFO) << "Loading persistent state.";
  if (!_state.load())
    LOG(FATAL) << "Failed to load persistent state on statup.";
  return true;
}

write_ret_t Agent::write (query_t const& query)  {

  if (_constituent.leading()) {                    // Leading 
    MUTEX_LOCKER(mutexLocker, _confirmedLock);
    std::vector<bool> applied = _spear_head.apply(query); // Apply to spearhead
    std::vector<index_t> indices = 
      _state.log (query, applied, term(), id()); // Append to log w/ indicies
    for (size_t i = 0; i < applied.size(); ++i) {
      if (applied[i]) {
        _confirmed[id()] = indices[i];           // Confirm myself
      }
    }
    _cv.signal();                                // Wake up run
    return write_ret_t(true,id(),applied,indices); // Indices to wait for to rest
  } else {                                       // Leading else redirect
    return write_ret_t(false,_constituent.leaderID());
  }
}

read_ret_t Agent::read (query_t const& query) const {
  if (_constituent.leading()) {     // We are leading
    auto result = (_config.size() == 1) ?
      _spear_head.read(query) : _read_db.read (query);
    return read_ret_t(true,_constituent.leaderID(),result);//(query); //TODO:
  } else {                          // We redirect
    return read_ret_t(false,_constituent.leaderID());
  }
}

void Agent::run() {

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping()) {

    if (leading())
      _cv.wait(10000000);
    else
      _cv.wait();

    std::vector<collect_ret_t> work(size());
    
    // Collect all unacknowledged
    for (size_t i = 0; i < size(); ++i) {
      if (i != id()) {
        work[i] = _state.collectFrom(_confirmed[i]+1);
      }
    }
    
    // (re-)attempt RPCs
    for (size_t j = 0; j < size(); ++j) {
      if (j != id() && work[j].size()) {
        sendAppendEntriesRPC(j, work[j]);
      }
    }
    
  }

}

void Agent::beginShutdown() {
  Thread::beginShutdown();
  _constituent.beginShutdown();
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

bool Agent::lead () {
  rebuildDBs();
  _cv.signal();
  return true;
}

bool Agent::rebuildDBs() {
  MUTEX_LOCKER(mutexLocker, _dbLock);
  return true;
}

log_t const& Agent::lastLog() const {
  return _state.lastLog();
}


}}
