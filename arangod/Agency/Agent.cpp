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

using namespace arangodb::velocypack;
namespace arangodb {
namespace consensus {

Agent::Agent () : Thread ("Agent"){}

Agent::Agent (config_t const& config) : _config(config), Thread ("Agent") {
  //readPersistence(); // Read persistence (log )
  _constituent.configure(this);
  _state.configure(this);
}

id_t Agent::id() const { return _config.id;}

Agent::~Agent () {}

void Agent::start() {
  _constituent.start();
}

term_t Agent::term () const {
  return _constituent.term();
}

query_t Agent::requestVote(term_t t, id_t id, index_t lastLogIndex, index_t lastLogTerm) {
  Builder builder;
  builder.add("term", Value(term()));
  builder.add("voteGranted", Value(_constituent.vote(id, t, lastLogIndex, lastLogTerm)));
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
  _status = status;
}

id_t Agent::leaderID () const {
  return _constituent.leaderID();
}

arangodb::LoggerStream& operator<< (arangodb::LoggerStream& l, Agent const& a) {
  a.print(l);
  return l;
}

query_ret_t Agent::appendEntries (
  term_t term, id_t leaderId, index_t prevLogIndex, term_t prevLogTerm,
  index_t leadersLastCommitIndex, query_t const& query) {
  
  if (query->isEmpty()) {              // heartbeat received
    Builder builder;
    builder.add("term", Value(this->term())); // Our own term
    builder.add("voteFor", Value(             // Our vite
      _constituent.vote(term, leaderId, prevLogIndex, leadersLastCommitIndex)));
    return query_ret_t(true, id(), std::make_shared<Builder>(builder));
  } else if (_constituent.leading()) { // We are leading
    _constituent.follow(term);
    return _state.log(term, leaderId, prevLogIndex, term, leadersLastCommitIndex);
  } else {                             // We redirect
    return query_ret_t(false,_constituent.leaderID());
  }
}

query_ret_t Agent::write (query_t const& query)  {
  if (_constituent.leading()) {     // We are leading
    return _state.write (query);
  } else {                          // We redirect
    return query_ret_t(false,_constituent.leaderID());
  }
}

query_ret_t Agent::read (query_t const& query) const {
  if (_constituent.leading()) {     // We are leading
    return _state.read (query);
  } else {                          // We redirect
    return query_ret_t(false,_constituent.leaderID());
  }
}

void State::run() {
  /*
   * - poll through the least of append entries
   * - if last round of polling took less than poll interval sleep
   * - else just start from top of list and work the 
   */
  while (true) {
    std::this_thread::sleep_for(duration_t(_poll_interval));
    for (auto& i : )
  }  
}

bool State::operator()(ClusterCommResult* ccr) {

}



}}
