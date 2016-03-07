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

#include <velocypack/Buffer.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <sstream>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

State::State(std::string const& end_point) : _end_point(end_point) {
  if (!_log.size())
    _log.push_back(log_t(index_t(0), term_t(0), id_t(0),
                         std::make_shared<Buffer<uint8_t>>()));
}

State::~State() {}

bool State::save (arangodb::velocypack::Slice const& slice, double timeout) {
  static const std::string path = "/_api/cursor";
  std::map<std::string, std::string> headerFields;
  std::unique_ptr<arangodb::ClusterCommResult> res = 
    arangodb::ClusterComm::instance()->syncRequest (
      "1", 1, _end_point, HttpRequest::HTTP_REQUEST_POST, path, slice.toJson(),
      headerFields, 0.0);
  return (res->status == CL_COMM_SENT); // TODO: More verbose result
}

//Leader
std::vector<index_t> State::log (
  query_t const& query, std::vector<bool> const& appl, term_t term, id_t lid) {
  std::vector<index_t> idx;
  std::vector<bool> good = appl;
  size_t j = 0;
  MUTEX_LOCKER(mutexLocker, _logLock); // log entries must stay in order
  for (auto const& i : VPackArrayIterator(query->slice()))  {
    if (good[j]) {
      std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
      buf->append ((char const*)i.begin(), i.byteSize()); 
      idx.push_back(_log.back().index+1);
      _log.push_back(log_t(idx[j], term, lid, buf)); // log to RAM
      save(i);                                       // log to disk
      ++j;
    }
  }
  return idx;
}

//Follower
void State::log (query_t const& query, index_t index, term_t term, id_t lid) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
  buf->append ((char const*)query->slice().begin(), query->slice().byteSize());
  _log.push_back(log_t(index, term, lid, buf));
  //save (builder);
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

bool State::setEndPoint (std::string const& end_point) {
  _end_point = end_point;
  return true; // TODO: check endpoint
};

bool State::load () {
  // Read all from arango db
  //return load_ret_t (currentTerm, votedFor)
  return true;
};



