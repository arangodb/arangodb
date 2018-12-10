////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::velocypack;
using namespace std::chrono;

namespace arangodb {
namespace consensus {

// Instanciations of some declarations in AgencyCommon.h:

std::string const pubApiPrefix("/_api/agency/");
std::string const privApiPrefix("/_api/agency_priv/");
std::string const NO_LEADER("");

/// Agent configuration
Agent::Agent(config_t const& config)
  : Thread("Agent"),
    _config(config),
    _commitIndex(0),
    _spearhead(this),
    _readDB(this),
    _transient(this),
    _agentNeedsWakeup(false),
    _compactor(this),
    _ready(false),
    _preparing(0) {
  _state.configure(this);
  _constituent.configure(this);
  if (size() > 1) {
    _inception = std::make_unique<Inception>(this);
  } else {
    _leaderSince = 0;
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
  waitForThreadsStop();
  // This usually was already done called from AgencyFeature::unprepare,
  // but since this only waits for the threads to stop, it can be done
  // multiple times, and we do it just in case the Agent object was
  // created but never really started. Here, we exit with a fatal error
  // if the threads do not stop in time.
}

/// Wait until threads are terminated:
void Agent::waitForThreadsStop() {
  // It is allowed to call this multiple times, we do so from the constructor
  // and from AgencyFeature::unprepare.
  int counter = 0;
  while (_constituent.isRunning() || _compactor.isRunning() ||
         (_config.supervision() && _supervision.isRunning()) ||
         (_inception != nullptr && _inception->isRunning())) {
    std::this_thread::sleep_for(std::chrono::microseconds(100000));

    // fail fatally after 5 mins:
    if (++counter >= 10 * 60 * 5) {
      LOG_TOPIC(FATAL, Logger::AGENCY) << "some agency thread did not finish";
      FATAL_ERROR_EXIT();
    }
  }
  shutdown();  // wait for the main Agent thread to terminate
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

  if (timeoutMult != -1 && timeoutMult != _config.timeoutMult()) {
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
  // When we become leader, we are first entering a preparation phase.
  // Note that this method returns true regardless of whether we
  // are still preparing or not. The preparation phases 1 and 2 are
  // indicated by the _preparing member in the Agent, the Constituent is
  // already LEADER.
  // The Constituent has to send out AppendEntriesRPC calls immediately, but
  // only when we are properly leading (with initialized stores etc.)
  // can we execute requests.
  return _constituent.leading();
}

// Waits here for confirmation of log's commits up to index. Timeout in seconds.
AgentInterface::raft_commit_t Agent::waitFor(index_t index, double timeout) {

  if (size() == 1) {  // single host agency
    return Agent::raft_commit_t::OK;
  }

  auto startTime = steady_clock::now();
  index_t lastCommitIndex = 0;

  // Wait until woken up through AgentCallback
  while (true) {

    /// success?
    ///  (_waitForCV's mutex stops writes to _commitIndex)
    CONDITION_LOCKER(guard, _waitForCV);
    if (leading()) {
      if (lastCommitIndex != _commitIndex) {
        // We restart the timeout computation if there has been progress:
        startTime = steady_clock::now();
      }
      lastCommitIndex = _commitIndex;
      if (lastCommitIndex >= index) {
        return Agent::raft_commit_t::OK;
      }
    } else {
      return Agent::raft_commit_t::UNKNOWN;
    }

    duration<double> d = steady_clock::now() - startTime;

    LOG_TOPIC(DEBUG, Logger::AGENCY) << "waitFor: index: " << index <<
      " _commitIndex: " << _commitIndex << " _lastCommitIndex: " <<
      lastCommitIndex << " elapsedTime: " << d.count();

    if (d.count() >= timeout) {
      return Agent::raft_commit_t::TIMEOUT;
    }

    // Go to sleep:
    _waitForCV.wait(static_cast<uint64_t>(1.0e6 * (timeout - d.count())));

    // shutting down
    if (this->isStopping()) {
      return Agent::raft_commit_t::UNKNOWN;
    }
  }

  // We should never get here
  TRI_ASSERT(false);

  return Agent::raft_commit_t::UNKNOWN;
}

// Check if log is committed up to index.
bool Agent::isCommitted(index_t index) {

  if (size() == 1) {  // single host agency
    return true;
  }

  CONDITION_LOCKER(guard, _waitForCV);
  if (leading()) {
    return _commitIndex >= index;
  } else {
    return false;
  }
}

//  AgentCallback reports id of follower and its highest processed index
void Agent::reportIn(std::string const& peerId, index_t index, size_t toLog) {

  auto startTime = steady_clock::now();

  // only update the time stamps here:
  {
    MUTEX_LOCKER(tiLocker, _tiLock);

    // Update last acknowledged answer
    auto t = steady_clock::now();
    std::chrono::duration<double> d = t - _lastAcked[peerId];
    auto secsSince = d.count();
    if (secsSince < 1.5e9 && peerId != id()
        && secsSince > _config.minPing() * _config.timeoutMult()) {
      LOG_TOPIC(WARN, Logger::AGENCY)
        << "Last confirmation from peer " << peerId
        << " was received more than minPing ago: " << secsSince;
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Setting _lastAcked[" << peerId << "] to time "
      << std::chrono::duration_cast<std::chrono::microseconds>(
        t.time_since_epoch()).count();
    _lastAcked[peerId] = t;

    if (index > _confirmed[peerId]) {  // progress this follower?
      _confirmed[peerId] = index;
      if (toLog > 0) { // We want to reset the wait time only if a package callback
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Got call back of " << toLog << " logs, resetting _earliestPackage to now for id " << peerId;
        _earliestPackage[peerId] = steady_clock::now();
      }
      wakeupMainLoop();   // only necessary for non-empty callbacks
    }
  }

  duration<double> reportInTime = steady_clock::now() - startTime;
  if (reportInTime.count() > 0.1) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "reportIn took longer than 0.1s: " << reportInTime.count();
  }
}

/// @brief Report a failed append entry call from AgentCallback
void Agent::reportFailed(std::string const& slaveId, size_t toLog, bool sent) {
  if (toLog > 0) {
    // This is only used for non-empty appendEntriesRPC calls. If such calls
    // fail, we have to set this earliestPackage time to now such that the
    // main thread tries again immediately: and for that agent starting at 0
    // which effectively will be _state.firstIndex().
    MUTEX_LOCKER(guard, _tiLock);
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Resetting _earliestPackage to now for id " << slaveId;
    _earliestPackage[slaveId] = steady_clock::now() + seconds(1);
    _confirmed[slaveId] = 0;
  } else {
    // answer to sendAppendEntries to empty request, when follower's highest
    // log index is 0. This is necessary so that a possibly restarted agent
    // without persistence immediately is brought up to date. We only want to do
    // this, when the agent was able to answer and no or corrupt answer is
    // handled
    if (sent) {
      MUTEX_LOCKER(guard, _tiLock);
      _confirmed[slaveId] = 0;
    }
  }
}

/// Followers' append entries
priv_rpc_ret_t Agent::recvAppendEntriesRPC(
  term_t term, std::string const& leaderId, index_t prevIndex, term_t prevTerm,
  index_t leaderCommitIndex, query_t const& queries) {

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Got AppendEntriesRPC from "
    << leaderId << " with term " << term;

  term_t t(this->term());
  if (!ready()) { // We have not been able to put together our configuration
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agent is not ready yet.";
    return priv_rpc_ret_t(false,t);
  }

  VPackSlice payload = queries->slice();

  // Update commit index
  if (payload.type() != VPackValueType::Array) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Received malformed entries for appending. Discarding!";
    return priv_rpc_ret_t(false,t);
  }

  size_t nqs = payload.length();
  if(nqs > 0 && !payload[0].get("readDB").isNone()) {
    // We have received a compacted state.
    // Whatever we got in our own state is meaningless now. It is a new world.
    // checkLeader just does plausibility as if it were an empty request
    prevIndex = 0;
    prevTerm = 0;
  }

  // Leadership claim plausibility check
  if (!_constituent.checkLeader(term, leaderId, prevIndex, prevTerm)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Not accepting appendEntries from " << leaderId;
    return priv_rpc_ret_t(false,t);
  }

  // Empty appendEntries:
  // We answer with success if and only if our highest index is greater 0.
  // Else we want to indicate to the leader that we are behind and need data:
  // a single false will go back and trigger _confirmed[thisfollower] = 0
  if (nqs == 0) {
    auto lastIndex = _state.lastIndex();
    if (lastIndex > 0) {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Finished empty AppendEntriesRPC from " << leaderId << " with term " << term;
      {
        WRITE_LOCKER(oLocker, _outputLock);
        _commitIndex = std::max(
          _commitIndex, std::min(leaderCommitIndex, lastIndex));
        if (_commitIndex >= _state.nextCompactionAfter()) {
          _compactor.wakeUp();
        }
      }
      return priv_rpc_ret_t(true,t);
    } else {
      return priv_rpc_ret_t(false,t);
    }
  }

  bool ok = true;
  index_t lastIndex = 0;   // Index of last entry in our log
  try {
    lastIndex = _state.logFollower(queries);
    if (lastIndex < payload[nqs-1].get("index").getNumber<index_t>()) {
      // We could not log all the entries in this query, we need to report
      // this to the leader!
      ok = false;
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Exception during log append: " << __FILE__ << __LINE__
      << " " << e.what();
  }

  {
    WRITE_LOCKER(oLocker, _outputLock);
    CONDITION_LOCKER(guard, _waitForCV);
    _commitIndex = std::max(
      _commitIndex, std::min(leaderCommitIndex, lastIndex));
    _waitForCV.broadcast();
    if (_commitIndex >= _state.nextCompactionAfter()) {
      _compactor.wakeUp();
    }
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Finished AppendEntriesRPC from "
    << leaderId << " with term " << term;

  return priv_rpc_ret_t(ok,t);
}

/// Leader's append entries
void Agent::sendAppendEntriesRPC() {
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return;
  }

  // _lastSent only accessed in main thread
  std::string const myid = id();

  for (auto const& followerId : _config.active()) {

    if (followerId != myid && leading()) {

      term_t t(0);

      index_t lastConfirmed;
      auto startTime = steady_clock::now();
      SteadyTimePoint earliestPackage;
      SteadyTimePoint lastAcked;

      {
        t = this->term();
        MUTEX_LOCKER(tiLocker, _tiLock);
        lastConfirmed = _confirmed[followerId];
        lastAcked = _lastAcked[followerId];
        earliestPackage = _earliestPackage[followerId];
      }

      // We essentially have to send some log entries from their lastConfirmed+1
      // on. However, we have to take care of the case that their lastConfirmed
      // is a value which is very outdated, such that we have in the meantime
      // done a log compaction and do not actually have lastConfirmed+1 any
      // more. In that case, we need to send our latest snapshot at index S
      // (say), and then some log entries from (and including) S on. This is
      // to ensure that the other side does not only have the snapshot, but
      // also the log entry which produced that snapshot.
      // Therefore, we will set lastConfirmed to one less than our latest
      // snapshot in this special case, and we will always fetch enough
      // entries from the log to fulfull our duties.

      if ((steady_clock::now() - earliestPackage).count() < 0 ||
          _state.lastIndex() <= lastConfirmed) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Nothing to append.";
        continue;
      }

      duration<double> lockTime = steady_clock::now() - startTime;
      if (lockTime.count() > 0.1) {
        LOG_TOPIC(WARN, Logger::AGENCY)
          << "Reading lastConfirmed took too long: " << lockTime.count();
      }

      index_t commitIndex;
      {
        READ_LOCKER(oLocker, _outputLock);
        commitIndex = _commitIndex;
      }

      // If the follower is behind our first log entry send last snapshot and
      // following logs. Else try to have the follower catch up in regular order.
      bool needSnapshot = lastConfirmed < _state.firstIndex();
      if (needSnapshot) {
        lastConfirmed = _state.lastCompactionAt() - 1;
      }

      LOG_TOPIC(TRACE, Logger::AGENCY)
        << "Getting unconfirmed from " << lastConfirmed << " to " <<  lastConfirmed+99;
      // If lastConfirmed is one minus the first log entry, then this is
      // corrected in _state::get and we only get from the beginning of the
      // log.
      std::vector<log_t> unconfirmed = _state.get(lastConfirmed, lastConfirmed+99);

      lockTime = steady_clock::now() - startTime;
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
        TRI_ASSERT(false);
      }

      // Note that if we get here we have at least two entries, otherwise
      // we would have done continue earlier, since this can only happen
      // if lastConfirmed is equal to the last index in our log, in which
      // case there is nothing to replicate.

      duration<double> m = steady_clock::now() - _lastSent[followerId];

      if (m.count() > _config.minPing() &&
          _lastSent[followerId].time_since_epoch().count() != 0) {
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "Note: sent out last AppendEntriesRPC "
          << "to follower " << followerId << " more than minPing ago: "
          << m.count() << " lastAcked: "
          << duration_cast<duration<double>>(lastAcked.time_since_epoch()).count();
      }
      index_t lowest = unconfirmed.front().index;

      Store snapshot(this, "snapshot");
      index_t snapshotIndex;
      term_t snapshotTerm;

      if (lowest > lastConfirmed || needSnapshot) {
        // Ooops, compaction has thrown away so many log entries that
        // we cannot actually update the follower. We need to send our
        // latest snapshot instead:
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
      {
        path << "/_api/agency_priv/appendEntries?term=" << t << "&leaderId="
             << id() << "&prevLogIndex=" << prevLogIndex
             << "&prevLogTerm=" << prevLogTerm << "&leaderCommit=" << commitIndex
             << "&senderTimeStamp=" << std::llround(steadyClockToDouble() * 1000);
      }

      // Body
      Builder builder;
      builder.add(VPackValue(VPackValueType::Array));

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

      size_t toLog = 0;
      index_t highest = 0;
      for (size_t i = 0; i < unconfirmed.size(); ++i) {
        auto const& entry = unconfirmed.at(i);
        if (entry.index > lastConfirmed) {
          // This condition is crucial, because usually we have one more
          // entry than we need in unconfirmed, so we want to skip this. If,
          // however, we have sent a snapshot, we need to send the log entry
          // with the same index than the snapshot along to retain the
          // invariant of our data structure that the _log in _state is
          // non-empty.
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

      // Really leading?
      if (challengeLeadership()) {
        resign();
        return;
      }

      // Postpone sending the next message for 30 seconds or until an
      // error or successful result occurs.
      earliestPackage = steady_clock::now() + std::chrono::seconds(30);
      {
        MUTEX_LOCKER(tiLocker, _tiLock);
        _earliestPackage[followerId] = earliestPackage;
      }
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Setting _earliestPackage to now + 30s for id " << followerId;

      // Send request
      std::unordered_map<std::string, std::string> headerFields;
      cc->asyncRequest(
        1, _config.poolAt(followerId),
        arangodb::rest::RequestType::POST, path.str(),
        std::make_shared<std::string>(builder.toJson()), headerFields,
        std::make_shared<AgentCallback>(this, followerId, highest, toLog),
        150.0, true);
      // Note the timeout is relatively long, but due to the 30 seconds
      // above, we only ever have at most 5 messages in flight.

      _lastSent[followerId]    = steady_clock::now();
      // _constituent.notifyHeartbeatSent(followerId);
      // Do not notify constituent, because the AppendEntriesRPC here could
      // take a very long time, so this must not disturb the empty ones
      // being sent out.

      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Appending (" << (uint64_t) (TRI_microtime() * 1000000000.0) << ") "
        << unconfirmed.size() - 1 << " entries up to index "
        << highest << (needSnapshot ? " and a snapshot" : "")
        << " to follower " << followerId
        << ". Next real log contact to " << followerId<< " in: "
        <<  std::chrono::duration<double, std::milli>(
          earliestPackage - steady_clock::now()).count() << "ms";
    }
  }
}


void Agent::resign(term_t otherTerm) {
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Resigning in term "
    << _constituent.term() << " because of peer's term " << otherTerm;
  _constituent.follow(otherTerm, NO_LEADER);
  endPrepareLeadership();
}


/// Leader's append entries, empty ones for heartbeat, triggered by Constituent
void Agent::sendEmptyAppendEntriesRPC(std::string followerId) {

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens during controlled shutdown
    return;
  }

  if (!leading()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Not sending empty appendEntriesRPC to follower " << followerId
      << " because we are no longer leading.";
    return;
  }

  index_t commitIndex;
  {
    READ_LOCKER(oLocker, _outputLock);
    commitIndex = _commitIndex;
  }

  // RPC path
  std::stringstream path;
  {
    path << "/_api/agency_priv/appendEntries?term=" << _constituent.term()
         << "&leaderId=" << id() << "&prevLogIndex=0"
         << "&prevLogTerm=0&leaderCommit=" << commitIndex
         << "&senderTimeStamp=" << std::llround(steadyClockToDouble() * 1000);
  }

  // Just check once more:
  if (!leading()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Not sending empty appendEntriesRPC to follower " << followerId
      << " because we are no longer leading.";
    return;
  }

  // Send request
  std::unordered_map<std::string, std::string> headerFields;
  cc->asyncRequest(
    1, _config.poolAt(followerId),
    arangodb::rest::RequestType::POST, path.str(),
    std::make_shared<std::string>("[]"), headerFields,
    std::make_shared<AgentCallback>(this, followerId, 0, 0),
    3 * _config.minPing() * _config.timeoutMult(), true);
  _constituent.notifyHeartbeatSent(followerId);

  double now = TRI_microtime();
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Sending empty appendEntriesRPC to follower " << followerId;
  double diff = TRI_microtime() - now;
  if (diff > 0.01) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Logging of a line took more than 1/100 of a second, this is bad:" << diff;
  }
}

void Agent::advanceCommitIndex() {
  // Determine median _confirmed value of followers:
  std::vector<index_t> temp;
  {
    MUTEX_LOCKER(_tiLocker, _tiLock);
    for (auto const& id: config().active()) {
      if (_confirmed.find(id) != _confirmed.end()) {
        temp.push_back(_confirmed[id]);
      }
    }
  }

  index_t quorum = size() / 2 + 1;
  if (temp.size() < quorum) {
    LOG_TOPIC(WARN, Logger::AGENCY) << "_confirmed not populated, quorum: " << quorum << ".";
    return;
  }
  std::sort(temp.begin(), temp.end());
  index_t index = temp[temp.size() - quorum];

  term_t t = _constituent.term();
  {
    WRITE_LOCKER(oLocker, _outputLock);
    if (index > _commitIndex) {
      CONDITION_LOCKER(guard, _waitForCV);
      LOG_TOPIC(TRACE, Logger::AGENCY)
        << "Critical mass for commiting " << _commitIndex + 1
        << " through " << index << " to read db";
      // Change _readDB and _commitIndex atomically together:
      _readDB.applyLogEntries(
        _state.slices( /* inform others by callbacks */
          _commitIndex + 1, index), _commitIndex, t, true);

      _commitIndex = index;
      // Wake up rest handlers:
      _waitForCV.broadcast();

      if (_commitIndex >= _state.nextCompactionAfter()) {
        _compactor.wakeUp();
      }
    }
  }
}

// Check if I am member of active agency
bool Agent::active() const {
  std::vector<std::string> active = _config.active();
  return (find(active.begin(), active.end(), id()) != active.end());
}


/// @brief Activate agency (Inception thread for multi-host, main thread else)
void Agent::activateAgency() {

  _config.activate();
  try {
    _state.persistActiveAgents(
      _config.activeToBuilder(), _config.poolToBuilder());
  } catch (std::exception const& e) {
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "Failed to persist active agency: " << e.what();
      FATAL_ERROR_EXIT();
  }

}

/// Load persistent state called once
void Agent::load() {
  auto* sysDbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  arangodb::SystemDatabaseFeature::ptr vocbase =
    sysDbFeature ? sysDbFeature->use() : nullptr;
  auto queryRegistry = QueryRegistryFeature::registry();

  if (vocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  {
    _tiLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(guard, _ioLock);  // need this for callback to set _spearhead
    // Note that _state.loadCollections eventually does a callback to the
    // setPersistedState method, which acquires _outputLock and _waitForCV.

    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Loading persistent state.";

    if (!_state.loadCollections(vocbase.get(), queryRegistry, _config.waitForSync())) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
          << "Failed to load persistent state on startup.";
      FATAL_ERROR_EXIT();
    }
  }

  // Note that the agent thread is terminated immediately when there is only
  // one agent, since no AppendEntriesRPC have to be issued. Therefore,
  // this thread is almost certainly terminated (and thus isStopping() returns
  // true), when we get here.
  if (size() > 1 && this->isStopping()) {
    return;
  }

  wakeupMainLoop();

  _compactor.start();

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting spearhead worker.";

  _constituent.start(vocbase.get(), queryRegistry);
  persistConfiguration(term());

  if (_config.supervision()) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting cluster sanity facilities";
    _supervision.start(this);
  }

  if (_inception != nullptr) { // resilient agency only
    _inception->start();
  } else {
    MUTEX_LOCKER(guard, _ioLock);  // need this for callback to set _spearhead
    READ_LOCKER(guard2, _outputLock);
    _spearhead = _readDB;
    activateAgency();
  }
}

/// Still leading? Under MUTEX from ::read or ::write
bool Agent::challengeLeadership() {
  MUTEX_LOCKER(tiLocker, _tiLock);
  size_t good = 0;

  std::string const myid = id();

  for (auto const& i : _lastAcked) {
    if (i.first != myid) {  // do not count ourselves
      duration<double> m = steady_clock::now() - i.second;
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "challengeLeadership: found "
        "_lastAcked[" << i.first << "] to be " << m.count() << " seconds in the past.";

      // This is rather arbitrary here: We used to have 0.9 here to absolutely
      // ensure that a leader resigns before another one even starts an election.
      // However, the Raft paper does not mention this at all. Rather, in the
      // paper it is written that the leader should resign immediately if it
      // sees a higher term from another server. Currently we have not
      // implemented to return the follower's term with a response to
      // AppendEntriesRPC, so the leader cannot find out a higher term this
      // way. The leader can, however, see a higher term in the incoming
      // AppendEntriesRPC a new leader sends out, and it will immediately
      // resign if it sees that. For the moment, this value here can stay.
      // We should soon implement sending the follower's term back with
      // each response and probably get rid of this method altogether,
      // but this requires a bit more thought.
      if (_config.maxPing() * _config.timeoutMult() > m.count()) {
        ++good;
      }
    }
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "challengeLeadership: good=" << good;

  return (good < size() / 2);  // not counting myself
}


/// Get last acknowledged responses on leader
void Agent::lastAckedAgo(Builder& ret) const {

  std::unordered_map<std::string, index_t> confirmed;
  std::unordered_map<std::string, SteadyTimePoint> lastAcked;
  std::unordered_map<std::string, SteadyTimePoint> lastSent;
  index_t lastCompactionAt, nextCompactionAfter;

  {
    MUTEX_LOCKER(tiLocker, _tiLock);
    lastAcked = _lastAcked;
    confirmed = _confirmed;
    lastSent = _lastSent;
    lastCompactionAt = _state.lastCompactionAt();
    nextCompactionAfter = _state.nextCompactionAfter();
  }

  std::function<double(std::pair<std::string,SteadyTimePoint> const&)> dur2str =
    [&](std::pair<std::string,SteadyTimePoint> const& i) {
    return id() == i.first ? 0.0 :
      1.0e-3 *
        std::floor(duration<double>(steady_clock::now()-i.second).count()*1.0e3);
  };

  ret.add("lastCompactionAt", VPackValue(lastCompactionAt));
  ret.add("nextCompactionAfter", VPackValue(nextCompactionAfter));
  if (leading()) {
    ret.add(VPackValue("lastAcked"));
    VPackObjectBuilder b(&ret);
    for (auto const& i : lastAcked) {
      auto lsit = lastSent.find(i.first);
      // Note that it is possible that a server is already in lastAcked
      // but not yet in lastSent, since lastSent only has times of non-empty
      // appendEntriesRPC calls, but we also get lastAcked entries for the
      // empty ones.
      ret.add(VPackValue(i.first));
      { VPackObjectBuilder o(&ret);
        ret.add("lastAckedTime", VPackValue(dur2str(i)));
        ret.add("lastAckedIndex", VPackValue(confirmed.at(i.first)));
        if (i.first != id()) {
          if (lsit != lastSent.end()) {
            ret.add("lastAppend", VPackValue(dur2str(*lsit)));
          } else {
            ret.add("lastAppend", VPackValue(dur2str(i)));
            // This is just for the above mentioned case, which will very
            // soon be rectified.
          }
        }
      }
    }
  }

}

trans_ret_t Agent::transact(query_t const& queries) {

  arangodb::consensus::index_t maxind = 0; // maximum write index

  // Note that we are leading (_constituent.leading()) if and only
  // if _constituent.leaderId == our own ID. Therefore, we do not have
  // to use leading() or _constituent.leading() here, but can simply
  // look at the leaderID.
  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return trans_ret_t(false, leader);
  }

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (getPrepareLeadership() != 0) {
      _waitForCV.wait(100);
    }
  }

  // Apply to spearhead and get indices for log entries
  auto qs = queries->slice();
  addTrxsOngoing(qs);    // remember that these are ongoing
  size_t failed;
  auto ret = std::make_shared<arangodb::velocypack::Builder>();
  {
    TRI_DEFER(removeTrxsOngoing(qs));
    // Note that once the transactions are in our log, we can remove them
    // from the list of ongoing ones, although they might not yet be committed.
    // This is because then, inquire will find them in the log and draw its
    // own conclusions. The map of ongoing trxs is only to cover the time
    // from when we receive the request until we have appended the trxs
    // ourselves.
    ret = std::make_shared<arangodb::velocypack::Builder>();
    failed = 0;
    ret->openArray();
    // Only leader else redirect
    if (challengeLeadership()) {
      resign();
      return trans_ret_t(false, NO_LEADER);
    }

    _tiLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);

    for (const auto& query : VPackArrayIterator(qs)) {
      if (query[0].isObject()) {
        check_ret_t res = _spearhead.applyTransaction(query);
        if(res.successful()) {
          maxind = (query.length() == 3 && query[2].isString()) ?
            _state.logLeaderSingle(query[0], term(), query[2].copyString()) :
            _state.logLeaderSingle(query[0], term());
          ret->add(VPackValue(maxind));
        } else {
          _spearhead.read(res.failed->slice(), *ret);
          ++failed;
        }
      } else if (query[0].isString()) {
        _spearhead.read(query, *ret);
      }
    }
    ret->close();
  }

  // Report that leader has persisted
  reportIn(id(), maxind);

  if (size() == 1) {
    advanceCommitIndex();
  }

  return trans_ret_t(true, id(), maxind, failed, ret);
}


// Non-persistent write to non-persisted key-value store
trans_ret_t Agent::transient(query_t const& queries) {

  // Note that we are leading (_constituent.leading()) if and only
  // if _constituent.leaderId == our own ID. Therefore, we do not have
  // to use leading() or _constituent.leading() here, but can simply
  // look at the leaderID.
  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return trans_ret_t(false, leader);
  }

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (getPrepareLeadership() != 0) {
      _waitForCV.wait(100);
    }
  }

  auto ret = std::make_shared<arangodb::velocypack::Builder>();

  // Apply to spearhead and get indices for log entries
  {
    VPackArrayBuilder b(ret.get());

    // Only leader else redirect
    if (challengeLeadership()) {
      resign();
      return trans_ret_t(false, NO_LEADER);
    }

    _tiLock.assertNotLockedByCurrentThread();
    MUTEX_LOCKER(ioLocker, _ioLock);

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


write_ret_t Agent::inquire(query_t const& query) {

  // Note that we are leading (_constituent.leading()) if and only
  // if _constituent.leaderId == our own ID. Therefore, we do not have
  // to use leading() or _constituent.leading() here, but can simply
  // look at the leaderID.
  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return write_ret_t(false, leader);
  }

  write_ret_t ret;

  while (true) {
    // Check ongoing ones:
    bool found = false;
    for (auto const& s : VPackArrayIterator(query->slice())) {
      std::string ss = s.copyString();
      if (isTrxOngoing(ss)) {
        found = true;
        break;
      }
    }
    if (!found) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(0.1));
    leader = _constituent.leaderID();
    if (leader != id()) {
      return write_ret_t(false, leader);
    }
  }

  _tiLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);

  ret.indices = _state.inquire(query);

  ret.accepted = true;

  return ret;
}


/// Write new entries to replicated state and store
write_ret_t Agent::write(query_t const& query, WriteMode const& wmode) {

  std::vector<apply_ret_t> applied;
  std::vector<index_t> indices;
  auto multihost = size()>1;

  // Note that we are leading (_constituent.leading()) if and only
  // if _constituent.leaderId == our own ID. Therefore, we do not have
  // to use leading() or _constituent.leading() here, but can simply
  // look at the leaderID.
  auto leader = _constituent.leaderID();
  if (multihost && leader != id()) {
    return write_ret_t(false, leader);
  }

  if (!wmode.discardStartup()) {
    CONDITION_LOCKER(guard, _waitForCV);
    while (getPrepareLeadership() != 0) {
      _waitForCV.wait(100);
    }
  }

  {
    addTrxsOngoing(query->slice());    // remember that these are ongoing
    TRI_DEFER(removeTrxsOngoing(query->slice()));
    // Note that once the transactions are in our log, we can remove them
    // from the list of ongoing ones, although they might not yet be committed.
    // This is because then, inquire will find them in the log and draw its
    // own conclusions. The map of ongoing trxs is only to cover the time
    // from when we receive the request until we have appended the trxs
    // ourselves.

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

      // Only leader else redirect
      if (multihost && challengeLeadership()) {
        resign();
        return write_ret_t(false, NO_LEADER);
      }

      _tiLock.assertNotLockedByCurrentThread();
      MUTEX_LOCKER(ioLocker, _ioLock);

      applied = _spearhead.applyTransactions(chunk, wmode);
      auto tmp = _state.logLeaderMulti(chunk, applied, term());
      indices.insert(indices.end(), tmp.begin(), tmp.end());

    }
  }

  // Maximum log index
  index_t maxind = 0;
  if (!indices.empty()) {
    maxind = *std::max_element(indices.begin(), indices.end());
  }

  // Report that leader has persisted
  reportIn(id(), maxind);

  if (size() == 1) {
    advanceCommitIndex();
  }

  return write_ret_t(true, id(), applied, indices);
}

/// Read from store
read_ret_t Agent::read(query_t const& query) {

  // Note that we are leading (_constituent.leading()) if and only
  // if _constituent.leaderId == our own ID. Therefore, we do not have
  // to use leading() or _constituent.leading() here, but can simply
  // look at the leaderID.
  auto leader = _constituent.leaderID();
  if (leader != id()) {
    return read_ret_t(false, leader);
  }

  {
    CONDITION_LOCKER(guard, _waitForCV);
    while (getPrepareLeadership() != 0) {
      _waitForCV.wait(100);
    }
  }

  // Only leader else redirect
  if (challengeLeadership()) {
    resign();
    return read_ret_t(false, NO_LEADER);
  }

  std::string leaderId = _constituent.leaderID();
  READ_LOCKER(oLocker, _outputLock);

  // Retrieve data from readDB
  auto result = std::make_shared<arangodb::velocypack::Builder>();
  std::vector<bool> success = _readDB.read(query, result);

  return read_ret_t(true, leaderId, success, result);

}


/// Send out append entries to followers regularly or on event
void Agent::run() {
  // Only run in case we are in multi-host mode
  while (!this->isStopping() && size() > 1) {

    {
      // We set the variable to false here, if any change happens during
      // or after the calls in this loop, this will be set to true to
      // indicate no sleeping. Any change will happen under the mutex.
      CONDITION_LOCKER(guard, _appendCV);
      _agentNeedsWakeup = false;
    }

    if (leading() && getPrepareLeadership() == 1) {
      // If we are officially leading but the _preparing flag is set, we
      // are in the process of preparing for leadership. This flag is
      // set when the Constituent celebrates an election victory. Here,
      // in the main thread, we do the actual preparations:

      if (!prepareLead()) {
        _constituent.follow(0);  // do not change _term or _votedFor
      } else {
        // we need to start work as leader
        lead();
      }

      donePrepareLeadership();   // we are ready to roll, except that we
                                 // have to wait for the _commitIndex to
                                 // reach the end of our log
    }

    // Leader working only
    if (leading()) {
      if (1 == getPrepareLeadership()) {
        // Skip the usual work and the waiting such that above preparation
        // code runs immediately. We will return with value 2 such that
        // replication and confirmation of it can happen. Service will
        // continue once _commitIndex has reached the end of the log and then
        // getPrepareLeadership() will finally return 0.
        continue;
      }

      // Challenge leadership.
      // Let's proactively know, that we no longer lead instead of finding out
      // through read/write.
      if (challengeLeadership()) {
        resign();
        continue;
      }

      // Append entries to followers
      sendAppendEntriesRPC();

      // Check whether we can advance _commitIndex
      advanceCommitIndex();

      bool commenceService = false;
      {
        READ_LOCKER(oLocker, _outputLock);
        if (leading() && getPrepareLeadership() == 2 &&
            _commitIndex == _state.lastIndex()) {
          commenceService = true;
        }
      }

      if (commenceService) {
        _tiLock.assertNotLockedByCurrentThread();
        MUTEX_LOCKER(ioLocker, _ioLock);
        READ_LOCKER(oLocker, _outputLock);
        _spearhead = _readDB;
        endPrepareLeadership();  // finally service can commence
      }

      // Go to sleep some:
      {
        CONDITION_LOCKER(guard, _appendCV);
        if (!_agentNeedsWakeup) {
          // wait up to minPing():
          _appendCV.wait(static_cast<uint64_t>(1.0e6 * _config.minPing()));
          // We leave minPing here without the multiplier to run this
          // loop often enough in cases of high load.
        }
      }
    } else {
      CONDITION_LOCKER(guard, _appendCV);
      if (!_agentNeedsWakeup) {
        _appendCV.wait(1000000);
      }
    }

  }

}

void Agent::persistConfiguration(term_t t) {

  // Agency configuration
  auto agency = std::make_shared<Builder>();
  { VPackArrayBuilder trxs(agency.get());
    { VPackArrayBuilder trx(agency.get());
      { VPackObjectBuilder oper(agency.get());
        agency->add(VPackValue(RECONFIGURE));
        { VPackObjectBuilder a(agency.get());
          agency->add("op", VPackValue("set"));
          agency->add(VPackValue("new"));
          { VPackObjectBuilder aa(agency.get());
            agency->add("term", VPackValue(t));
            agency->add(config_t::idStr, VPackValue(id()));
            agency->add(config_t::activeStr, _config.activeToBuilder()->slice());
            agency->add(config_t::poolStr, _config.poolToBuilder()->slice());
            agency->add("size", VPackValue(size()));
            agency->add(config_t::timeoutMultStr, VPackValue(_config.timeoutMult()));
          }}}}}

  // In case we've lost leadership, no harm will arise as the failed write
  // prevents bogus agency configuration to be replicated among agents. ***
  write(agency, WriteMode(true,true));

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

  // Wake up all waiting rest handlers
  {
    CONDITION_LOCKER(guard, _waitForCV);
    _waitForCV.broadcast();
  }

  // Wake up run
  wakeupMainLoop();
}


bool Agent::prepareLead() {

  {
    // Erase _earliestPackage, which allows for immediate sending of
    // AppendEntriesRPC when we become a leader.
    MUTEX_LOCKER(tiLocker, _tiLock);
    _earliestPackage.clear();
  }

  {
    // Clear transient for supervision start
    MUTEX_LOCKER(ioLocker, _ioLock);
    _transient.clear();
  }

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
    MUTEX_LOCKER(tiLocker, _tiLock);
    for (auto const& i : _config.active()) {
      _lastAcked[i] = steady_clock::now();
    }
  }

  return true;

}

/// Becoming leader
void Agent::lead() {

  {
    // We cannot start sendAppendentries before first log index.
    // Any missing indices before _commitIndex were compacted.
    // DO NOT EDIT without understanding the consequences for sendAppendEntries!
    index_t commitIndex;
    {
      READ_LOCKER(oLocker, _outputLock);
      commitIndex = _commitIndex;
    }

    MUTEX_LOCKER(tiLocker, _tiLock);
    for (auto& i : _confirmed) {
      if (i.first != id()) {
        i.second = commitIndex;
      }
    }
  }

  // Agency configuration
  term_t myterm;
  myterm = _constituent.term();

  persistConfiguration(myterm);

  // This is all right now, in the main loop we will wait until a
  // majority of all servers have replicated this configuration.
  // Then we will copy the _readDB to the _spearhead and start service.
}


// How long back did I take over leadership, result in seconds
int64_t Agent::leaderFor() const {
  return std::chrono::duration_cast<std::chrono::duration<int64_t>>(
    std::chrono::steady_clock::now().time_since_epoch()).count() - _leaderSince;
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


bool Agent::addGossipPeer(std::string const& endpoint) {
  return _config.addGossipPeer(endpoint);
}

void Agent::updatePeerEndpoint(std::string const& id, std::string const& ep) {
  if (_config.updateEndpoint(id, ep)) {
    if (!challengeLeadership()) {
      persistConfiguration(term());
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
void Agent::rebuildDBs() {

  term_t term = _constituent.term();

  _tiLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  WRITE_LOCKER(oLocker, _outputLock);
  CONDITION_LOCKER(guard, _waitForCV);

  index_t lastCompactionIndex;

  // We must go back to clean sheet
  _readDB.clear();
  _spearhead.clear();

  if (!_state.loadLastCompactedSnapshot(_readDB, lastCompactionIndex, term)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS);
  }

  _commitIndex = lastCompactionIndex;
  _waitForCV.broadcast();

  // Apply logs from last applied index to leader's commit index
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Rebuilding key-value stores from index "
    << lastCompactionIndex << " to " << _commitIndex << " " << _state;

  {
    auto logs = _state.slices(lastCompactionIndex+1, _commitIndex);
    _readDB.applyLogEntries(logs, _commitIndex, term,
        false /* do not send callbacks */);
  }
  _spearhead = _readDB;

  LOG_TOPIC(INFO, Logger::AGENCY)
    << id() << " rebuilt key-value stores - serving.";
}


/// Compact read db
void Agent::compact() {
  // We do not allow the case _config.compactionKeepSize() == 0, since
  // we need to keep a part of the recent log. Therefore we cannot use
  // the _readDB ever, since we have to compute a state of the key/value
  // space well before _commitIndex anyway. Apart from this, the compaction
  // code runs on the followers as well where we do not have a _readDB
  // anyway.
  index_t commitIndex;
  {
    READ_LOCKER(guard, _outputLock);
    commitIndex = _commitIndex;
  }

  if (commitIndex >= _state.nextCompactionAfter()) {
    // This check needs to be here, because the compactor thread wakes us
    // up every 5 seconds.
    // Note that it is OK to compact anywhere before or at _commitIndex.
    if (!_state.compact(commitIndex, _config.compactionKeepSize())) {
      LOG_TOPIC(WARN, Logger::AGENCY) << "Compaction for index "
        << commitIndex << " with keep size " << _config.compactionKeepSize()
        << " did not work.";
    }
  }
}


/// Last commit index
arangodb::consensus::index_t Agent::lastCommitted() const {
  READ_LOCKER(oLocker, _outputLock);
  return _commitIndex;
}

/// Last log entry
log_t Agent::lastLog() const { return _state.lastLog(); }

/// Get spearhead
Store const& Agent::spearhead() const { return _spearhead; }

/// Get _readDB reference with intentionally no lock acquired here.
///   Safe ONLY IF via executeLock() (see example Supervisor.cpp)
Store const& Agent::readDB() const {
  return _readDB;
}

/// Get readdb
arangodb::consensus::index_t Agent::readDB(Node& node) const {
  READ_LOCKER(oLocker, _outputLock);
  node = _readDB.get();
  return _commitIndex;
}

void Agent::executeLockedRead(std::function<void()> const& cb) {
  _tiLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  READ_LOCKER(oLocker, _outputLock);
  cb();
}

void Agent::executeLockedWrite(std::function<void()> const& cb) {
  _tiLock.assertNotLockedByCurrentThread();
  MUTEX_LOCKER(ioLocker, _ioLock);
  WRITE_LOCKER(oLocker, _outputLock);
  CONDITION_LOCKER(guard, _waitForCV);
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
void Agent::setPersistedState(VPackSlice const& compaction) {
  // Catch up with compacted state, this is only called at startup
  _spearhead = compaction.get("readDB");

  // Catch up with commit
  try {
    WRITE_LOCKER(oLocker, _outputLock);
    CONDITION_LOCKER(guard, _waitForCV);
    _readDB = compaction.get("readDB");
    _commitIndex =
      arangodb::basics::StringUtils::uint64(compaction.get("_key").copyString());
    _waitForCV.broadcast();
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
  }
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

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Incoming gossip: " << in->slice().toJson();

  VPackSlice slice = in->slice();
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20001,
        std::string("Gossip message must be an object. Incoming type is ") +
            slice.typeName());
  }

  if (slice.hasKey(StaticStrings::Error)) {
    if (slice.get(StaticStrings::Code).getNumber<int>() == 403) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "Gossip peer does not have us in their pool " << slice.toJson();
      FATAL_ERROR_EXIT(); /// We don't belong here
    } else {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Received gossip error. We'll retry " << slice.toJson();
    }
    query_t out = std::make_shared<Builder>();
    return out;
  }
  
  if (!slice.hasKey("id") || !slice.get("id").isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        20002, "Gossip message must contain string parameter 'id'");
  }
  std::string id = slice.get("id").copyString();

  // If pool is complete and id not in our pool reject under all circumstances
  if (_config.poolComplete() && !_config.findInPool(id)) {
    query_t ret = std::make_shared<VPackBuilder>();
     { VPackObjectBuilder o(ret.get());
    ret->add(StaticStrings::Code, VPackValue(403));
    ret->add(StaticStrings::Error, VPackValue(true));
    ret->add(StaticStrings::ErrorMessage,
             VPackValue("This agents is not member of this pool"));
     ret->add(StaticStrings::ErrorNum, VPackValue(403)); }
    return ret;
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


  LOG_TOPIC(TRACE, Logger::AGENCY) << "Received gossip " << slice.toJson();
  for (auto const& pair : VPackObjectIterator(pslice)) {
    if (!pair.value.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          20004, "Gossip message pool must contain string parameters");
    }
  }

  query_t out = std::make_shared<Builder>();

  {
    VPackObjectBuilder b(out.get());
    
    std::unordered_set<std::string> gossipPeers = _config.gossipPeers();
    if (!gossipPeers.empty() && !isCallback) {
      try {
        _config.eraseGossipPeer(endpoint);
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY)
          << __FILE__ << ":" << __LINE__ << " " << e.what();
      }
    }
    
    std::string err;
    config_t::upsert_t upsert = config_t::UNCHANGED;
    
    /// Pool incomplete or the other guy is in my pool: I'll gossip.
    if (!_config.poolComplete() || _config.matchPeer(id, endpoint)) {

      upsert = _config.upsertPool(pslice, id);
      if (upsert == config_t::WRONG) {
        LOG_TOPIC(FATAL, Logger::AGENCY) << "Discrepancy in agent pool!";
        FATAL_ERROR_EXIT();      /// disagreement over pool membership are fatal!
      }
      
      // Wrapped in envelope in RestAgencyPrivHandler
      auto pool = _config.pool();
      out->add(VPackValue("pool"));
      { VPackObjectBuilder bb(out.get());
        for (auto const& i : pool) {
          out->add(i.first, VPackValue(i.second));
        }}
       
    } else {  // Pool complete & id's endpoint not matching.

      // Not leader: redirect / 503     
      if (challengeLeadership()) {
        out->add("redirect", VPackValue(true));
        out->add("id", VPackValue(leaderID()));
      } else { // leader magic
        auto tmp = _config;
        tmp.upsertPool(pslice, id);
        auto query = std::make_shared<VPackBuilder>();
        { VPackArrayBuilder trs(query.get());
          { VPackArrayBuilder tr(query.get());
            { VPackObjectBuilder o(query.get());
              query->add(VPackValue(RECONFIGURE));
              { VPackObjectBuilder o(query.get());
                query->add("op", VPackValue("set"));
                query->add(VPackValue("new"));
                { VPackObjectBuilder c(query.get());
                  tmp.toBuilder(*query); }}}}}
         
        LOG_TOPIC(DEBUG, Logger::AGENCY)
          << "persisting new agency configuration via RAFT: " << query->toJson();
            
        // Do write
        write_ret_t ret;
        try {
          ret = write(query, WriteMode(false,true));
          arangodb::consensus::index_t max_index = 0;
          if (ret.indices.size() > 0) {
            max_index =
              *std::max_element(ret.indices.begin(), ret.indices.end());
          }
          if (max_index > 0) { // We have a RAFT index. Wait for the RAFT commit.
            auto result = waitFor(max_index);
            if (result != Agent::raft_commit_t::OK) {
              err = "failed to retrieve RAFT index for updated agency endpoints";
            } else {
              auto pool = _config.pool();
              out->add(VPackValue("pool"));
              { VPackObjectBuilder bb(out.get());
                for (auto const& i : pool) {
                  out->add(i.first, VPackValue(i.second));
                }}
            }
          } else {
            err = "failed to retrieve RAFT index for updated agency endpoints";
          }
        } catch (std::exception const& e) {
          err = std::string("failed to write new agency to RAFT") + e.what();
          LOG_TOPIC(ERR, Logger::AGENCY) << err;
        }
        
      }

      if (!err.empty()) {
        out->add(StaticStrings::Code, VPackValue(500));
        out->add(StaticStrings::Error, VPackValue(true));
        out->add(StaticStrings::ErrorMessage, VPackValue(err));
        out->add(StaticStrings::ErrorNum, VPackValue(500));
      } 
    }

    // let gossip loop know that it has new data
    if (_inception != nullptr && upsert == config_t::CHANGED) {
      _inception->signalConditionVar();
    }
    
  }

  if (!isCallback) {
    LOG_TOPIC(TRACE, Logger::AGENCY)
      << "Answering with gossip " << out->slice().toJson();
  }
    
  return out;
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
    READ_LOCKER(oLocker, _outputLock);
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

void Agent::updateConfiguration(Slice const& slice) {
  _config.updateConfiguration(slice);
}

}}  // namespace
