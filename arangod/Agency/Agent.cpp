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
#include "GossipCallback.h"

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
      _serveActiveAgent(false),
      _nextCompationAfter(_config.compactionStepSize) {
  LOG(WARN)<< __FILE__ << __LINE__;
  _state.configure(this);
  LOG(WARN)<< __FILE__ << __LINE__;
  _constituent.configure(this);
  LOG(WARN)<< __FILE__ << __LINE__;
}


/// This agent's id
arangodb::consensus::id_t Agent::id() const {
  return config().id;
}


/// Agent's id is set once from state machine
bool Agent::id(arangodb::consensus::id_t const& id) {
  MUTEX_LOCKER(mutexLocker, _cfgLock);  
  _config.setId(id);
  return true;
}


/// Merge command line and persisted comfigurations
bool Agent::mergeConfiguration(VPackSlice const& persisted) {
  MUTEX_LOCKER(mutexLocker, _cfgLock);  
  return _config.merge(persisted);
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
  return priv_rpc_ret_t(
    _constituent.vote(t, id, lastLogIndex, lastLogTerm), this->term());
}


/// Get copy of momentary configuration
config_t const Agent::config() const {
  MUTEX_LOCKER(mutexLocker, _cfgLock);  
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

  if (index > _confirmed[id]) {  // progress this follower?
    _confirmed[id] = index;
  }

  if (index > _lastCommitIndex) {  // progress last commit?

    size_t n = 0;

    for (auto const& i : _config.active) {
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

  if (this->term() > term) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "I have a higher term than RPC caller.";
    return false;
  }

  if (!_constituent.vote(term, leaderId, prevIndex, prevTerm, true)) {
    return false;
  }

  size_t nqs   = queries->slice().length();

  if (nqs > 0) {

    size_t ndups = _state.removeConflicts(queries);
    
    if (nqs > ndups) {
      
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Appending " << nqs - ndups << " entries to state machine." <<
        nqs << " " << ndups;

      _state.log(queries, ndups);
  
    }
    
  }
  
  _spearhead.apply(_state.slices(_lastCommitIndex + 1, leaderCommitIndex));
  _readDB.apply(_state.slices(_lastCommitIndex + 1, leaderCommitIndex));
  _lastCommitIndex = leaderCommitIndex;
  
  if (_lastCommitIndex >= _nextCompationAfter) {
    _state.compact(_lastCommitIndex);
    _nextCompationAfter += _config.compactionStepSize;
  }
  
  return true;

}


/// Leader's append entries
priv_rpc_ret_t Agent::sendAppendEntriesRPC(
    arangodb::consensus::id_t follower_id) {
  
  term_t t(0);
  {
    MUTEX_LOCKER(mutexLocker, _ioLock);
    t = this->term();
  }

  index_t last_confirmed = _confirmed[follower_id];
  std::vector<log_t> unconfirmed = _state.get(last_confirmed);
  index_t highest = unconfirmed.back().index;

  if (highest == _lastHighest[follower_id] && (long)(500.0e6*_config.minPing) >
      (std::chrono::system_clock::now() - _lastSent[follower_id]).count()) {
    return priv_rpc_ret_t(true, t);
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

  // Body
  Builder builder;
  builder.add(VPackValue(VPackValueType::Array));
  for (size_t i = 1; i < unconfirmed.size(); ++i) {
    auto const& entry = unconfirmed.at(i);
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("index", VPackValue(entry.index));
    builder.add("term", VPackValue(entry.term));
    builder.add("query", VPackSlice(entry.entry->data()));
    builder.close();
    highest = entry.index;
  }
  builder.close();

  // Verbose output
  if (unconfirmed.size() > 1) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Appending " << unconfirmed.size() - 1
                                     << " entries up to index " << highest
                                     << " to follower " << follower_id;
  }

  // Send request
  arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, _config.pool[follower_id],
      arangodb::GeneralRequest::RequestType::POST, path.str(),
      std::make_shared<std::string>(builder.toJson()), headerFields,
      std::make_shared<AgentCallback>(this, follower_id, highest),
      0.5*_config.minPing, true, 0.75*_config.minPing);

  _lastSent[follower_id] = std::chrono::system_clock::now();
  _lastHighest[follower_id] = highest;

  return priv_rpc_ret_t(true, t);
  
}



bool Agent::activateSingle() {
  MUTEX_LOCKER(mutexLocker, _ioLock);
  if (_config.active.empty()) {
    _config.active.push_back(_config.id);
    return _state.persistActiveAgents(_config.id);
  }
  return true;
}


/// Load persistent state
bool Agent::load() {
  
  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");

  auto vocbase = database->vocbase();
  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;

  if (vocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Loading persistent state.";
  if (!_state.loadCollections(vocbase, queryRegistry, _config.waitForSync)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Failed to load persistent state on startup.";
  }

  if (size() > 1) {
    inception();
  } else {
    MUTEX_LOCKER(mutexLocker, _cfgLock);
    activateSingle();
  }
  
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Reassembling spearhead and read stores.";
  _spearhead.apply(_state.slices(_lastCommitIndex + 1));

  {
    CONDITION_LOCKER(guard, _appendCV);
    guard.broadcast();
  }

  reportIn(id(), _state.lastLog().index);

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting spearhead worker.";
  _spearhead.start();
  _readDB.start();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting constituent personality.";
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
    indices = _state.log(query, applied, term());
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
  auto result = std::make_shared<arangodb::velocypack::Builder>();
  std::vector<bool> success = _readDB.read(query, result);
  
  return read_ret_t(true, _constituent.leaderID(), success, result);

}


/// Send out append entries to followers regularly or on event
void Agent::run() {
  
  CONDITION_LOCKER(guard, _appendCV);

  // Only run in case we are in multi-host mode
  while (!this->isStopping() && size() > 1) {

    if (leading()) {             // Only if leading
      _appendCV.wait(1000);
    } else {
      _appendCV.wait();         // Else wait for our moment in the sun
    }

    // Append entries to followers
    for (auto const& i : _config.active) {
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
  {
    CONDITION_LOCKER(guard, _appendCV);
    guard.broadcast();
  }
  
  // Wake up supervision
  _supervision.wakeUp();

  return true;
  
}

// Rebuild key value stores
bool Agent::rebuildDBs() {

  MUTEX_LOCKER(mutexLocker, _ioLock);

  _spearhead.apply(_state.slices(_lastCommitIndex+1));
  _readDB.apply(_state.slices(_lastCommitIndex+1));
  
  return true;
  
}


/// Last commit index
arangodb::consensus::index_t Agent::lastCommitted() const {
  return _lastCommitIndex;
}

/// Last commit index
void Agent::lastCommitted(
  arangodb::consensus::index_t lastCommitIndex) {
  MUTEX_LOCKER(mutexLocker, _ioLock);
  _lastCommitIndex = lastCommitIndex;
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


/// Are we still starting up?
bool Agent::booting() {
  MUTEX_LOCKER(mutexLocker, _cfgLock);
  return (_config.poolSize > _config.pool.size());
}


void Agent::gossipCallback(
  arangodb::consensus::id_t const& peerId, query_t const& word) {
  {
    MUTEX_LOCKER(mutexLocker, _cfgLock);
    
    VPackSlice slice = word->slice();
    TRI_ASSERT(slice.isObject());
    
    size_t counter = 0;
    for (auto const& i : VPackObjectIterator(slice)) {
      MUTEX_LOCKER(mutexLocker, _cfgLock);
      if (++counter > _config.poolSize) {
        LOG_TOPIC(FATAL, Logger::AGENCY) <<
          "Too many peers for poolsize: " counter << ">" <<_config.poolSize;
        FATAL_ERROR_EXIT();
      }
      TRI_ASSERT(i.value.isString());
      if (_config.pool.find(i.key.copyString()) != _config.pool.end()) {
        _config.pool[i.key.copyString()] = i.value.copyString();
      } else {
        if (_config.pool[i.key.copyString()] != i.value.copyString()) {
          LOG_TOPIC(FATAL, Logger::AGENCY) << "Discrepancy in agent pool!";
          FATAL_ERROR_EXIT();
        }
      }
    }
  }

  if (counter < size()) {

    std::map<std::string,std::string> pool;
    { 
      MUTEX_LOCKER(mutexLocker, _cfgLock);
      pool = _config.pool;
    }
    
    Builder word;
    word.openObject();
    word.add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : pool) {
      word.add(i.first,VPackValue(i.second));
    }
    word.close();
    word.close();
    return ret;

    auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();
    std::string path = "/_api/agency/gossip";
    
    arangodb::ClusterComm::instance()->asyncRequest(
      "1", 1, pool(peerId), GeneralRequest::RequestType::POST, path,
      std::make_shared<std::string>(word->toJson()), headerFields,
      std::make_shared<GossipCallback>(this, peerId), 1.0, true);
    
  }
  
}


/// Gossip to others
void Agent::gossip() {

  if (booting()) {

    std::vector<std::string> peers;
    std::map<std::string,std::string> pool;

    { 
      MUTEX_LOCKER(mutexLocker, _cfgLock);
      peers = _config.gossipPeers;
      pool = _config.pool;
    }

    Builder word;
    word.openObject();
    word.add("pool", VPackValue(VPackValueType::Object));
    for (auto const& i : pool) {
      word.add(i.first,VPackValue(i.second));
    }
    word.close();
    word.close();
    return ret;

    for (auto const& endpoint : gossipPeers) {

      auto headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
      std::string path = "/_api/agency/config";
        
      for (auto const& agent : pool) {
        arangodb::ClusterComm::instance()->asyncRequest(
          "1", 1, agent.second, GeneralRequest::RequestType::POST, path,
          std::make_shared<std::string>(word->toJson()), headerFields,
          std::make_shared<GossipCallback>(this, agent.first), 1.0, true);
      }
      
    }
    
  }
  
}


/// Handle incomint gossip
void Agent::gossip(query_t const& word) {

  TRI_ASSERT(word->slice().isObject());

  MUTEX_LOCKER(mutexLocker, _cfgLock);
  for (auto const& ent : VPackObjectIterator(word->slice())) {
    TRI_ASSERT(ent.value.isString());
    auto it = _config.pool.find(ent.key.copyString());
    if (it != _config.pool.end()) {
      TRI_ASSERT(it->second == ent.value.copyString());
    } else {
      _config.pool[ent.key.copyString()] = ent.value.copyString();
    }
  }
  
}


/// Check for persisted agency
/// - yes: Ask leader for agents
///   - if my id among those: RAFT
///   - else: relax
/// - no: 
bool Agent::activeAgency() {

  std::map<std::string, std::string> pool;
  std::vector<std::string> active;
  { 
    MUTEX_LOCKER(mutexLocker, _cfgLock);
    pool = _config.pool;
    active = _config.active;
  }

  if (_config.size() == active.size()) {
    
  }

  return false;

}

inline static query_t getResponse(
  std::string const& endpoint, std::string const& path, double timeout = 1.0) {

  query_t ret = nullptr;
  std::unordered_map<std::string, std::string> headerFields;
  auto res = arangodb::ClusterComm::instance()->syncRequest(
    "1", 1, endpoint, GeneralRequest::RequestType::GET, path, std::string(),
    headerFields, 1.0);

  if (res->status == CL_COMM_RECEIVED) {
    ret = res->result->getBodyVelocyPack();
  }

  return ret;

}

bool Agent::persistedAgents() {
  
  std::vector<std::string> active;
  std::map<std::string,std::string> pool;
  query_t activeBuilder = nullptr;
  {
    MUTEX_LOCKER(mutexLocker, _cfgLock);
    active = _config.active;
    pool   = _config.pool;
    activeBuilder = _config.activeToBuilder();
  }

  if (active.empty()) {
    return false;
  }

  // 1 min timeout
  std::chrono::seconds timeout(60);

  auto const& it = find(active.begin(), active.end(), _config.id);
  auto start = std::chrono::system_clock::now();
  std::string const path = "/_api/agency/activeAgents";

  if (it != active.end()) {  // We used to be active agent
    
    {
      MUTEX_LOCKER(mutexLocker, _cfgLock);
      _serveActiveAgent = true; // Serve /_api/agency/activeAgents
    }

    std::map<std::string,bool> consens;
    
    while (true) {
      
      // contact others and count how many succeeded
      for (auto const& agent : active) {
        /*std::unique_ptr<ClusterCommResult> res =
          ClusterComm::instance()->syncRequest(
            "1", 1, pool.at(agent), GeneralRequest::RequestType::POST, path,
            std::make_shared<std::string>(word->toString()), headerFields,
            std::make_shared<GossipCallback>(), 1.0, true, 0.5);
        if (res->status == CL_COMM_SENT) { 
          if (res->result->getHttpReturnCode() == 200) {
            consens[agent] = true;
          } else if (res->result->getHttpReturnCode() == 428) {
            LOG_TOPIC(DEBUG, Logger::AGENCY) <<
              "Local information on agency is rejected by " << agent;
          } else if (res->result->getHttpReturnCode() == 409) {
            LOG_TOPIC(DEBUG, Logger::AGENCY) <<
              "I am no longer active agent according to " << agent;
          }
          }*/
        0;
      }

      // Collect what we know. Act accordingly.
      
      // timeout: clear list of active and give up
      if (std::chrono::system_clock::now() - start > timeout) { 
        {
          MUTEX_LOCKER(mutexLocker, _cfgLock);
          _config.active.clear();
        }

      }
      
    }
    
    {
      MUTEX_LOCKER(mutexLocker, _cfgLock);
      _serveActiveAgent = false;  // Unserve /_api/agency/activeAgents
    }
    
  } else {

    std::string path = "/_api/agency/config";

    while (true) { // try to connect an agent until attempts futile for too long

      for (auto const& ep : active) {

        auto res = getResponse(ep, path);

        if (res != nullptr) { // Got in touch with an active agent
          auto slice = res->slice();
          LOG_TOPIC(DEBUG, Logger::AGENCY)
            << "Got response from a persisted agent with configuration "
            << slice.toJson();
          MUTEX_LOCKER(mutexLocker, _cfgLock);
          _config.override(slice);
          
        }
        
      }

      if (std::chrono::system_clock::now() - start > timeout) { 
        {
          MUTEX_LOCKER(mutexLocker, _cfgLock);
          _config.active.clear();
        }
        return false;
      } 

    }
  }
  
  return false;
  
}


/// Do we serve agentStartupSequence
bool Agent::serveActiveAgent() {
  return _serveActiveAgent;
}


/// Initial inception of the agency world
void Agent::inception() {

  auto start = std::chrono::system_clock::now(); //
  std::chrono::seconds timeout(300);
  
  if (persistedAgents()) {
    return;
  }

  CONDITION_LOCKER(guard, _configCV);

  while (booting()) {

    if (activeAgency()) {
      return;
    }

    gossip()
    
    _configCV.wait(1000);

    if (std::chrono::system_clock::now() - start > timeout) {
      LOG_TOPIC(ERR, Logger::AGENCY) <<
        "Failed to find complete pool of agents.";
      FATAL_ERROR_EXIT();
    }
      
  }
  
}

}} // namespace
