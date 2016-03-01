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

#include <chrono>

using namespace arangodb::velocypack;
namespace arangodb {
namespace consensus {

Agent::Agent () : Thread ("Agent"), _stopping(false) {}

Agent::Agent (config_t const& config) : _config(config), Thread ("Agent") {
  _constituent.configure(this);
  _state.load(); // read persistent database
}

id_t Agent::id() const { return _config.id;}

Agent::~Agent () {}

void Agent::start() {
  _constituent.start();
}

term_t Agent::term () const {
  return _constituent.term();
}

inline size_t Agent::size() const {
  return _config.size();
}

query_t Agent::requestVote(term_t t, id_t id, index_t lastLogIndex,
                           index_t lastLogTerm) {
  Builder builder;
  builder.add("term", Value(term()));
  builder.add("voteGranted", Value(
                _constituent.vote(id, t, lastLogIndex, lastLogTerm)));
  builder.close();
	return std::make_shared<Builder>(builder);
}

Config<double> const& Agent::config () const {
  return _config;
}

void Agent::print (arangodb::LoggerStream& logger) const {
  //logger << _config;
}

void Agent::report(status_t status) {
  //_status = status;
}

id_t Agent::leaderID () const {
  return _constituent.leaderID();
}

arangodb::LoggerStream& operator<< (arangodb::LoggerStream& l, Agent const& a) {
  a.print(l);
  return l;
}

void Agent::catchUpReadDB() {}; // TODO

bool Agent::waitFor (index_t index, duration_t timeout) {

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
    if (_last_commit_index > index)
      return true;
  }
  // We should never get here
  TRI_ASSERT(false);
}

void Agent::reportIn (id_t id, index_t index) {
  MUTEX_LOCKER(mutexLocker, _confirmedLock);
  if (index > _confirmed[id])
    _confirmed[id] = index;
  // last commit index smaller?
  // check if we can move forward
  if(_last_commit_index < index) {
    size_t n = 0;
    for (size_t i = 0; i < size(); ++i) {
      n += (_confirmed[i]>index);
    }
    if (n>size()/2) {
      _last_commit_index = index;
    }
  }
  _rest_cv.broadcast();
}

bool Agent::recvAppendEntriesRPC (term_t term, id_t leaderId, index_t prevIndex,
  term_t prevTerm, index_t leaderCommitIndex, query_t const& queries) {

  // Update commit index
  _last_commit_index = leaderCommitIndex;

  // Sanity
  if (this->term() > term)
    throw LOWER_TERM_APPEND_ENTRIES_RPC; // (§5.1)
  if (!_state.findit(prevIndex, prevTerm))
    throw NO_MATCHING_PREVLOG; // (§5.3)

  // Delete conflits and append (§5.3)
  for (size_t i = 0; i < queries->slice().length()/2; i+=2) {
    _state.log (queries->slice()[i  ].toString(),
                queries->slice()[i+1].getUInt(), term, leaderId);
  }
  
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
  for (size_t i = 0; i < entries.size(); ++i) {
    builder.add ("index", Value(std::to_string(entries.indices[i])));
    builder.add ("query", Value(_state[entries.indices[i]].entry));
  }
  builder.close();

  // Send 
  arangodb::ClusterComm::instance()->asyncRequest
    ("1", 1, _config.end_points[slave_id],
     rest::HttpRequest::HTTP_REQUEST_GET,
     path.str(), std::make_shared<std::string>(builder.toString()), headerFields,
     std::make_shared<arangodb::ClusterCommCallback>(_agent_callback),
     1.0, true);
}

//query_ret_t
write_ret_t Agent::write (query_t const& query)  { // Signal auf die _cv
  if (_constituent.leading()) {                    // We are leading
    if (true/*_spear_head.apply(query)*/) {            // We could apply to spear head?
      std::vector<index_t> indices =               //    otherwise through
        _state.log (query, term(), id()); // Append to my own log
      {
        MUTEX_LOCKER(mutexLocker, _confirmedLock);
        _confirmed[id()]++;
      }
      return write_ret_t(true,id(),indices); // indices
    } else {
      throw QUERY_NOT_APPLICABLE;
    }
  } else {                          // We redirect
    return write_ret_t(false,_constituent.leaderID());
  }
}

read_ret_t Agent::read (query_t const& query) const {
  if (_constituent.leading()) {     // We are leading
    return read_ret_t(true,_constituent.leaderID());//(query); //TODO:
  } else {                          // We redirect
    return read_ret_t(false,_constituent.leaderID());
  }
}

void Agent::run() {

  CONDITION_LOCKER(guard, _cv);
  
  while (!this->isStopping()) {
    
    _cv.wait();
    auto dur = std::chrono::system_clock::now();
    std::vector<collect_ret_t> work(size());
    
    // Collect all unacknowledged
    for (size_t i = 0; i < size(); ++i) {
      if (i != id()) {
        work[i] = _state.collectFrom(_confirmed[i]);
      }
    }
    
    // (re-)attempt RPCs
    for (size_t j = 0; j < size(); ++j) {
      if (j != id() && work[j].size()) {
        sendAppendEntriesRPC(j, work[j]);
      }
    }
    
    // catch up read db
    catchUpReadDB();
    
  }
}

void Agent::beginShutdown() {
  Thread::beginShutdown();
  // Stop callbacks
  _agent_callback.shutdown();
  // wake up all blocked rest handlers
  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}


}}
