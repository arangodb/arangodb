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

State::State() {}
State::~State() {
  save();
}

std::vector<size_t> State::log (query_t const& query, term_t term, id_t lid, size_t size) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  index_t idx = _log.end().index+1;
  _log.push_back(idx, term, lid, query.toString(), std::vector<bool>(size));
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

bool save (std::string const& ep) {};

bool load (std::string const& ep) {};



