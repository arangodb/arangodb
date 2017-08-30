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
    _commitIndex(0),
    _lastApplied(0),
    _spearhead(this),
    _readDB(this),
    _transient(this),
    _nextCompactionAfter(_config.compactionStepSize()),
    _activator(nullptr),
    _compactor(this),
    _ready(false),
    _preparing(false) {
  _state.configure(this);
  _constituent.configure(this);
  if (size() > 1) {
    _inception = std::make_unique<Inception>(this);
  }
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
    index_t lastLogTerm, query_t const& query, int64_t timeoutMult) {

  if (timeoutMult != -1 && timeoutMult != _config._timeoutMult) {
    adjustTimeoutMult(timeoutMult);
    LOG_TOPIC(WARN, Logger::AGENCY) << "Voter: setting timeout multiplier to "
      << timeoutMult << " for next term.";
  }

  bool doIVote = _constituent.vote(termOfPeer, id, lastLogIndex, lastLogTerm);
  return priv_rpc_ret_t(doIVote, this->term());
}

/// Get copy of momentary configuration
config_t const Agent::config() const {
  return _config;
}

/// Adjust timeoutMult:
void Agent::adjustTimeoutMult(int64_t timeoutMult) {
  _config.setTimeoutMult(timeoutMult);
}

/// Get timeoutMult:
int64_t Agent::getTimeoutMult() const {
  return _config.timeoutMult();
}

/// Leader's id
std::string Agent::leaderID() const {
  return _constituent.leaderID();
}

/// Are we leading?
bool Agent::leading() const {
  // When we become leader, we first are officially still a follower, but
  // prepare for the leading. This is indicated by the _preparing flag in the
  // Agent, the Constituent stays with role FOLLOWER for now. The agent has
  // to send out AppendEntriesRPC calls immediately, but only when we are
  // properly leading (with initialized stores etc.) can we execute requests.
  return (_preparing && _constituent.following()) || _constituent.leading();
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
      _liLock.assertNotLockedByCurrentThread();
      MUTEX_LOCKER(lockIndex, _ioLock);
      if (_commitIndex >= index) {
        return Agent::raft_commit_t::OK;
      }
    }

    // timeout
    if (!_waitForCV.wait(static_cast<uint64_t>(1.0e6 * timeout))) {
      if (leading()) {
        _liLock.assertNotLockedByCurrentThread();
        MUTEX_LOCKER(lockIndex, _ioLock);
        return (_commitIndex >= index) ?
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

  auto startTime = system_clock::now();

  {
    // Enforce _lastCommitIndex, _readDB and compaction to progress atomically
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);

    // Update last acknowledged answer
    auto t = system_clock::now();
    std::chrono::duration<double> d = t - _lastAcked[peerId];
    if (peerId != id() && d.count() > _config._minPing * _config._timeoutMult) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Last confirmation from peer "
        << peerId << " was received more than minPing ago: " << d.count();
    }
    _lastAcked[peerId] = t;

    if (index > _confirmed[peerId]) {  // progress this follower?
      _confirmed[peerId] = index;
      if (toLog > 0) { // We want to reset the wait time only if a package callback
        LOG_TOPIC(TRACE, Logger::AGENCY) << "Got call back of " << toLog << " logs";
        _earliestPackage[peerId] = system_clock::now();
      }
    }

    if (index > _commitIndex) {  // progress last commit?

      size_t n = 0;

      for (auto const& i : _config.active()) {
        n += (_confirmed[i] >= index);
      }

      // catch up read database and commit index
      if (n > size() / 2) {

        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Critical mass for commiting " << _commitIndex + 1
          << " through " << index << " to read db";
        {
          MUTEX_LOCKER(mutexLocker, _compactionLock);
          _readDB.applyLogEntries(
            _state.slices(
              _commitIndex + 1, index), _commitIndex, _constituent.term(),
              true /* inform others by callbacks */ );
        }

        // TODO: why _liLock here, should by _ioLock, and we already have it
        MUTEX_LOCKER(liLocker, _liLock);
        _commitIndex = index;
        if (_commitIndex >= _nextCompactionAfter) {
          _compactor.wakeUp();
        }

      }

    }
  } // MUTEX_LOCKER

  duration<double> reportInTime = system_clock::now() - startTime;
  if (reportInTime.count() > 0.1) {
    LOG_TOPIC(WARN, Logger::AGENCY)
      << "reportIn took too long: " << reportInTime.count();
  }

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

  VPackSlice payload = queries->slice();

  // Update commit index
  if (payload.type() != VPackValueType::Array) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Received malformed entries for appending. Discarding!";
    return false;
  }

  if (!_constituent.checkLeader(term, leaderId, prevIndex, prevTerm)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Not accepting appendEntries from " << leaderId;
    return false;
  }

  // Check whether we have got a snapshot in the first position:
  bool gotSnapshot = payload.length() > 0 &&
                     payload[0].isObject() &&
                     !payload[0].get("readDB").isNone();

  // In case of a snapshot, there are three possibilities:
  //   1. Our highest log index is smaller than the snapshot index, in this 
  //      case we must throw away our complete local log and start from the
  //      snapshot (note that snapshot indexes are always committed by a 
  //      majority).
  //   2. For the snapshot index we have an entry with this index in 
  //      our log (and it is not yet compacted), in this case we verify
  //      that the terms match and if so, we can simply ignore the
  //      snapshot. If the term in our log entry is smaller (cannot be
  //      larger because compaction snapshots are always committed), then
  //      our complete log must be deleted as in 1.
  //   3. Our highest log index is larger than the snapshot index but we
  //      no longer have an entry in the log for the snapshot index due to
  //      our own compaction. In this case we have compacted away the
  //      snapshot index, therefore we know it was committed by a majority
  //      and thus the snapshot can be ignored safely as well.
  if (gotSnapshot) {
    bool useSnapshot = false;   // if this remains, we ignore the snapshot

    index_t snapshotIndex
        = static_cast<index_t>(payload[0].get("index").getNumber<index_t>());
    term_t snapshotTerm
        = static_cast<term_t>(payload[0].get("term").getNumber<index_t>());
    index_t ourLastIndex = _state.lastIndex();
    if (ourLastIndex < snapshotIndex) {
      useSnapshot = true;   // this implies that we completely eradicate our log
    } else {
      try {
        log_t logEntry = _state.at(snapshotIndex);
        if (logEntry.term != snapshotTerm) {  // can only be < as in 2.
          useSnapshot = true;
        }
      } catch (...) {
        // Simply ignore that we no longer have the entry, useSnapshot remains
        // false and we will ignore the snapshot as in 3. above
      }
    }
    if (useSnapshot) {
      // Now we must completely erase our log and compaction snapshots and
      // start from the snapshot
      Store snapshot(this, "snapshot");
      snapshot = payload[0].get("readDB");
      if (!_state.restoreLogFromSnapshot(snapshot, snapshotIndex, snapshotTerm)) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << "Could not restore received log snapshot.";
        return false;
      }
      // Now the log is empty, but this will soon be rectified.
      { 
        MUTEX_LOCKER(ioLocker, _ioLock);
        _nextCompactionAfter = (std::min)(_nextCompactionAfter,
            snapshotIndex + _config.compactionStepSize());
      }
    }
  }

  size_t nqs = payload.length();

  // State machine, _lastCommitIndex to advance atomically
  bool ok = true;
  if (nqs > 0) {
    
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);
  
    size_t ndups = _state.removeConflicts(queries, gotSnapshot);
    
    if (nqs > ndups) {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Appending " << nqs - ndups << " entries to state machine. ("
        << nqs << ", " << ndups << "): " << payload.toJson() ;
      
      try {

        MUTEX_LOCKER(ioLocker, _liLock);
        _lastApplied = _state.log(queries, ndups);
        if (_lastApplied < payload[nqs-1].get("index").getNumber<index_t>()) {
          // We could not log all the entries in this query, we need to report
          // this to the leader!
          ok = false;
        }
        
      } catch (std::exception const& e) {
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Exception during log append: " << __FILE__ << __LINE__
          << " " << e.what();
      }
      
    }
  }

  bool wakeup;
  {
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);
    _commitIndex = std::min(leaderCommitIndex, _lastApplied);

    wakeup = (_commitIndex >= _nextCompactionAfter);
  }
 
  if (wakeup) {
    _compactor.wakeUp();
  }

  return ok;
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

      index_t lastConfirmed, commitIndex;
      auto startTime = system_clock::now();
      time_point<system_clock> earliestPackage, lastAcked;
      
      {
        _liLock.assertNotLockedByCurrentThread();
        MUTEX_LOCKER(ioLocker, _ioLock);
        t = this->term();
        lastConfirmed = _confirmed[followerId];
        commitIndex = _commitIndex;
        lastAcked = _lastAcked[followerId];
        earliestPackage = _earliestPackage[followerId];
      }
      duration<double> lockTime = system_clock::now() - startTime;
      if (lockTime.count() > 0.1) {
        LOG_TOPIC(WARN, Logger::AGENCY)
          << "Reading lastConfirmed took too long: " << lockTime.count();
      }

      std::vector<log_t> unconfirmed = _state.get(lastConfirmed, lastConfirmed+99);

      lockTime = system_clock::now() - startTime;
      if (lockTime.count() > 0.2) {
        LOG_TOPIC(WARN, Logger::AGENCY)
          << "Finding unconfirmed entries took too long: " << lockTime.count();
      }

      // Note that despite compaction this vector can never be empty, since
      // any compaction keeps at least one active log entry!

      if (unconfirmed.empty()) {
        LOG_TOPIC(ERR, Logger::AGENCY) << "Unexpected empty unconfirmed: "
          << "lastConfirmed=" << lastConfirmed << " commitIndex="
          << commitIndex;
      }

      TRI_ASSERT(!unconfirmed.empty());

      index_t highest = unconfirmed.back().index;

      // _lastSent, _lastHighest: local and single threaded access
      duration<double> m = system_clock::now() - _lastSent[followerId];

      if (highest == _lastHighest[followerId] &&
          m.count() < 0.25 * _config.minPing()) {
        // I intentionally left here _config.minPing() without the
        // _config.timeoutMult(), if things are getting tight on the
        // system, we still send out empty heartbeats every 1/4 minpings,
        // even if we increase tolerance by a multiplier.
        continue;
      }

      if (m.count() > _config.minPing() &&
          _lastSent[followerId].time_since_epoch().count() != 0) {
        LOG_TOPIC(WARN, Logger::AGENCY) << "Oops, sent out last heartbeat "
          << "to follower " << followerId << " more than minPing ago: " 
          << m.count() << " lastAcked: " << timepointToString(lastAcked)
          << " lastSent: " << timepointToString(_lastSent[followerId]);
      }
      index_t lowest = unconfirmed.front().index;

      bool needSnapshot = false;
      Store snapshot(this, "snapshot");
      index_t snapshotIndex;
      term_t snapshotTerm;
      if (lowest > lastConfirmed) {
        // Ooops, compaction has thrown away so many log entries that
        // we cannot actually update the follower. We need to send our
        // latest snapshot instead:
        needSnapshot = true;
        bool success = false;
        try {
          success = _state.loadLastCompactedSnapshot(snapshot,
              snapshotIndex, snapshotTerm);
        } catch (std::exception const& e) {
          LOG_TOPIC(WARN, Logger::AGENCY)
            << "Exception thrown by loadLastCompactedSnapshot: "
            << e.what();
        }
        if (!success) {
          LOG_TOPIC(WARN, Logger::AGENCY)
            << "Could not load last compacted snapshot, not sending appendEntriesRPC!";
          continue;
        }
        if (snapshotTerm == 0) {
          // No shapshot yet
          needSnapshot = false;
        }
      }

      // RPC path
      std::stringstream path;
      index_t prevLogIndex = unconfirmed.front().index;
      index_t prevLogTerm = unconfirmed.front().term;
      if (needSnapshot) {
        prevLogIndex = snapshotIndex;
        prevLogTerm = snapshotTerm;
      }
      path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId="
           << id() << "&prevLogIndex=" << prevLogIndex
           << "&prevLogTerm=" << prevLogTerm << "&leaderCommit=" << commitIndex
           << "&senderTimeStamp=" << std::llround(readSystemClock() * 1000);
      
      size_t toLog = 0;
      // Body
      Builder builder;
      builder.add(VPackValue(VPackValueType::Array));
      if (
          ((system_clock::now() - earliestPackage).count() > 0)) {
        if (needSnapshot) {
          { VPackObjectBuilder guard(&builder);
            builder.add(VPackValue("readDB"));
            { VPackArrayBuilder guard2(&builder);
              snapshot.dumpToBuilder(builder);
            }
            builder.add("term", VPackValue(snapshotTerm));
            builder.add("index", VPackValue(snapshotIndex));
          }
        }
        for (size_t i = 0; i < unconfirmed.size(); ++i) {
          auto const& entry = unconfirmed.at(i);
          if (entry.index > lastConfirmed) {
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
      }
      builder.close();
      
      // Really leading?
      {
        MUTEX_LOCKER(ioLocker, _ioLock);

        if (challengeLeadership()) {
          ioLocker.unlock();
          _constituent.candidate();
          _preparing = false;
          return;
        }
      }
      
      // Verbose output
      if (toLog > 0) {
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Appending " << toLog << " entries up to index "
          << highest
          << (needSnapshot ? " and a snapshot" : "")
          << " to follower " << followerId << ". Message: "
          << builder.toJson();
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
        std::max(1.0e-3 * toLog * dt.count(), 
                 _config.minPing() * _config.timeoutMult()), true);

      _lastSent[followerId]        = system_clock::now();
      _lastHighest[followerId]     = highest;

      if (toLog > 0) {
        earliestPackage = system_clock::now() + toLog * dt;
        {
          MUTEX_LOCKER(ioLocker, _ioLock);
          _earliestPackage[followerId] = earliestPackage;
        }
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Appending " << unconfirmed.size() - 1 << " entries up to index "
          << highest << " to follower " << followerId 
          << ". Next real log contact to " << followerId<< " in: " 
          <<  std::chrono::duration<double, std::milli>(
            earliestPackage-system_clock::now()).count() << "ms";
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

      index_t commitIndex = 0;
      Slice compact = slice.get("compact");
      Slice    logs = slice.get("logs");

      
      VPackBuilder batch;
      batch.openArray();
      for (auto const& q : VPackArrayIterator(logs)) {
        batch.add(q.get("request"));
      }
      batch.close();

      {
        _liLock.assertNotLockedByCurrentThread();
        MUTEX_LOCKER(ioLocker, _ioLock); // Atomicity 
        if (!compact.isEmptyArray()) {
          _readDB = compact.get("readDB");
        }
        commitIndex = _commitIndex;
        // no need to lock via _readDB._compactionLock here
        _readDB.applyLogEntries(batch, commitIndex, _constituent.term(),
                                false  /* do not perform callbacks */);
        _spearhead = _readDB;
      }

      ret->add("success", VPackValue(true));
      ret->add("commitId", VPackValue(commitIndex));
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
void Agent::load() {

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

  // Note that the agent thread is terminated immediately when there is only
  // one agent, since no AppendEntriesRPC have to be issued. Therefore,
  // this thread is almost certainly terminated (and thus isStopping() returns
  // true), when we get here.
  if (size() > 1 && this->isStopping()) {
    return;
  }

  {
    CONDITION_LOCKER(guard, _appendCV);
    guard.broadcast();
  }

  _compactor.start();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting spearhead worker.";

  // Single threaded startup no need locking
  _spearhead.start();
  _readDB.start();

  _constituent.start(vocbase, queryRegistry);
  persistConfiguration(term());

  if (_config.supervision()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting cluster sanity facilities";
    _supervision.start(this);
  }

  if (_inception != nullptr) { // resilient agency only
    _inception->start();
  } else {
    _spearhead = _readDB;
    activateAgency();
  }
}

/// Still leading? Under MUTEX from ::read or ::write
bool Agent::challengeLeadership() {
  _ioLock.assertLockedByCurrentThread();

  size_t good = 0;
  
  for (auto const& i : _lastAcked) {
    duration<double> m = system_clock::now() - i.second;
    if (0.9 * _config.minPing() * _config.timeoutMult() > m.count()) {
      ++good;
    }
  }
  
  return (good < size() / 2);  // not counting myself
}


/// Get last acknowledged responses on leader
query_t Agent::lastAckedAgo() const {
  
  std::unordered_map<std::string, TimePoint> lastAcked;
  {
    _liLock.assertNotLockedByCurrentThread();
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

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (_preparing) {
      _waitForCV.wait(100);
    }
  }

  // Apply to spearhead and get indices for log entries
  auto qs = queries->slice();
  addTrxsOngoing(qs);    // remember that these are ongoing
  auto ret = std::make_shared<arangodb::velocypack::Builder>();
  size_t failed = 0;
  ret->openArray();
  {
    
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      _preparing = false;
      return trans_ret_t(false, NO_LEADER);
    }
    
    for (const auto& query : VPackArrayIterator(qs)) {
      if (query[0].isObject()) {
        check_ret_t res = _spearhead.applyTransaction(query); 
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

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (_preparing) {
      _waitForCV.wait(100);
    }
  }
  
  // Apply to spearhead and get indices for log entries
  {
    VPackArrayBuilder b(ret.get());
    
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);
    
    // Only leader else redirect
    if (challengeLeadership()) {
      _constituent.candidate();
      _preparing = false;
      return trans_ret_t(false, NO_LEADER);
    }

    // Read and writes
    for (const auto& query : VPackArrayIterator(queries->slice())) {
      if (query[0].isObject()) {
        ret->add(VPackValue(_transient.applyTransaction(query).successful()));
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
  
  _liLock.assertNotLockedByCurrentThread();
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
write_ret_t Agent::write(query_t const& query, bool discardStartup) {

  std::vector<bool> applied;
  std::vector<index_t> indices;
  auto multihost = size()>1;

  auto leader = _constituent.leaderID();
  if (multihost && leader != id()) {
    return write_ret_t(false, leader);
  }

  if (!discardStartup) {
    CONDITION_LOCKER(guard, _waitForCV);
    while (_preparing) {
      _waitForCV.wait(100);
    }
  }
  
  addTrxsOngoing(query->slice());    // remember that these are ongoing

  auto slice = query->slice();
  size_t ntrans = slice.length();
  size_t npacks = ntrans/_config.maxAppendSize();
  if (ntrans%_config.maxAppendSize()!=0) {
    npacks++;
  }

  // Apply to spearhead and get indices for log entries
  // Avoid keeping lock indefinitely
  for (size_t i = 0, l = 0; i < npacks; ++i) {
    query_t chunk = std::make_shared<Builder>();
    {
      VPackArrayBuilder b(chunk.get());
      for (size_t j = 0; j < _config.maxAppendSize() && l < ntrans; ++j, ++l) {
        chunk->add(slice.at(l));
      }
    }

    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);

    // Only leader else redirect
    if (multihost && challengeLeadership()) {
      _constituent.candidate();
      _preparing = false;
      return write_ret_t(false, NO_LEADER);
    }
    
    applied = _spearhead.applyTransactions(chunk);
    auto tmp = _state.log(chunk, applied, term());
    indices.insert(indices.end(), tmp.begin(), tmp.end());

  }

  removeTrxsOngoing(query->slice());

  // Maximum log index
  index_t maxind = 0;
  if (!indices.empty()) {
    maxind = *std::max_element(indices.begin(), indices.end());
  // update my last applied
    MUTEX_LOCKER(liLocker, _liLock);
    _lastApplied = maxind;
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

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (_preparing) {
      _waitForCV.wait(100);
    }
  }

  _liLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  // Only leader else redirect
  if (challengeLeadership()) {
    _constituent.candidate();
    _preparing = false;
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
      _appendCV.wait(static_cast<uint64_t>(4.e3*_config.minPing()));
      // Again, we leave minPing here without the multiplier to run this
      // loop often enough in cases of high load.
      
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
      _liLock.assertNotLockedByCurrentThread();
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
    _liLock.assertNotLockedByCurrentThread();
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
          agency->add("timeoutMult", VPackValue(_config.timeoutMult()));
        }}}}
  
  // In case we've lost leadership, no harm will arise as the failed write
  // prevents bogus agency configuration to be replicated among agents. ***
  write(agency, true); 

}


void Agent::failedActivation(
  std::string const& failed, std::string const& replacement) {
  MUTEX_LOCKER(actLock, _activatorLock);
  _activator.reset(nullptr);
}


void Agent::detectActiveAgentFailures() {
  // Detect faulty agent if pool larger than agency

  std::unordered_map<std::string, TimePoint> lastAcked;
  {
    _liLock.assertNotLockedByCurrentThread();
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
  if (_inception != nullptr) { // resilient agency only
    _inception->beginShutdown();
  } 

  // Compactor
  _compactor.beginShutdown();

  // Stop key value stores
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    _spearhead.beginShutdown();
    _readDB.beginShutdown();
  }

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
    _liLock.assertNotLockedByCurrentThread();
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
    _liLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);
    myterm = _constituent.term();
  }

  persistConfiguration(myterm);

  // Notify inactive pool
  notifyInactive();

  index_t commitIndex;
  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    commitIndex = _commitIndex;
  }
  
  {
    CONDITION_LOCKER(guard, _waitForCV);
    while(commitIndex != _state.lastIndex()) {
      _waitForCV.wait(10000);
      MUTEX_LOCKER(ioLocker, _ioLock);
      commitIndex = _commitIndex;
    }
  }

  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    _spearhead = _readDB;
  }
  
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

  std::unordered_map<std::string, std::string> pool = _config.pool();
  std::string path = "/_api/agency_priv/inform";

  Builder out;
  {
    VPackObjectBuilder o(&out);
    out.add("term", VPackValue(term()));
    out.add("id", VPackValue(id()));
    out.add("active", _config.activeToBuilder()->slice());
    out.add("pool", _config.poolToBuilder()->slice());
    out.add("min ping", VPackValue(_config.minPing()));
    out.add("max ping", VPackValue(_config.maxPing()));
    out.add("timeoutMult", VPackValue(_config.timeoutMult()));
  }

  for (auto const& p : pool) {
    if (p.first != id()) {
      auto headerFields =
          std::make_unique<std::unordered_map<std::string, std::string>>();
      cc->asyncRequest("1", 1, p.second, arangodb::rest::RequestType::POST,
                       path, std::make_shared<std::string>(out.toJson()),
                       headerFields, nullptr, 1.0, true);
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
    MUTEX_LOCKER(ioLocker, _ioLock);

    if (!challengeLeadership()) {
      ioLocker.unlock();

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
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING);
  }
  if (!slice.hasKey("max ping") || !slice.get("max ping").isNumber()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING);
  }
  if (!slice.hasKey("timeoutMult") || !slice.get("timeoutMult").isInteger()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT);
  }

  _config.update(message);

  _state.persistActiveAgents(_config.activeToBuilder(), _config.poolToBuilder());
  
}

// Rebuild key value stores
arangodb::consensus::index_t Agent::rebuildDBs() {

  _liLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);

  index_t lastCompactionIndex;
  term_t term;

  // We must go back to clean sheet
  _readDB.clear();
  _spearhead.clear();
  
  if (!_state.loadLastCompactedSnapshot(_readDB, lastCompactionIndex, term)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS);
  }

  // Apply logs from last applied index to leader's commit index
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Rebuilding key-value stores from index "
    << lastCompactionIndex << " to " << _commitIndex << " " << _state;

  {
    MUTEX_LOCKER(mutexLocker, _compactionLock);
    auto logs = _state.slices(lastCompactionIndex+1, _commitIndex);
    _readDB.applyLogEntries(logs, _commitIndex, _constituent.term(),
        false  /* do not send callbacks */);
  }
  _spearhead = _readDB;

  MUTEX_LOCKER(liLocker, _liLock);
  _lastApplied = _commitIndex;
  
  LOG_TOPIC(INFO, Logger::AGENCY)
    << id() << " rebuilt key-value stores - serving.";

  return _lastApplied;

}


/// Compact read db
void Agent::compact() {
  // We do not optimize for the case of _config.compactionKeepSize() == 0,
  // since one usually would like to keep a part of the recent log. Therefore
  // we cannot use the _readDB ever, since we have to compute a state of the
  // key/value space well before _lastAppliedIndex anyway:
  index_t commitIndex = 0;

  {
    MUTEX_LOCKER(ioLocker, _ioLock);
    _nextCompactionAfter += _config.compactionStepSize();
    commitIndex = _commitIndex;
  }

  if (commitIndex > _config.compactionKeepSize()) {
    // If the keep size is too large, we do not yet compact
    // TODO: check if there is at problem that we call State::compact()
    // now with a commit index that may have been slightly modified by other
    // threads
    // TODO: the question is if we have to lock out others while we 
    // call compact or while we grab _commitIndex and then call compact
    if (!_state.compact(commitIndex - _config.compactionKeepSize())) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Compaction for index "
        << commitIndex - _config.compactionKeepSize()
        << " did not work.";
    }
  }
}


/// Last commit index
std::pair<arangodb::consensus::index_t, arangodb::consensus::index_t>
Agent::lastCommitted() const {
  _liLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  MUTEX_LOCKER(liLocker, _liLock);
  return std::pair<arangodb::consensus::index_t, arangodb::consensus::index_t>(
    _lastApplied, _commitIndex);
}

/// Last commit index
void Agent::lastCommitted(arangodb::consensus::index_t lastCommitIndex) {
  _liLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  _commitIndex = lastCommitIndex;
  MUTEX_LOCKER(liLocker, _liLock);
  _lastApplied = lastCommitIndex;
}

/// Last log entry
log_t Agent::lastLog() const { return _state.lastLog(); }

/// Get spearhead
Store const& Agent::spearhead() const { return _spearhead; }

/// Get readdb
/// intentionally no lock is acquired here, so we can return
/// a const reference
/// the caller has to make sure the lock is actually held
Store const& Agent::readDB() const { 
  _ioLock.assertLockedByCurrentThread();
  return _readDB; 
}

/// Get readdb
arangodb::consensus::index_t Agent::readDB(Node& node) const {
  MUTEX_LOCKER(ioLocker, _ioLock);
  node = _readDB.get();
  return _commitIndex;
}

void Agent::executeLocked(std::function<void()> const& cb) {
  MUTEX_LOCKER(ioLocker, _ioLock);
  cb();
}

/// Get transient
/// intentionally no lock is acquired here, so we can return
/// a const reference
/// the caller has to make sure the lock is actually held
Store const& Agent::transient() const { 
  _ioLock.assertLockedByCurrentThread();
  return _transient;
}

/// Rebuild from persisted state
Agent& Agent::operator=(VPackSlice const& compaction) {
  // Catch up with compacted state
  MUTEX_LOCKER(ioLocker, _ioLock);
  _spearhead = compaction.get("readDB");
  _readDB = compaction.get("readDB");

  // Catch up with commit
  try {
    _commitIndex = arangodb::basics::StringUtils::uint64(
      compaction.get("_key").copyString());
    MUTEX_LOCKER(liLocker, _liLock);
    _lastApplied = _commitIndex;
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }

  // Schedule next compaction
  _nextCompactionAfter = _commitIndex + _config.compactionStepSize();

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
  if ( _inception != nullptr && isCallback) {
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

  std::unordered_map<std::string, std::string> incoming;
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
  
  if (!isCallback) {
    LOG_TOPIC(TRACE, Logger::AGENCY) << "Answering with gossip "
                                     << out->slice().toJson();
  }

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
  Store store(this);
  index_t oldIndex;
  term_t term;
  if (!_state.loadLastCompactedSnapshot(store, oldIndex, term)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS);
  }
 
  { 
    MUTEX_LOCKER(ioLocker, _ioLock);
    if (index > _commitIndex) {
      LOG_TOPIC(INFO, Logger::AGENCY)
        << "Cannot snapshot beyond leaderCommitIndex: " << _commitIndex;
      index = _commitIndex;
    } else if (index < oldIndex) {
      LOG_TOPIC(INFO, Logger::AGENCY)
        << "Cannot snapshot before last compaction index: " << oldIndex;
      index = oldIndex;
    }
  }
  
  {
    MUTEX_LOCKER(mutexLocker, _compactionLock);
    if (index > oldIndex) {
      auto logs = _state.slices(oldIndex+1, index);
      store.applyLogEntries(logs, index, term,
                            false  /* do not perform callbacks */);
    } else {
      VPackBuilder logs;
      logs.openArray();
      logs.close();
      store.applyLogEntries(logs, index, term,
                            false  /* do not perform callbacks */);
    }
  }

  auto builder = std::make_shared<VPackBuilder>();
  store.toBuilder(*builder);
  
  return builder;
  
}

void Agent::addTrxsOngoing(Slice trxs) {
  try {
    MUTEX_LOCKER(guard,_trxsLock);
    for (auto const& trx : VPackArrayIterator(trxs)) {
      if (trx.isArray() && trx.length() == 3 && trx[0].isObject() && trx[2].isString()) {
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
      if (trx.isArray() && trx.length() == 3 && trx[0].isObject() && trx[2].isString()) {
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
