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
#include <iomanip>
#include <sstream>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

State::State(std::string const& end_point) :
  _end_point(end_point), _collections_checked(false), _collections_loaded(false) {
  std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
  arangodb::velocypack::Slice tmp("\x00a",&Options::Defaults);
  buf->append(reinterpret_cast<char const*>(tmp.begin()), tmp.byteSize());
  if (!_log.size()) {
    _log.push_back(log_t(index_t(0), term_t(0), id_t(0), buf));
  }
}

State::~State() {}

bool State::persist (index_t index, term_t term, id_t lid,
                  arangodb::velocypack::Slice const& entry) {

  static std::string const path = "/_api/document?collection=log";
  std::map<std::string, std::string> headerFields;
  
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  std::stringstream index_str;
  index_str << std::setw(20) << std::setfill('0') << index;
  body.add("_key",Value(index_str.str()));
  body.add("term",Value(term));
  body.add("leader", Value((uint32_t)lid));
  body.add("request",entry[0]);
  body.close();
  
  std::unique_ptr<arangodb::ClusterCommResult> res = 
    arangodb::ClusterComm::instance()->syncRequest (
      "1", 1, _end_point, HttpRequest::HTTP_REQUEST_POST, path,
      body.toJson(), headerFields, 0.0);

  
  if (res->status != CL_COMM_SENT) {
    LOG_TOPIC(WARN, Logger::AGENCY) << res->status << ": " << CL_COMM_SENT << ", " << res->errorMessage;
    LOG_TOPIC(WARN, Logger::AGENCY) << res->result->getBodyVelocyPack()->toJson();
  }
  
  return (res->status == CL_COMM_SENT); // TODO: More verbose result
  
}

//Leader
std::vector<index_t> State::log (
  query_t const& query, std::vector<bool> const& appl, term_t term, id_t lid) {
  if (!checkCollections()) {
    createCollections();
  }
  if (!_collections_loaded) {
    loadCollections();
    _collections_loaded = true;
  }
  
  // TODO: Check array
  std::vector<index_t> idx(appl.size());
  std::vector<bool> good = appl;
  size_t j = 0;
  MUTEX_LOCKER(mutexLocker, _logLock); // log entries must stay in order
  for (auto const& i : VPackArrayIterator(query->slice()))  {
    if (good[j]) {
      std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
      buf->append ((char const*)i[0].begin(), i[0].byteSize()); 
      idx[j] = _log.back().index+1;
      _log.push_back(log_t(idx[j], term, lid, buf)); // log to RAM
      persist(idx[j], term, lid, i);                    // log to disk
      ++j;
    }
  }
  return idx;
}

//Follower
#include <iostream>
bool State::log (query_t const& queries, term_t term, id_t leaderId,
                 index_t prevLogIndex, term_t prevLogTerm) { // TODO: Throw exc
  if (queries->slice().type() != VPackValueType::Array) {
    return false;
  }
  MUTEX_LOCKER(mutexLocker, _logLock); // log entries must stay in order
  for (auto const& i : VPackArrayIterator(queries->slice())) {
    try {
      std::shared_ptr<Buffer<uint8_t>> buf = std::make_shared<Buffer<uint8_t>>();
      buf->append ((char const*)i.get("query").begin(), i.get("query").byteSize());
      _log.push_back(log_t(i.get("index").getUInt(), term, leaderId, buf));
    } catch (std::exception const& e) {
      LOG(FATAL) << e.what();
    }
    //save (builder);
  }
  return true;
}

std::vector<log_t> State::get (index_t start, index_t end) const {
  std::vector<log_t> entries;
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (end == (std::numeric_limits<uint64_t>::max)())
    end = _log.size() - 1;
  for (size_t i = start; i <= end; ++i) {// TODO:: Check bounds
    entries.push_back(_log[i]);
  }
  return entries;
}

std::vector<VPackSlice> State::slices (index_t start, index_t end) const {
  std::vector<VPackSlice> slices;
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (end == (std::numeric_limits<uint64_t>::max)())
    end = _log.size() - 1;
  for (size_t i = start; i <= end; ++i) {// TODO:: Check bounds
    slices.push_back(VPackSlice(_log[i].entry->data()));
  }
  return slices;
}

log_t const& State::operator[](index_t index) const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  return _log[index];
}

log_t const& State::lastLog() const {
  MUTEX_LOCKER(mutexLocker, _logLock);
  return _log.back();
}

bool State::setEndPoint (std::string const& end_point) {
  _end_point = end_point;
  _collections_checked = false;
  return true;
};

bool State::checkCollections() {
  if (!_collections_checked) {
    _collections_checked = checkCollection("log") && checkCollection("election");
  }
  return _collections_checked;
}

bool State::createCollections() {
  if (!_collections_checked) {
    return (createCollection("log") && createCollection("election"));
  }
  return _collections_checked;
}

bool State::checkCollection (std::string const& name) {
  if (!_collections_checked) {
    std::string path (std::string("/_api/collection/") + name
                      + std::string("/properties"));
    std::map<std::string, std::string> headerFields;
    std::unique_ptr<arangodb::ClusterCommResult> res = 
      arangodb::ClusterComm::instance()->syncRequest (
        "1", 1, _end_point, HttpRequest::HTTP_REQUEST_GET, path, "",
        headerFields, 1.0);
    return (!res->result->wasHttpError());
  }
  return true;
}

bool State::createCollection (std::string const& name) {
  static std::string const path = "/_api/collection";
  std::map<std::string, std::string> headerFields;
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.add("name", Value(name));
  body.close();
  std::unique_ptr<arangodb::ClusterCommResult> res = 
    arangodb::ClusterComm::instance()->syncRequest (
      "1", 1, _end_point, HttpRequest::HTTP_REQUEST_POST, path,
      body.toJson(), headerFields, 1.0);
  return (!res->result->wasHttpError());
}

bool State::loadCollections () {
  loadCollection("log");
  return true;
}



bool State::loadCollection (std::string const& name) {
  
  if (checkCollections()) {

    // Path
    std::string path ("/_api/cursor");

    // Body
    Builder tmp;
    tmp.openObject();
    tmp.add("query", Value(std::string("FOR l IN ")
                           + name + std::string(" SORT l._key RETURN l")));
    tmp.close();
    
    // Request
    std::map<std::string, std::string> headerFields;
    std::unique_ptr<arangodb::ClusterCommResult> res = 
      arangodb::ClusterComm::instance()->syncRequest (
        "1", 1, _end_point, HttpRequest::HTTP_REQUEST_POST, path,
        tmp.toJson(), headerFields, 1.0);
    
    // If success rebuild state deque
    if (res->status == CL_COMM_SENT) {
      std::shared_ptr<Builder> body = res->result->getBodyVelocyPack();
      if (body->slice().hasKey("result")) {
        for (auto const& i : VPackArrayIterator(body->slice().get("result"))) {
          buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
          tmp->append((char const*)i.get("request").begin(),i.get("request").byteSize());
          _log.push_back(log_t(
                           std::stoi(i.get("_key").copyString()),
                           i.get("term").getUInt(), i.get("leader").getUInt(), tmp));
        }
      }
    }

    return true;

  } else {
    
    return false;
  }
}



