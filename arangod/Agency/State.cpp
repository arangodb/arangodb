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

#include "State.h"

#include <chrono>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::velocypack;

State::State() {
  load();
  if (!_log.size())
    _log.push_back(log_t(index_t(0), term_t(0), id_t(0), std::string()));
}

State::~State() {}

//Leader
std::vector<index_t> State::log (query_t const& query, term_t term, id_t lid) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  std::vector<index_t> idx;
  Builder builder;
  for (size_t i = 0; i < query->slice().length(); ++i) {
    idx.push_back(_log.back().index+1);
    _log.push_back(log_t(idx[i], term, lid, query->slice()[i].toString()));
    builder.add("query", query->slice()[i]);
    builder.add("idx", Value(idx[i]));
    builder.add("term", Value(term));
    builder.add("leaderID", Value(lid));
    builder.close();
  }
  save (builder); // persist
  return idx;
}

//Follower
void State::log (std::string const& query, index_t index, term_t term, id_t lid) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  _log.push_back(log_t(index, term, lid, query));
  Builder builder;
  builder.add("query", Value(query));         // query
  builder.add("idx", Value(index));    // log index
  builder.add("term", Value(term));    // term
  builder.add("leaderID", Value(lid)); // leader id
  builder.close();
  save (builder);
}

bool State::findit (index_t index, term_t term) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  auto i = std::begin(_log);
  while (i != std::end(_log)) { // Find entry matching index and term
    if ((*i).index == index) {
      if ((*i).term == term) {
        return true;
      } else if ((*i).term < term) {
        // If an existing entry conflicts with a new one (same index
        // but different terms), delete the existing entry and all that
        // follow it (§5.3)
        _log.erase(i, _log.end()); 
        return true;
      }
    }
  }
  return false;
}

log_t const& State::operator[](index_t index) const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  return _log[index];
}

log_t const& State::lastLog() const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  return _log.back();
}

collect_ret_t State::collectFrom (index_t index) {
  // Collect all from index on
  MUTEX_LOCKER(mutexLocker, _logLock);
  std::vector<index_t> work;
  id_t prev_log_term;
  index_t prev_log_index;
  prev_log_term = _log[index-1].term;
  prev_log_index = _log[index-1].index;
  for (index_t i = index; i < _log.size(); ++i) {
    work.push_back(_log[i].index);
  }
  return collect_ret_t(prev_log_index, prev_log_term, work);
}

bool State::save (arangodb::velocypack::Builder const&) {
  // Persist to arango db
  // AQL votedFor, lastCommit
  return true;
};

bool State::load () {
  // Read all from arango db
  //return load_ret_t (currentTerm, votedFor)
  return true;
};



