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

State::State() {
}

State::~State() {}

State::configure (size_t size) {
  _log.push_back(log_t (0, 0, 0, "", std::vector<bool>(size,true)));
}

//Leader
std::vector<index_t> State::log (query_t const& query, term_t term, id_t lid, size_t size) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  std::vector<index_t> idx;
  Builder builder;
  for (size_t i = 0; i < query->slice().length()) {
    idx.push_back(_log.end().index+1);
    _log.push_back(idx[i], term, lid, query.toString(), std::vector<bool>(size));
    _log.end().ack[lid] = true; // Leader confirms myself
    builder.add("query", qyery->Slice());
    builder.add("idx", Value(idx[i]));
    builder.add("term", Value(term));
    builder.add("leaderID", Value(lid));
    builder.close();
  }
  save (builder.slice());
  return idx;
}

//Follower
void State::log (query_t const& query, std::vector<index_t> cont& idx, term_t term, id_t lid, size_t size) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  Builder builder;
  for (size_t i = 0; i < query->slice().length()) {
    _log.push_back(idx[i], term, lid, query.toString(), std::vector<bool>(size));
    builder.add("query", qyery->Slice());  // query
    builder.add("idx", Value(idx[i]));     // log index
    builder.add("term", Value(term));      // term
    builder.add("leaderID", Value(lid));   // leader id
    builder.close();
  }
  save (builder.slice());
}

void State::log (query_t const& query, index_t idx, term_t term, id_t lid, size_t size) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  _log.push_back(idx, term, lid, query.toString(), std::vector<bool>(size));
}

void State::confirm (id_t id, index_t index) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  _log[index][id] = true;
}

bool findit (index_t index, term_t term) {
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

collect_ret_t collectUnacked (id_t id) {
  // Collect all unacknowledged
  MUTEX_LOCKER(mutexLocker, _logLock);
  std::vector<index_t> work;
  bool found_first = false;
  id_t prev_log_term;
  index_t prev_log_index;
  for (size_t i = 0; i < _log.end(); ++i) {
    if (!_log[i].ack[id]) {
      work.push_back(_log[i].index);
      if (!found_first) {
        prev_log_term = _log[i-1].term;
        prev_log_index = _log[i-1].index;
        found_first = true;
      }
    }
  }
  return collect_ret_t(prev_log_index, prev_log_term, work);
}

bool save (std::string const& ep) {
  // Persist to arango db
  // AQL votedFor, lastCommit
};

load_ret_t load (std::string const& ep) {
  // Read all from arango db
  return load_ret_t (currentTerm, votedFor)
};



