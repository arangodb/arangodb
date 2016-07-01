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
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>

using namespace arangodb::application_features;
using namespace arangodb::velocypack;

namespace arangodb {
namespace consensus {


/// Agent configuration
Agent::Agent(config_t const& config)
    : Thread("Agent"),
      _config(config),
      _lastCommitIndex(0),
      _spearhead(this),
      _readDB(this),
      _nextCompationAfter(_config.compactionStepSize) {
  _state.configure(this);
  _constituent.configure(this);
  _confirmed.resize(size(), 0);  // agency's size and reset to 0
}


/// This agent's id
arangodb::consensus::id_t Agent::id() const {
  return _config.id;
}


/// Dtor shuts down thread
Agent::~Agent() {
  shutdown();
}


/// State machine
State const& Agent::state() const {
  return _state;
}


/// Start all agent thread
bool Agent::start() {
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting agency comm worker.";
  Thread::start();
  return true;
}


/// This agent's term
term_t Agent::term() const {
  return _constituent.term();
}


/// Agency size
size_t Agent::size() const {
  return _config.size();
}


/// My endpoint
std::string const& Agent::endpoint() const {
  return _config.endpoint;
}


/// Handle voting
priv_rpc_ret_t Agent::requestVote(
  term_t t, arangodb::consensus::id_t id, index_t lastLogIndex,
  index_t lastLogTerm, query_t const& query) {
  
  /// Are we receiving new endpoints
  if (query != nullptr) {  // record new endpoints
    if (query->slice().hasKey("endpoints") &&
        query->slice().get("endpoints").isArray()) {
      size_t j = 0;
      for (auto const& i : VPackArrayIterator(query->slice().get("endpoints"))) {
        _config.endpoints[j++] = i.copyString();
      }
    }
  }

  /// Constituent handles this
  return priv_rpc_ret_t(_constituent.vote(t, id, lastLogIndex, lastLogTerm),
                        this->term());
  
}


/// Get configuration
config_t const& Agent::config() const {
  return _config;
}


/// Leader's id
arangodb::consensus::id_t Agent::leaderID() const {
  return _constituent.leaderID();
}


/// Are we leading?
bool Agent::leading() const {
  return _constituent.leading();
}


// Waits here for confirmation of log's commits up to index.
// Timeout in seconds
bool Agent::waitFor(index_t index, double timeout) {

  if (size() == 1) {  // single host agency
    return true;
  }

  CONDITION_LOCKER(guard, _waitForCV);

  // Wait until woken up through AgentCallback
  while (true) {

    /// success?
    if (_lastCommitIndex >= index) {
      return true;
    }

    // timeout
    if (!_waitForCV.wait(static_cast<uint64_t>(1.0e6 * timeout))) {
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
void Agent::reportIn(arangodb::consensus::id_t id, index_t index) {

  MUTEX_LOCKER(mutexLocker, _ioLock);

  TRI_ASSERT(id<_confirmed.size());

  if (index > _confirmed[id]) {  // progress this follower?
    _confirmed[id] = index;
  }

  if (index > _lastCommitIndex) {  // progress last commit?

    size_t n = 0;

    for (size_t i = 0; i < size(); ++i) {
      n += (_confirmed[i] >= index);
    }

    // catch up read database and commit index
    if (n > size() / 2) {

      LOG_TOPIC(DEBUG, Logger::AGENCY) << "Critical mass for commiting "
                                       << _lastCommitIndex + 1 << " through "
                                       << index << " to read db";

      _readDB.apply(_state.slices(_lastCommitIndex + 1, index));
      _lastCommitIndex = index;
      
      if (_lastCommitIndex >= _nextCompationAfter) {
        _state.compact(_lastCommitIndex);
        _nextCompationAfter += _config.compactionStepSize;
      }
      
    }
    
  }

  {
    CONDITION_LOCKER(guard, _waitForCV);
    guard.broadcast();
  }

}


/// Followers' append entries
bool Agent::recvAppendEntriesRPC(term_t term,
                                 arangodb::consensus::id_t leaderId,
                                 index_t prevIndex, term_t prevTerm,
                                 index_t leaderCommitIndex,
                                 query_t const& queries) {

  // Update commit index
  if (queries->slice().type() != VPackValueType::Array) {
    LOG_TOPIC(WARN, Logger::AGENCY)
        << "Received malformed entries for appending. Discarting!";
    return false;
  }

  MUTEX_LOCKER(mutexLocker, _ioLock);

//  index_t lastPersistedIndex = _lastCommitIndex;

  if (this->term() > term) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "I have a higher term than RPC caller.";
    return false;
  }

  if (!_constituent.vote(term, leaderId, prevIndex, prevTerm, true)) {
    return false;
  }

  if (queries->slice().length()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Appending "
                                     << queries->slice().length()
                                     << " entries to state machine.";
    /* bool success = */
    _state.log(queries, term, leaderId, prevIndex, prevTerm);
    _spearhead.apply(_state.slices(_lastCommitIndex + 1, leaderCommitIndex));
    _readDB.apply(_state.slices(_lastCommitIndex + 1, leaderCommitIndex));
    _lastCommitIndex = leaderCommitIndex;
  }

  if (_lastCommitIndex >= _nextCompationAfter) {

    _state.compact(_lastCommitIndex);
    _nextCompationAfter += _config.compactionStepSize;
  }
  
  return true;

}


/// Leader's append entries
priv_rpc_ret_t Agent::sendAppendEntriesRPC(
    arangodb::consensus::id_t follower_id) {
  
  index_t last_confirmed = _confirmed[follower_id];
  std::vector<log_t> unconfirmed = _state.get(last_confirmed);

  term_t t(0);
  {
    MUTEX_LOCKER(mutexLocker, _ioLock);
    t = this->term();
  }

  if (unconfirmed.empty()) {
    return priv_rpc_ret_t(false, t);
  }
  
  // RPC path
  std::stringstream path;
  path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId=" << id()
       << "&prevLogIndex=" << unconfirmed.front().index
       << "&prevLogTerm=" << unconfirmed.front().term
       << "&leaderCommit=" << _lastCommitIndex;

  // Headers
  auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();

  // Highest unconfirmed
  index_t last = unconfirmed[0].index;

  // Body
  Builder builder;
  builder.add(VPackValue(VPackValueType::Array));
  for (size_t i = 1; i < unconfirmed.size(); ++i) {
    auto const& entry = unconfirmed.at(i);
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("index", VPackValue(entry.index));
    builder.add("query", VPackSlice(entry.entry->data()));
    builder.close();
    last = entry.index;
  }
  builder.close();

  // Verbose output
  if (unconfirmed.size() > 1) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Appending " << unconfirmed.size() - 1
                                     << " entries up to index " << last
                                     << " to follower " << follower_id;
  }

  // Send request
  arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, _config.endpoints[follower_id],
      arangodb::GeneralRequest::RequestType::POST, path.str(),
      std::make_shared<std::string>(builder.toJson()), headerFields,
      std::make_shared<AgentCallback>(this, follower_id, last), 1, true);

  return priv_rpc_ret_t(true, t);
  
}


/// Load persistent state
bool Agent::load() {
  
  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");

  auto vocbase = database->vocbase();
  if (vocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Loading persistent state.";
  if (!_state.loadCollections(vocbase, _config.waitForSync)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Failed to load persistent state on startup.";
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Reassembling spearhead and read stores.";
  _spearhead.apply(_state.slices(_lastCommitIndex + 1));
  reportIn(id(), _state.lastLog().index);

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting spearhead worker.";
  _spearhead.start();
  _readDB.start();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting constituent personality.";
  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;
  TRI_ASSERT(queryRegistry != nullptr);
  _constituent.start(vocbase, queryRegistry);

  if (_config.supervision) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting cluster sanity facilities";
    _supervision.start(this);
  }

  return true;
}


/// Write new entries to replicated state and store
write_ret_t Agent::write(query_t const& query) {

  std::vector<bool> applied;
  std::vector<index_t> indices;
  index_t maxind = 0;

  // Only leader else redirect
  if (!_constituent.leading()) {
    return write_ret_t(false, _constituent.leaderID());
  }

  // Apply to spearhead and get indices for log entries
  {
    MUTEX_LOCKER(mutexLocker, _ioLock);
    applied = _spearhead.apply(query);
    indices = _state.log(query, applied, term(), id());
  }

  // Maximum log index
  if (!indices.empty()) {
    maxind = *std::max_element(indices.begin(), indices.end());
  }

  // Report that leader has persisted
  reportIn(id(), maxind);
  
  return write_ret_t(true, id(), applied, indices);
  
}


/// Read from store
read_ret_t Agent::read(query_t const& query) {

  // Only leader else redirect
  if (!_constituent.leading()) {
    return read_ret_t(false, _constituent.leaderID());
  }

  // Retrieve data from readDB
  query_t result = std::make_shared<arangodb::velocypack::Builder>();
  std::vector<bool> success = _readDB.read(query, result);
  
  return read_ret_t(true, _constituent.leaderID(), success, result);

}


/// Send out append entries to followers regularly or on event
void Agent::run() {
  
  CONDITION_LOCKER(guard, _appendCV);

  // Only run in case we are in multi-host mode
  while (!this->isStopping() && size() > 1) {

    if (leading()) {             // Only if leading
      _appendCV.wait(100000);
    } else {
      _appendCV.wait();         // Else wait for our moment in the sun
    }
    
    // Append entries to followers
    for (arangodb::consensus::id_t i = 0; i < size(); ++i) {
      if (i != id()) {
        sendAppendEntriesRPC(i);
      }
    }
    
  }

}


/// Orderly shutdown
void Agent::beginShutdown() {
  
  Thread::beginShutdown();

  // Stop supervision
  if (_config.supervision) {
    _supervision.beginShutdown();
  }

  // Stop constituent and key value stores
  _constituent.beginShutdown();
  _spearhead.beginShutdown();
  _readDB.beginShutdown();
  
  // Wake up all waiting rest handlers
  {
    CONDITION_LOCKER(guardW, _waitForCV);
    guardW.broadcast();
  }
  
  // Wake up run
  {
    CONDITION_LOCKER(guardA, _appendCV);
    guardA.broadcast();
  } 

}


/// Becoming leader
bool Agent::lead() {

  // Key value stores
  rebuildDBs();
  
  // Wake up run
  CONDITION_LOCKER(guard, _appendCV);
  guard.signal();

  return true;
  
}

// Rebuild key value stores
bool Agent::rebuildDBs() {
  
  MUTEX_LOCKER(mutexLocker, _ioLock);
  _spearhead.apply(_state.slices());
  _readDB.apply(_state.slices());
  
  return true;
  
}


/// Last commit index
arangodb::consensus::index_t Agent::lastCommited() const {
  return _lastCommitIndex;
}


/// Last log entry
log_t const& Agent::lastLog() const { return _state.lastLog(); }


/// Get spearhead
Store const& Agent::spearhead() const { return _spearhead; }


/// Get readdb
Store const& Agent::readDB() const { return _readDB; }


/// Rebuild from persisted state
Agent& Agent::operator=(VPackSlice const& compaction) {

  // Catch up with compacted state
  MUTEX_LOCKER(mutexLocker, _ioLock);
  _spearhead = compaction.get("readDB");
  _readDB = compaction.get("readDB");

  // Catch up with commit
  try {
    _lastCommitIndex = std::stoul(compaction.get("_key").copyString());
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " <<__FILE__ << __LINE__;
  }

  // Schedule next compaction
  _nextCompationAfter = _lastCommitIndex + _config.compactionStepSize;

  return *this;
  
}


}} // namespace
