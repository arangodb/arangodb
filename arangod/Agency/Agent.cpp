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
      _spearhead(this),
      _readDB(this),
      _nextCompationAfter(_config.compactionStepSize()),
      _inception(std::make_unique<Inception>(this)),
      _activator(nullptr),
      _ready(false) {
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
  return _constituent.leading();
}

/// Start constituent personality
void Agent::startConstituent() {
  activateAgency();
}

// Waits here for confirmation of log's commits up to index. Timeout in seconds.
bool Agent::waitFor(index_t index, double timeout) {
  if (size() == 1) {  // single host agency
    return true;
  }

  // Get condition variable to notice commits
  CONDITION_LOCKER(guard, _waitForCV);

  // Wait until woken up through AgentCallback
  while (true) {
    /// success?
    {
      MUTEX_LOCKER(lockIndex, _ioLock);
      if (_lastCommitIndex >= index) {
        return true;
      }
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
void Agent::reportIn(std::string const& id, index_t index) {

  {
    // Enforce _lastCommitIndex, _readDB and compaction to progress atomically
    MUTEX_LOCKER(mutexLocker, _ioLock);

    // Update last acknowledged answer
    _lastAcked[id] = system_clock::now();

    if (index > _confirmed[id]) {  // progress this follower?
      _confirmed[id] = index;
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
        _lastCommitIndex = index;

        if (_lastCommitIndex >= _nextCompationAfter) {
          _state.compact(_lastCommitIndex);
          _nextCompationAfter += _config.compactionStepSize();
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

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Got AppendEntriesRPC from "
    << leaderId << " with term " << term;

  // Update commit index
  if (queries->slice().type() != VPackValueType::Array) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "Received malformed entries for appending. Discarding!";
    return false;
  }

  if (!_constituent.checkLeader(term, leaderId, prevIndex, prevTerm)) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "Not accepting appendEntries from "
      << leaderId;
    return false;
  }

  size_t nqs = queries->slice().length();

  // State machine, _lastCommitIndex to advance atomically
  MUTEX_LOCKER(mutexLocker, _ioLock);

  if (nqs > 0) {
    size_t ndups = _state.removeConflicts(queries);

    if (nqs > ndups) {
      LOG_TOPIC(TRACE, Logger::AGENCY)
        << "Appending " << nqs - ndups << " entries to state machine. ("
        << nqs << ", " << ndups << ")";

      try {
        _state.log(queries, ndups);
      } catch (std::exception const&) {
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Malformed query: " << __FILE__ << __LINE__;
      }
    }
  }


  _spearhead.apply(
    _state.slices(_lastCommitIndex + 1, leaderCommitIndex), _lastCommitIndex,
    _constituent.term());
  
  _readDB.apply(
    _state.slices(_lastCommitIndex + 1, leaderCommitIndex), _lastCommitIndex,
    _constituent.term());

  _lastCommitIndex = leaderCommitIndex;

  if (_lastCommitIndex >= _nextCompationAfter) {
    _state.compact(_lastCommitIndex);
    _nextCompationAfter += _config.compactionStepSize();
  }

  return true;
}

/// Leader's append entries
void Agent::sendAppendEntriesRPC() {

  // _lastSent, _lastHighest and _confirmed only accessed in main thread
  std::string const myid = id();
  
  for (auto const& followerId : _config.active()) {

    if (followerId != myid) {

      term_t t(0);

      index_t last_confirmed, lastCommitIndex;
      {
        MUTEX_LOCKER(mutexLocker, _ioLock);
        t = this->term();
        last_confirmed = _confirmed[followerId];
        lastCommitIndex = _lastCommitIndex;
      }

      std::vector<log_t> unconfirmed = _state.get(last_confirmed);

      if (unconfirmed.empty()) {
        // this can only happen if the log is totally empty (I think, Max)
        // and so it is OK, to skip the time check here
        continue;
      }

      index_t highest = unconfirmed.back().index;

      // _lastSent, _lastHighest: local and single threaded access
      duration<double> m = system_clock::now() - _lastSent[followerId];

      if (highest == _lastHighest[followerId] &&
          m.count() < 0.5 * _config.minPing()) {
        continue;
      }

      // RPC path
      std::stringstream path;
      path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId="
           << id() << "&prevLogIndex=" << unconfirmed.front().index
           << "&prevLogTerm=" << unconfirmed.front().term << "&leaderCommit="
           << lastCommitIndex;

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
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Appending " << unconfirmed.size() - 1 << " entries up to index "
          << highest << " to follower " << followerId;
      }

      // Send request
      auto headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
      arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, _config.poolAt(followerId),
        arangodb::rest::RequestType::POST, path.str(),
        std::make_shared<std::string>(builder.toJson()), headerFields,
        std::make_shared<AgentCallback>(this, followerId, highest),
        0.7 * _config.minPing(), true);

      // _lastSent, _lastHighest: local and single threaded access
      _lastSent[followerId] = system_clock::now();
      _lastHighest[followerId] = highest;

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
        MUTEX_LOCKER(mutexLocker, _ioLock); // Atomicity 
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
      persisted = _state.persistActiveAgents(_config.activeToBuilder(),
                                             _config.poolToBuilder());
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
    MUTEX_LOCKER(mutexLocker, _ioLock);
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

  if (!_constituent.leading()) {
    return trans_ret_t(false, _constituent.leaderID());
  }

  // Apply to spearhead and get indices for log entries
  auto qs = queries->slice();
  auto ret = std::make_shared<arangodb::velocypack::Builder>();
  size_t failed = 0;
  ret->openArray();
  {
    
    MUTEX_LOCKER(mutexLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      return trans_ret_t(false, NO_LEADER);
    }
    
    for (const auto& query : VPackArrayIterator(qs)) {
      if (query[0].isObject()) {
        if(_spearhead.apply(query)) {
          maxind = _state.log(query[0], term());
          ret->add(VPackValue(maxind));
        } else {
          ret->add(VPackValue(0));
          ++failed;
        }
      } else if (query[0].isString()) {
        _spearhead.read(query, *ret);
      }
    }
    
    // (either no writes or all preconditions failed)
    if (maxind == 0) {
      ret->clear();
      ret->openArray();      
      for (const auto& query : VPackArrayIterator(qs)) {
        if (query[0].isObject()) {
          ret->add(VPackValue(0));
        } else if (query[0].isString()) {
          _readDB.read(query, *ret);
        }
      }
    }
    
  }
  ret->close();
  
  // Report that leader has persisted
  reportIn(id(), maxind);

  return trans_ret_t(true, id(), maxind, failed, ret);
}

/// Write new entries to replicated state and store
write_ret_t Agent::write(query_t const& query) {
  std::vector<bool> applied;
  std::vector<index_t> indices;

  if (!_constituent.leading()) {
    return write_ret_t(false, _constituent.leaderID());
  }
  
  // Apply to spearhead and get indices for log entries
  {
    MUTEX_LOCKER(mutexLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      return write_ret_t(false, NO_LEADER);
    }
    
    applied = _spearhead.apply(query);
    indices = _state.log(query, applied, term());
    
  }

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
  if (!_constituent.leading()) {
    return read_ret_t(false, _constituent.leaderID());
  }
  
  MUTEX_LOCKER(mutexLocker, _ioLock);

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
      _appendCV.wait(1000);

      // Append entries to followers
      sendAppendEntriesRPC();

      // Detect faulty agent and replace
      // if possible and only if not already activating
      if (duration<double>(system_clock::now() - tp).count() > 10.0) {
        detectActiveAgentFailures();
        tp = system_clock::now();
      }

    } else {
      _appendCV.wait(1000000);
      updateConfiguration();
    }

  }
  
}


void Agent::reportActivated(
  std::string const& failed, std::string const& replacement, query_t state) {

  term_t myterm;
      
  if (state->slice().get("success").getBoolean()) {
    
    {
      MUTEX_LOCKER(mutexLocker, _ioLock);
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
    MUTEX_LOCKER(mutexLocker, _ioLock);
    myterm = _constituent.term();
  }

  // Agency configuration
  auto agency = std::make_shared<Builder>();
  agency->openArray();
  agency->openArray();
  agency->openObject();
  agency->add(".agency", VPackValue(VPackValueType::Object));
  agency->add("term", VPackValue(myterm));
  agency->add("id", VPackValue(id()));
  agency->add("active", _config.activeToBuilder()->slice());
  agency->add("pool", _config.poolToBuilder()->slice());
  agency->close();
  agency->close();
  agency->close();
  agency->close();
  write(agency);

  // Notify inactive pool
  notifyInactive();

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
    MUTEX_LOCKER(mutexLocker, _ioLock);
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
          LOG_TOPIC(DEBUG, Logger::AGENCY) << "Active agent " << id << " has failed. << "
                    << repl << " will be promoted to active agency membership";
          // Guarded in ::
          _activator = std::make_unique<AgentActivator>(this, id, repl);
          _activator->start();
          return;
        }
      }
    }
  }
}


void Agent::updateConfiguration() {

  // First ask last know leader

}


/// Orderly shutdown
void Agent::beginShutdown() {
  Thread::beginShutdown();

  // Stop supervision
  if (_config.supervision()) {
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

  term_t myterm;
  // Reset last acknowledged
  {
    MUTEX_LOCKER(mutexLocker, _ioLock);
    for (auto const& i : _config.active()) {
      _lastAcked[i] = system_clock::now();
    }
    myterm = _constituent.term();
  }

  // Agency configuration
  auto agency = std::make_shared<Builder>();
  agency->openArray();
  agency->openArray();
  agency->openObject();
  agency->add(".agency", VPackValue(VPackValueType::Object));
  agency->add("term", VPackValue(myterm));
  agency->add("id", VPackValue(id()));
  agency->add("active", _config.activeToBuilder()->slice());
  agency->add("pool", _config.poolToBuilder()->slice());
  agency->close();
  agency->close();
  agency->close();
  agency->close();
  write(agency);

  // Wake up supervision
  _supervision.wakeUp();

  // Notify inactive pool
  notifyInactive();

  return true;
}

// Notify inactive pool members of configuration change()
void Agent::notifyInactive() const {
  if (_config.poolSize() > _config.size()) {

    std::map<std::string, std::string> pool = _config.pool();
    std::string path = "/_api/agency_priv/inform";

    Builder out;
    out.openObject();
    out.add("term", VPackValue(term()));
    out.add("id", VPackValue(id()));
    out.add("active", _config.activeToBuilder()->slice());
    out.add("pool", _config.poolToBuilder()->slice());
    out.close();

    for (auto const& p : pool) {

      if (p.first != id()) {
        auto headerFields =
          std::make_unique<std::unordered_map<std::string, std::string>>();

        arangodb::ClusterComm::instance()->asyncRequest(
          "1", 1, p.second, arangodb::rest::RequestType::POST,
          path, std::make_shared<std::string>(out.toJson()), headerFields,
          nullptr, 1.0, true);
      }

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

  _config.update(message);
  _state.persistActiveAgents(_config.activeToBuilder(),
                             _config.poolToBuilder());
}

// Rebuild key value stores
bool Agent::rebuildDBs() {

  MUTEX_LOCKER(mutexLocker, _ioLock);

  _spearhead.apply(_state.slices(_lastCommitIndex + 1), _lastCommitIndex,
                   _constituent.term());
  _readDB.apply(_state.slices(_lastCommitIndex + 1), _lastCommitIndex,
                _constituent.term());

  return true;
}

/// Last commit index
arangodb::consensus::index_t Agent::lastCommitted() const {
  MUTEX_LOCKER(mutexLocker, _ioLock);
  return _lastCommitIndex;
}

/// Last commit index
void Agent::lastCommitted(arangodb::consensus::index_t lastCommitIndex) {
  MUTEX_LOCKER(mutexLocker, _ioLock);
  _lastCommitIndex = lastCommitIndex;
}

/// Last log entry
log_t Agent::lastLog() const { return _state.lastLog(); }

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
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }

  // Schedule next compaction
  _nextCompationAfter = _lastCommitIndex + _config.compactionStepSize();

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
    
    size_t counter = 0;
    for (auto const& i : incoming) {
      
      /// more data than pool size: fatal!
      if (++counter > _config.poolSize()) {
        LOG_TOPIC(FATAL, Logger::AGENCY)
          << "Too many peers in pool: " << counter << ">" << _config.poolSize();
        FATAL_ERROR_EXIT();
      }
      
      /// disagreement over pool membership: fatal!
      if (!_config.addToPool(i)) {
        LOG_TOPIC(FATAL, Logger::AGENCY) << "Discrepancy in agent pool!";
        FATAL_ERROR_EXIT();
      }
      
    }
    
    if (!isCallback) { // no gain in callback to a callback.
      std::map<std::string, std::string> pool = _config.pool();
      
      out->add("endpoint", VPackValue(_config.endpoint()));
      out->add("id", VPackValue(_config.id()));
      out->add(VPackValue("pool"));
      {
        VPackObjectBuilder bb(out.get());
        for (auto const& i : pool) {
          out->add(i.first, VPackValue(i.second));
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


}}  // namespace
