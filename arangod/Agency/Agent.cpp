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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <thread>

#include "Agency/GossipCallback.h"
#include "Basics/ConditionLocker.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::velocypack;
using namespace std::chrono;

namespace arangodb {
namespace consensus {

/// Agent configuration
Agent::Agent(config_t const& config)
  : Thread("Agent"),
    _config(config),
    _lastCommitIndex(0),
    _lastAppliedIndex(0),
    _lastCompactionIndex(0),
    _leaderCommitIndex(0),
    _spearhead(this),
    _readDB(this),
    _transient(this),
    _compacted(this),
    _nextCompactionAfter(_config.compactionStepSize()),
    _inception(std::make_unique<Inception>(this)),
    _activator(nullptr),
    _compactor(this),
    _ready(false),
    _preparing(false) {
  _state.configure(this);
  _constituent.configure(this);
}

/// This agent's id
std::string Agent::id() const { return _config.id(); }

/// Agent's id is set once from state machine
bool Agent::id(std::string const& id) {
  bool success;
  if ((success = _config.setId(id))) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "My id is " << id;
  } else {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Cannot reassign id once set: My id is " << _config.id()
      << " reassignment to " << id;
  }
  return success;
}

/// Merge command line and persisted comfigurations
bool Agent::mergeConfiguration(VPackSlice const& persisted) {
  return _config.merge(persisted); // Concurrency managed in merge
}

/// Dtor shuts down thread
Agent::~Agent() {

  // Give up if constituent breaks shutdown
  int counter = 0;
  while (_constituent.isRunning()) {
    usleep(100000);

    // emit warning after 5 seconds
    if (++counter == 10 * 5) {
      LOG_TOPIC(FATAL, Logger::AGENCY) << "constituent thread did not finish";
      FATAL_ERROR_EXIT();
    }
  }
    
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

/// Get all logs from state machine
query_t Agent::allLogs() const {
  return _state.allLogs();
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
std::string Agent::endpoint() const {
  return _config.endpoint();
}

/// Handle voting
priv_rpc_ret_t Agent::requestVote(
    term_t termOfPeer, std::string const& id, index_t lastLogIndex,
    index_t lastLogTerm, query_t const& query) {

  bool doIVote = _constituent.vote(termOfPeer, id, lastLogIndex, lastLogTerm);
  return priv_rpc_ret_t(doIVote, this->term());
}

/// Get copy of momentary configuration
config_t const Agent::config() const {
  return _config;
}

/// Leader's id
std::string Agent::leaderID() const {
  return _constituent.leaderID();
}

/// Are we leading?
bool Agent::leading() const {
  return _preparing || _constituent.leading();
}

/// Start constituent personality
void Agent::startConstituent() {
  activateAgency();
}

// Waits here for confirmation of log's commits up to index. Timeout in seconds.
AgentInterface::raft_commit_t Agent::waitFor(index_t index, double timeout) {

  if (size() == 1) {  // single host agency
    return Agent::raft_commit_t::OK;
  }

  // Get condition variable to notice commits
  CONDITION_LOCKER(guard, _waitForCV);

  // Wait until woken up through AgentCallback
  while (true) {
    /// success?
    {
      MUTEX_LOCKER(lockIndex, _ioLock);
      if (_lastCommitIndex >= index) {
        return Agent::raft_commit_t::OK;
      }
    }

    // timeout
    if (!_waitForCV.wait(static_cast<uint64_t>(1.0e6 * timeout))) {
      if (leading()) {
        MUTEX_LOCKER(lockIndex, _ioLock);
        return (_lastCommitIndex >= index) ?
          Agent::raft_commit_t::OK : Agent::raft_commit_t::TIMEOUT;
      } else {
        return Agent::raft_commit_t::UNKNOWN;
      }
    } 

    // shutting down
    if (this->isStopping()) {
      return Agent::raft_commit_t::UNKNOWN;
    }
  }

  // We should never get here
  TRI_ASSERT(false);

  return Agent::raft_commit_t::UNKNOWN;
}

//  AgentCallback reports id of follower and its highest processed index
void Agent::reportIn(std::string const& peerId, index_t index, size_t toLog) {

  {
    // Enforce _lastCommitIndex, _readDB and compaction to progress atomically
    MUTEX_LOCKER(ioLocker, _ioLock);

    // Update last acknowledged answer
    _lastAcked[peerId] = system_clock::now();

    if (index > _confirmed[peerId]) {  // progress this follower?
      _confirmed[peerId] = index;
      if (toLog > 0) { // We want to reset the wait time only if a package callback
        LOG_TOPIC(TRACE, Logger::AGENCY) << "Got call back of " << toLog << " logs";
        _earliestPackage[peerId] = system_clock::now();
      }
    }

    if (index > _lastCommitIndex) {  // progress last commit?

      size_t n = 0;

      for (auto const& i : _config.active()) {
        n += (_confirmed[i] >= index);
      }

      // catch up read database and commit index
      if (n > size() / 2) {

        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Critical mass for commiting " << _lastCommitIndex + 1
          << " through " << index << " to read db";

        _readDB.apply(
          _state.slices(
            _lastCommitIndex + 1, index), _lastCommitIndex, _constituent.term());
        
        _lastCommitIndex   = index;
        _lastAppliedIndex  = index;

        MUTEX_LOCKER(liLocker, _liLock);
        _leaderCommitIndex = index;
        if (_leaderCommitIndex >= _nextCompactionAfter) {
          _compactor.wakeUp();
        }

      }

    }
  } // MUTEX_LOCKER

  { // Wake up rest handler
    CONDITION_LOCKER(guard, _waitForCV);
    guard.broadcast();
  }

}

/// Followers' append entries
bool Agent::recvAppendEntriesRPC(
  term_t term, std::string const& leaderId, index_t prevIndex, term_t prevTerm,
  index_t leaderCommitIndex, query_t const& queries) {

  LOG_TOPIC(TRACE, Logger::AGENCY) << "Got AppendEntriesRPC from "
    << leaderId << " with term " << term;

  // Update commit index
  if (queries->slice().type() != VPackValueType::Array) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Received malformed entries for appending. Discarding!";
    return false;
  }

  if (!_constituent.checkLeader(term, leaderId, prevIndex, prevTerm)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Not accepting appendEntries from " << leaderId;
    return false;
  }

  {
    MUTEX_LOCKER(ioLocker, _liLock);
    _leaderCommitIndex = leaderCommitIndex;
  }
  
  size_t nqs = queries->slice().length();

  // State machine, _lastCommitIndex to advance atomically
  if (nqs > 0) {
    
    MUTEX_LOCKER(ioLocker, _ioLock);
  
    size_t ndups = _state.removeConflicts(queries);
    
    if (nqs > ndups) {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Appending " << nqs - ndups << " entries to state machine. ("
        << nqs << ", " << ndups << "): " << queries->slice().toJson() ;
      
      try {
        
        _lastCommitIndex = _state.log(queries, ndups);
        
        if (_lastCommitIndex >= _nextCompactionAfter) {
          _compactor.wakeUp();
        }

      } catch (std::exception const&) {
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Malformed query: " << __FILE__ << __LINE__;
      }
      
    }
  }

  return true;
}

/// Leader's append entries
void Agent::sendAppendEntriesRPC() {

  std::chrono::duration<int, std::ratio<1, 1000>> const dt (
    (_config.waitForSync() ? 40 : 2));
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return;
  }

  // _lastSent, _lastHighest and _confirmed only accessed in main thread
  std::string const myid = id();
  
  for (auto const& followerId : _config.active()) {

    if (followerId != myid && leading()) {

      term_t t(0);

      index_t last_confirmed, lastCommitIndex;
      {
        MUTEX_LOCKER(ioLocker, _ioLock);
        t = this->term();
        last_confirmed = _confirmed[followerId];
        lastCommitIndex = _lastCommitIndex;
      }

      std::vector<log_t> unconfirmed = _state.get(last_confirmed);

      index_t highest = unconfirmed.back().index;

      // _lastSent, _lastHighest: local and single threaded access
      duration<double> m = system_clock::now() - _lastSent[followerId];

      if (highest == _lastHighest[followerId] &&
          m.count() < 0.25 * _config.minPing()) {
        continue;
      }

      // RPC path
      std::stringstream path;
      path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId="
           << id() << "&prevLogIndex=" << unconfirmed.front().index
           << "&prevLogTerm=" << unconfirmed.front().term << "&leaderCommit="
           << lastCommitIndex;

      size_t toLog = 0;
      // Body
      Builder builder;
      builder.add(VPackValue(VPackValueType::Array));
      if (!_preparing &&
          ((system_clock::now() - _earliestPackage[followerId]).count() > 0)) {
        for (size_t i = 1; i < unconfirmed.size(); ++i) {
          auto const& entry = unconfirmed.at(i);
          builder.add(VPackValue(VPackValueType::Object));
          builder.add("index", VPackValue(entry.index));
          builder.add("term", VPackValue(entry.term));
          builder.add("query", VPackSlice(entry.entry->data()));
          builder.add("clientId", VPackValue(entry.clientId));
          builder.close();
          highest = entry.index;
          ++toLog;
        }
      }
      builder.close();
      
      // Verbose output
      if (unconfirmed.size() > 1) {
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Appending " << unconfirmed.size() - 1 << " entries up to index "
          << highest << " to follower " << followerId << ". Message: "
          << builder.toJson();
      }

      // Really leading?
      if (challengeLeadership()) {
        _constituent.candidate();
      }
      
      // Send request
      auto headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
      cc->asyncRequest(
        "1", 1, _config.poolAt(followerId),
        arangodb::rest::RequestType::POST, path.str(),
        std::make_shared<std::string>(builder.toJson()), headerFields,
        std::make_shared<AgentCallback>(
          this, followerId, (toLog) ? highest : 0, toLog),
        std::max(1.0e-3 * toLog * dt.count(), 0.25 * _config.minPing()), true);

      // _lastSent, _lastHighest: local and single threaded access
      _lastSent[followerId]        = system_clock::now();
      _lastHighest[followerId]     = highest;

      if (toLog > 0) {
        _earliestPackage[followerId] = system_clock::now() + toLog * dt;
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Appending " << unconfirmed.size() - 1 << " entries up to index "
          << highest << " to follower " << followerId << ". Message: "
          << builder.toJson() 
          << ". Next real log contact to " << followerId<< " in: " 
          <<  std::chrono::duration<double, std::milli>(
            _earliestPackage[followerId]-system_clock::now()).count() << "ms";
      } else {
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Just keeping follower " << followerId
          << " devout with " << builder.toJson();
      }
        
    }
  }
}


// Check if I am member of active agency
bool Agent::active() const {
  std::vector<std::string> active = _config.active();
  return (find(active.begin(), active.end(), id()) != active.end());
}


// Activate with everything I need to know
query_t Agent::activate(query_t const& everything) {

  auto ret = std::make_shared<Builder>();
  ret->openObject();

  Slice slice = everything->slice();

  if (slice.isObject()) {
    
    if (active()) {
      ret->add("success", VPackValue(false));
    } else {

      index_t lastCommitIndex = 0;
      Slice compact = slice.get("compact");
      Slice    logs = slice.get("logs");

      
      std::vector<Slice> batch;
      for (auto const& q : VPackArrayIterator(logs)) {
        batch.push_back(q.get("request"));
      }

      {
        MUTEX_LOCKER(ioLocker, _ioLock); // Atomicity 
        if (!compact.isEmptyArray()) {
          _readDB = compact.get("readDB");
        }
        lastCommitIndex = _lastCommitIndex;
        _readDB.apply(batch, lastCommitIndex, _constituent.term());
        _spearhead = _readDB;
      }

      //_state.persistReadDB(everything->slice().get("compact").get("_key"));
      //_state.log((everything->slice().get("logs"));

      ret->add("success", VPackValue(true));
      ret->add("commitId", VPackValue(lastCommitIndex));
    }

  } else {

    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Activation failed. \"Everything\" must be an object, is however "
      << slice.typeName();

  }
  ret->close();
  return ret;

}

/// @brief Activate agency (Inception thread for multi-host, main thread else)
bool Agent::activateAgency() {
  if (_config.activeEmpty()) {
    size_t count = 0;
    for (auto const& pair : _config.pool()) {
      _config.activePushBack(pair.first);
      if (++count == size()) {
        break;
      }
    }
    bool persisted = false; 
    try {
      _state.persistActiveAgents(_config.activeToBuilder(),
                                 _config.poolToBuilder());
      persisted = true;
    } catch (std::exception const& e) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "Failed to persist active agency: " << e.what();
    }
    return persisted;
  }
  return true;
}

/// Load persistent state called once
bool Agent::load() {

  DatabaseFeature* database =
      ApplicationServer::getFeature<DatabaseFeature>("Database");

  auto vocbase = database->systemDatabase();
  auto queryRegistry = QueryRegistryFeature::QUERY_REGISTRY;

  if (vocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Loading persistent state.";
  if (!_state.loadCollections(vocbase, queryRegistry, _config.waitForSync())) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Failed to load persistent state on startup.";
  }

  if (size() > 1) {
    _inception->start();
  } 

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Reassembling spearhead and read stores.";
  _spearhead.apply(
    _state.slices(_lastCommitIndex + 1), _lastCommitIndex, _constituent.term());
  
  {
    CONDITION_LOCKER(guard, _appendCV);
    guard.broadcast();
  }

  reportIn(id(), _state.lastLog().index);

  _compactor.start();
  TRI_ASSERT(queryRegistry != nullptr);

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting spearhead worker.";
  if (size() == 1 || !this->isStopping()) {
    _spearhead.start();
    _readDB.start();
  }
 
  TRI_ASSERT(queryRegistry != nullptr);
  if (size() == 1) {
    activateAgency();
  }
 
  if (size() == 1 || !this->isStopping()) {
    _constituent.start(vocbase, queryRegistry);
    persistConfiguration(term());
  }
 
  if (size() == 1 || (!this->isStopping() && _config.supervision())) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting cluster sanity facilities";
    _supervision.start(this);
  }

  return true;

}

/// Still leading? Under MUTEX from ::read or ::write
bool Agent::challengeLeadership() {

  size_t good = 0;
  
  for (auto const& i : _lastAcked) {
    duration<double> m = system_clock::now() - i.second;
    if (0.9 * _config.minPing() > m.count()) {
      ++good;
    }
  }
  
  return (good < size() / 2);  // not counting myself
}


/// Get last acknowledged responses on leader
query_t Agent::lastAckedAgo() const {
  
  std::map<std::string, TimePoint> lastAcked;
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    lastAcked = _lastAcked;
  }
  
  auto ret = std::make_shared<Builder>();
  ret->openObject();
  if (leading()) {
    for (auto const& i : lastAcked) {
      ret->add(i.first, VPackValue(
                 1.0e-2 * std::floor(
                   (i.first!=id() ?
                    duration<double>(system_clock::now()-i.second).count()*100.0
                    : 0.0))));
    }
  }
  ret->close();
  
  return ret;
  
}

trans_ret_t Agent::transact(query_t const& queries) {

  arangodb::consensus::index_t maxind = 0; // maximum write index

  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return trans_ret_t(false, leader);
  }

  // Apply to spearhead and get indices for log entries
  auto qs = queries->slice();
  addTrxsOngoing(qs);    // remember that these are ongoing
  auto ret = std::make_shared<arangodb::velocypack::Builder>();
  size_t failed = 0;
  ret->openArray();
  {
    
    MUTEX_LOCKER(ioLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      return trans_ret_t(false, NO_LEADER);
    }
    
    for (const auto& query : VPackArrayIterator(qs)) {
      if (query[0].isObject()) {
        check_ret_t res = _spearhead.apply(query); 
        if(res.successful()) {
          maxind = (query.length() == 3 && query[2].isString()) ?
            _state.log(query[0], term(), query[2].copyString()) :
            _state.log(query[0], term());
          ret->add(VPackValue(maxind));
        } else {
          _spearhead.read(res.failed->slice(), *ret);
          ++failed;
        }
      } else if (query[0].isString()) {
        _spearhead.read(query, *ret);
      }
    }
    
    removeTrxsOngoing(qs);

    // (either no writes or all preconditions failed)
/*    if (maxind == 0) {
      ret->clear();
      ret->openArray();      
      for (const auto& query : VPackArrayIterator(qs)) {
        if (query[0].isObject()) {
          ret->add(VPackValue(0));
        } else if (query[0].isString()) {
          _readDB.read(query, *ret);
        }
      }
      }*/
    
  }
  ret->close();
  
  // Report that leader has persisted
  reportIn(id(), maxind);

  return trans_ret_t(true, id(), maxind, failed, ret);
}


// Non-persistent write to non-persisted key-value store
trans_ret_t Agent::transient(query_t const& queries) {

  auto ret = std::make_shared<arangodb::velocypack::Builder>();
  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return trans_ret_t(false, leader);
  }
  
  // Apply to spearhead and get indices for log entries
  {
    VPackArrayBuilder b(ret.get());
    
    MUTEX_LOCKER(ioLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      return trans_ret_t(false, NO_LEADER);
    }

    // Read and writes
    for (const auto& query : VPackArrayIterator(queries->slice())) {
      if (query[0].isObject()) {
        ret->add(VPackValue(_transient.apply(query).successful()));
      } else if (query[0].isString()) {
        _transient.read(query, *ret);
      }
    }

  }

  return trans_ret_t(true, id(), 0, 0, ret);

}


inquire_ret_t Agent::inquire(query_t const& query) {
  inquire_ret_t ret;

  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return inquire_ret_t(false, leader);
  }
  
  MUTEX_LOCKER(ioLocker, _ioLock);

  auto si = _state.inquire(query);

  bool found = false;
  auto builder = std::make_shared<VPackBuilder>();
  {
    VPackArrayBuilder b(builder.get());
    for (auto const& i : si) {
      VPackArrayBuilder bb(builder.get());
      for (auto const& j : i) {
        found = true;
        VPackObjectBuilder bbb(builder.get());
        builder->add("index", VPackValue(j.index));
        builder->add("term", VPackValue(j.term));
        builder->add("query", VPackSlice(j.entry->data()));
      }
    }
  }
  
  ret = inquire_ret_t(true, id(), builder);

  if (!found) {
    return ret;
  }

  // Check ongoing ones:
  for (auto const& s : VPackArrayIterator(query->slice())) {
    std::string ss = s.copyString();
    if (isTrxOngoing(ss)) {
      ret.result->clear();
      ret.result->add(VPackValue("ongoing"));
    }
  }

  return ret;
}


/// Write new entries to replicated state and store
write_ret_t Agent::write(query_t const& query) {

  std::vector<bool> applied;
  std::vector<index_t> indices;
  auto multihost = size()>1;

  auto leader = _constituent.leaderID();
  if (multihost && leader != id()) {
    return write_ret_t(false, leader);
  }
  
  addTrxsOngoing(query->slice());    // remember that these are ongoing

  // Apply to spearhead and get indices for log entries
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    
    // Only leader else redirect
    if (multihost && challengeLeadership()) {
      _constituent.candidate();
      return write_ret_t(false, NO_LEADER);
    }
    
    applied = _spearhead.apply(query);
    indices = _state.log(query, applied, term());
  }

  removeTrxsOngoing(query->slice());

  // Maximum log index
  index_t maxind = 0;
  if (!indices.empty()) {
    maxind = *std::max_element(indices.begin(), indices.end());
  }

  // Report that leader has persisted
  reportIn(id(), maxind);

  return write_ret_t(true, id(), applied, indices);
}

/// Read from store
read_ret_t Agent::read(query_t const& query) {

  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return read_ret_t(false, leader);
  }

  MUTEX_LOCKER(ioLocker, _ioLock);

  // Only leader else redirect
  if (challengeLeadership()) {
    _constituent.candidate();
    return read_ret_t(false, NO_LEADER);
  }

  // Retrieve data from readDB
  auto result = std::make_shared<arangodb::velocypack::Builder>();
  std::vector<bool> success = _readDB.read(query, result);

  return read_ret_t(true, _constituent.leaderID(), success, result);
  
}


/// Send out append entries to followers regularly or on event
void Agent::run() {

  CONDITION_LOCKER(guard, _appendCV);
  using namespace std::chrono;
  auto tp = system_clock::now();

  // Only run in case we are in multi-host mode
  while (!this->isStopping() && size() > 1) {

    // Leader working only
    if (leading()) {

      // Append entries to followers
      sendAppendEntriesRPC();

      // Don't panic
      _appendCV.wait(100);

      // Detect faulty agent and replace
      // if possible and only if not already activating
      if (duration<double>(system_clock::now() - tp).count() > 10.0) {
        detectActiveAgentFailures();
        tp = system_clock::now();
      }

    } else {
      _appendCV.wait(1000000);
    }

  }
  
}


void Agent::reportActivated(
  std::string const& failed, std::string const& replacement, query_t state) {

  term_t myterm;
      
  if (state->slice().get("success").getBoolean()) {
    
    {
      MUTEX_LOCKER(ioLocker, _ioLock);
      _confirmed.erase(failed);
      auto commitIndex = state->slice().get("commitId").getNumericValue<index_t>();
      _confirmed[replacement] = commitIndex;
      _lastAcked[replacement] = system_clock::now();
      _config.swapActiveMember(failed, replacement);
      myterm = _constituent.term();
    }
    
    {
      MUTEX_LOCKER(actLock, _activatorLock);
      if (_activator->isRunning()) {
        _activator->beginShutdown();
      }
      _activator.reset(nullptr);
    }
    
  } else {
    MUTEX_LOCKER(ioLocker, _ioLock);
    myterm = _constituent.term();
  }

  persistConfiguration(myterm);

  // Notify inactive pool
  notifyInactive();

}


void Agent::persistConfiguration(term_t t) {

  // Agency configuration
  auto agency = std::make_shared<Builder>();
  { VPackArrayBuilder trxs(agency.get());
    { VPackArrayBuilder trx(agency.get());
      { VPackObjectBuilder oper(agency.get());
        agency->add(VPackValue(".agency"));
        { VPackObjectBuilder a(agency.get());
          agency->add("term", VPackValue(t));
          agency->add("id", VPackValue(id()));
          agency->add("active", _config.activeToBuilder()->slice());
          agency->add("pool", _config.poolToBuilder()->slice());
          agency->add("size", VPackValue(size()));
        }}}}
  
  // In case we've lost leadership, no harm will arise as the failed write
  // prevents bogus agency configuration to be replicated among agents. ***
  write(agency); 

}


void Agent::failedActivation(
  std::string const& failed, std::string const& replacement) {
  MUTEX_LOCKER(actLock, _activatorLock);
  _activator.reset(nullptr);
}


void Agent::detectActiveAgentFailures() {
  // Detect faulty agent if pool larger than agency

  std::map<std::string, TimePoint> lastAcked;
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    lastAcked = _lastAcked;
  }

  MUTEX_LOCKER(actLock, _activatorLock);
  if (_activator != nullptr) {
    return;
  }
  
  if (_config.poolSize() > _config.size()) {
    std::vector<std::string> active = _config.active();
    for (auto const& id : active) {
      if (id != this->id()) {
        auto ds = duration<double>(
          system_clock::now() - lastAcked.at(id)).count();
        if (ds > 180.0) {
          std::string repl = _config.nextAgentInLine();
          LOG_TOPIC(DEBUG, Logger::AGENCY)
            << "Active agent " << id << " has failed. << " << repl
            << " will be promoted to active agency membership";
          _activator = std::make_unique<AgentActivator>(this, id, repl);
          _activator->start();
          return;
        }
      }
    }
  }
}


/// Orderly shutdown
void Agent::beginShutdown() {
  Thread::beginShutdown();

  // Stop constituent and key value stores
  _constituent.beginShutdown();

  // Stop supervision
  if (_config.supervision()) {
    _supervision.beginShutdown();
  }

  // Stop inception process
  if (_inception != nullptr) {
    _inception->beginShutdown();
  } 

  // Compactor
  _compactor.beginShutdown();

  // Stop key value stores
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


bool Agent::prepareLead() {
  
  // Key value stores
  try {
    rebuildDBs();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to rebuild key value stores." << e.what();
    return false;
  }
  
  // Reset last acknowledged
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    for (auto const& i : _config.active()) {
      _lastAcked[i] = system_clock::now();
    }
    _leaderSince = system_clock::now();
  }
  
  return true; 
  
}

/// Becoming leader
void Agent::lead() {

  // Wake up run
  {
    CONDITION_LOCKER(guard, _appendCV);
    guard.broadcast();
  }


  // Agency configuration
  term_t myterm;
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    myterm = _constituent.term();
  }

  persistConfiguration(myterm);

  // Notify inactive pool
  notifyInactive();

}

// When did we take on leader ship?
TimePoint const& Agent::leaderSince() const {
  return _leaderSince;
}

// Notify inactive pool members of configuration change()
void Agent::notifyInactive() const {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return;
  }

  std::map<std::string, std::string> pool = _config.pool();
  std::string path = "/_api/agency_priv/inform";

  Builder out;
  { VPackObjectBuilder o(&out);
    out.add("term", VPackValue(term()));
    out.add("id", VPackValue(id()));
    out.add("active", _config.activeToBuilder()->slice());
    out.add("pool", _config.poolToBuilder()->slice());
    out.add("min ping", VPackValue(_config.minPing()));
    out.add("max ping", VPackValue(_config.maxPing())); }

  for (auto const& p : pool) {
    if (p.first != id()) {
      auto headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
      cc->asyncRequest(
        "1", 1, p.second, arangodb::rest::RequestType::POST,
        path, std::make_shared<std::string>(out.toJson()), headerFields,
        nullptr, 1.0, true);
    }
  }

}


void Agent::updatePeerEndpoint(query_t const& message) {

  VPackSlice slice = message->slice();

  if (!slice.isObject() || slice.length() == 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT,
      std::string("Inproper greeting: ") + slice.toJson());
  }

  std::string uuid, endpoint;
  try {
    uuid = slice.keyAt(0).copyString();
  } catch (std::exception const& e) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT,
      std::string("Cannot deal with UUID: ") + e.what());
  }

  try {
    endpoint = slice.valueAt(0).copyString();
  } catch (std::exception const& e) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT,
      std::string("Cannot deal with UUID: ") + e.what());
  }

  updatePeerEndpoint(uuid, endpoint);
  
}

void Agent::updatePeerEndpoint(std::string const& id, std::string const& ep) {
  
  if (_config.updateEndpoint(id, ep)) {
    if (!challengeLeadership()) {
      persistConfiguration(term());
      notifyInactive();
    }
  }
  
}

void Agent::notify(query_t const& message) {
  VPackSlice slice = message->slice();

  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT,
        std::string("Inform message must be an object. Incoming type is ") +
            slice.typeName());
  }

  if (!slice.hasKey("id") || !slice.get("id").isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID);
  }
  if (!slice.hasKey("term")) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM);
  }
  _constituent.update(slice.get("id").copyString(),
                      slice.get("term").getUInt());

  if (!slice.hasKey("active") || !slice.get("active").isArray()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE);
  }
  if (!slice.hasKey("pool") || !slice.get("pool").isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL);
  }
  if (!slice.hasKey("min ping") || !slice.get("min ping").isNumber()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL);
  }
  if (!slice.hasKey("max ping") || !slice.get("max ping").isNumber()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL);
  }

  _config.update(message);

  _state.persistActiveAgents(_config.activeToBuilder(), _config.poolToBuilder());
  
}

// Rebuild key value stores
arangodb::consensus::index_t Agent::rebuildDBs() {

  MUTEX_LOCKER(ioLocker, _ioLock);
  MUTEX_LOCKER(liLocker, _liLock);

  // Apply logs from last applied index to leader's commit index
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Rebuilding key-value stores from index "
    << _lastCompactionIndex << " to " << _leaderCommitIndex << " " << _state;

  auto logs = _state.slices(_lastCompactionIndex+1);

  _spearhead.clear();
  _spearhead.apply(logs, _leaderCommitIndex, _constituent.term());
  _readDB.clear();
  _readDB.apply(logs, _leaderCommitIndex, _constituent.term());
  LOG_TOPIC(TRACE, Logger::AGENCY) << "ReadDB: " << _readDB;

    
  _lastAppliedIndex = _leaderCommitIndex;
  //_lastCompactionIndex = _leaderCommitIndex;
  
  LOG_TOPIC(INFO, Logger::AGENCY)
    << id() << " rebuilt key-value stores - serving.";

  return _lastAppliedIndex;

}


/// Compact read db
void Agent::compact() {
  rebuildDBs();
  _state.compact(_lastAppliedIndex-_config.compactionKeepSize());
  _nextCompactionAfter += _config.compactionStepSize();
}


/// Last commit index
std::pair<arangodb::consensus::index_t, arangodb::consensus::index_t>
  Agent::lastCommitted() const {
  MUTEX_LOCKER(ioLocker, _ioLock);
  return std::pair<arangodb::consensus::index_t, arangodb::consensus::index_t>(
    _lastCommitIndex,_leaderCommitIndex);
}

/// Last commit index
void Agent::lastCommitted(arangodb::consensus::index_t lastCommitIndex) {
  MUTEX_LOCKER(ioLocker, _ioLock);
  _lastCommitIndex = lastCommitIndex;
  MUTEX_LOCKER(liLocker, _liLock);
  _leaderCommitIndex = lastCommitIndex;
}

/// Last log entry
log_t Agent::lastLog() const { return _state.lastLog(); }

/// Get spearhead
Store const& Agent::spearhead() const { return _spearhead; }

/// Get readdb
Store const& Agent::readDB() const { return _readDB; }

/// Get readdb
arangodb::consensus::index_t Agent::readDB(Node& node) const {
  MUTEX_LOCKER(ioLocker, _ioLock);
  node = _readDB.get();
  return _lastCommitIndex;
}

/// Get transient
Store const& Agent::transient() const { return _transient; }

/// Rebuild from persisted state
Agent& Agent::operator=(VPackSlice const& compaction) {
  // Catch up with compacted state
  MUTEX_LOCKER(ioLocker, _ioLock);
  _spearhead = compaction.get("readDB");
  _readDB = compaction.get("readDB");

  // Catch up with commit
  try {
    _lastCommitIndex = std::stoul(compaction.get("_key").copyString());
    MUTEX_LOCKER(liLocker, _liLock);
    _leaderCommitIndex = _lastCommitIndex;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }

  // Schedule next compaction
  _nextCompactionAfter = _lastCommitIndex + _config.compactionStepSize();

  return *this;
}

/// Are we still starting up?
bool Agent::booting() { return (!_config.poolComplete()); }

/// We expect an object as follows {id:<id>,endpoint:<endpoint>,pool:{...}}
/// key: uuid value: endpoint
/// Lock configuration and compare
/// Add whatever is missing in our list.
/// Compare whatever is in our list already. (ASSERT identity)
/// If I know more immediately contact peer with my list.
query_t Agent::gossip(query_t const& in, bool isCallback, size_t version) {

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Incoming gossip: "
      << in->slice().toJson();

  VPackSlice slice = in->slice();
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20001,
        std::string("Gossip message must be an object. Incoming type is ") +
            slice.typeName());
  }

  if (!slice.hasKey("id") || !slice.get("id").isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20002, "Gossip message must contain string parameter 'id'");
  }

  if (!slice.hasKey("endpoint") || !slice.get("endpoint").isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20003, "Gossip message must contain string parameter 'endpoint'");
  }
  std::string endpoint = slice.get("endpoint").copyString();
  if (isCallback) {
    _inception->reportVersionForEp(endpoint, version);
  }

  LOG_TOPIC(TRACE, Logger::AGENCY)
      << "Gossip " << ((isCallback) ? "callback" : "call") << " from "
      << endpoint;

  if (!slice.hasKey("pool") || !slice.get("pool").isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20003, "Gossip message must contain object parameter 'pool'");
  }
  VPackSlice pslice = slice.get("pool");

  if (slice.hasKey("active") && slice.get("active").isArray()) {
    for (auto const& a : VPackArrayIterator(slice.get("active"))) {
      _config.activePushBack(a.copyString());
    }
  }

  LOG_TOPIC(TRACE, Logger::AGENCY) << "Received gossip " << slice.toJson();

  std::map<std::string, std::string> incoming;
  for (auto const& pair : VPackObjectIterator(pslice)) {
    if (!pair.value.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          20004, "Gossip message pool must contain string parameters");
    }
    incoming[pair.key.copyString()] = pair.value.copyString();
  }

  query_t out = std::make_shared<Builder>();

  {  
    VPackObjectBuilder b(out.get());
    
    std::vector<std::string> gossipPeers = _config.gossipPeers();
    if (!gossipPeers.empty()) {
      try {
        _config.eraseFromGossipPeers(endpoint);
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << __FILE__ << ":" << __LINE__ << " " << e.what();
      }
    }
    
    for (auto const& i : incoming) {
      
      /// disagreement over pool membership: fatal!
      if (!_config.addToPool(i)) {
        LOG_TOPIC(FATAL, Logger::AGENCY) << "Discrepancy in agent pool!";
        FATAL_ERROR_EXIT();
      }
      
    }
    
    if (!isCallback) { // no gain in callback to a callback.
      auto pool = _config.pool();
      auto active = _config.active();

      // Wrapped in envelope in RestAgencyPriveHandler
      out->add(VPackValue("pool"));
      {
        VPackObjectBuilder bb(out.get());
        for (auto const& i : pool) {
          out->add(i.first, VPackValue(i.second));
        }
      }
      out->add(VPackValue("active"));
      {
        VPackArrayBuilder bb(out.get());
        for (auto const& i : active) {
          out->add(VPackValue(i));
        }
      }
    }
  }
  
  LOG_TOPIC(TRACE, Logger::AGENCY) << "Answering with gossip "
                                   << out->slice().toJson();
  return out;
}


void Agent::reportMeasurement(query_t const& query) {
  if (_inception != nullptr) {
    _inception->reportIn(query);
  }
}

void Agent::resetRAFTTimes(double min_timeout, double max_timeout) {
  _config.pingTimes(min_timeout,max_timeout);
}

void Agent::ready(bool b) {
  // From main thread of Inception
  _ready = b;
}


bool Agent::ready() const {

  if (size() == 1) {
    return true;
  }

  return _ready;

}

query_t Agent::buildDB(arangodb::consensus::index_t index) {

  auto builder = std::make_shared<VPackBuilder>();
  arangodb::consensus::index_t start = 0, end = 0;

  Store store(this);
  {

    MUTEX_LOCKER(ioLocker, _ioLock);
    store = _compacted;

    MUTEX_LOCKER(liLocker, _liLock);
    end = _leaderCommitIndex;
    start = _lastCompactionIndex+1;
    
  }
  
  if (index > end) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Cannot snapshot beyond leaderCommitIndex: " << end;
    index = end;
  } else if (index < start) {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << "Cannot snapshot before last compaction index: " << start;
    index = start+1;
  }
  
  store.apply(_state.slices(start+1, index), index, _constituent.term());
  store.toBuilder(*builder);
  
  return builder;
  
}

void Agent::addTrxsOngoing(Slice trxs) {
  try {
    MUTEX_LOCKER(guard,_trxsLock);
    for (auto const& trx : VPackArrayIterator(trxs)) {
      if (trx[0].isObject() && trx.length() == 3 && trx[2].isString()) {
        // only those are interesting:
        _ongoingTrxs.insert(trx[2].copyString());
      }
    }
  } catch (...) {
  }
}

void Agent::removeTrxsOngoing(Slice trxs) {
  try {
    MUTEX_LOCKER(guard, _trxsLock);
    for (auto const& trx : VPackArrayIterator(trxs)) {
      if (trx[0].isObject() && trx.length() == 3 && trx[2].isString()) {
        // only those are interesting:
        _ongoingTrxs.erase(trx[2].copyString());
      }
    }
  } catch (...) {
  }
}

bool Agent::isTrxOngoing(std::string& id) {
  try {
    MUTEX_LOCKER(guard, _trxsLock);
    auto it = _ongoingTrxs.find(id);
    return it != _ongoingTrxs.end();
  } catch (...) {
    return false;
  }
}

Inception const* Agent::inception() const {
  return _inception.get();
}

}}  // namespace
