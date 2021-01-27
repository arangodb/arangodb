////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_AGENT_H
#define ARANGOD_CONSENSUS_AGENT_H 1

#include "Agency/AgencyCommon.h"
#include "Agency/AgencyStrings.h"
#include "Agency/AgentConfiguration.h"
#include "Agency/AgentInterface.h"
#include "Agency/Compactor.h"
#include "Agency/Constituent.h"
#include "Agency/Inception.h"
#include "Agency/State.h"
#include "Agency/Store.h"
#include "Agency/Supervision.h"
#include "Futures/Promise.h"
#include "Basics/ConditionLocker.h"
#include "Basics/ReadWriteLock.h"
#include "RestServer/MetricsFeature.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace consensus {

class Agent final : public arangodb::Thread, public AgentInterface {
 public:
  /// @brief Construct with program options
  explicit Agent(application_features::ApplicationServer& server, config_t const&);

  /// @brief Clean up
  ~Agent();

  /// @brief bring down threads, can be called multiple times.
  void waitForThreadsStop();

  /// @brief Get current term
  term_t term() const;

  /// @brief Get current term
  std::string id() const;

  /// @brief Vote request
  priv_rpc_ret_t requestVote(term_t, std::string const&, index_t, index_t,
                             query_t const&, int64_t timeoutMult);

  /// @brief Provide configuration
  config_t const config() const;

  /// @brief Get timeoutMult:
  int64_t getTimeoutMult() const;

  /**
   * @brief add gossip peer to configuration
   * @param   endpoint  new endpoint
   * @return  true: if new endpoint, false: if already known
   */
  bool addGossipPeer(std::string const& endpoint);

  /// @brief Adjust timeoutMult:
  void adjustTimeoutMult(int64_t timeoutMult);

  /// @brief Start thread
  bool start();

  /// @brief My endpoint
  std::string endpoint() const;

  /// @brief Verbose print of myself
  void print(arangodb::LoggerStream&) const;

  /// @brief Are we fit to run?
  bool fitness() const;

  /// @brief Leader ID
  index_t lastCommitted() const;

  /// @brief Leader ID
  std::string leaderID() const;

  /// @brief Are we leading?
  bool leading() const;

  /// @brief Pick up leadership tasks
  void lead();

  /// @brief Prepare leadership
  bool prepareLead();

  /// @brief Load persistent state
  void load();

  /// @brief Unpersisted key-value-store
  trans_ret_t transient(query_t const&) override;

  /// @brief Attempt write
  ///        Startup flag should NEVER be discarded solely for purpose of
  ///        persisting the agency configuration
  write_ret_t write(query_t const&, WriteMode const& wmode = WriteMode()) override;

  /// @brief Read from agency
  read_ret_t read(query_t const&);

  /// @brief Long pool for higher index than given if leader or else empty builder and false
  std::tuple<futures::Future<query_t>, bool, std::string> poll(
    index_t const& index, double const& timeout);

  /// @brief Inquire success of logs given clientIds
  write_ret_t inquire(query_t const&);

  /// @brief Attempt read/write transaction
  trans_ret_t transact(query_t const&) override;

  /// @brief Put trxs into list of ongoing ones.
  void addTrxsOngoing(Slice trxs);

  /// @brief Remove trxs from list of ongoing ones.
  void removeTrxsOngoing(Slice trxs);

  /// @brief Check whether a trx is ongoing.
  bool isTrxOngoing(std::string& id);

  /// @brief Received by followers to replicate log entries ($5.3);
  ///        also used as heartbeat ($5.2).
  priv_rpc_ret_t recvAppendEntriesRPC(term_t term, std::string const& leaderId,
                                      index_t prevIndex, term_t prevTerm,
                                      index_t leaderCommitIndex, query_t const& queries);

  /// @brief Resign leadership
  void resign(term_t otherTerm = 0);

  /// @brief collect store callbacks for removal
  void trashStoreCallback(std::string const& url, velocypack::Slice body);

 private:

  void logsForTrigger();

  /// @brief clear expired polls registered by Agent::poll
  ///        if qu is nullptr, we're resigning.
  ///        Caller must have _promLock!
  void triggerPollsNoLock(
    query_t qu = nullptr,
    SteadyTimePoint const& tp = std::chrono::steady_clock::now() + std::chrono::seconds(60));

  /// @brief trigger all expire polls
  void clearExpiredPolls();

  /// @brief empty callback trash bin
  void emptyCbTrashBin();

  /// @brief Invoked by leader to replicate log entries ($5.3);
  ///        also used as heartbeat ($5.2).
  void sendAppendEntriesRPC();

  /// @brief check whether _confirmed indexes have been advance so that we
  /// can advance _commitIndex and apply things to readDB.
  void advanceCommitIndex();

 public:

  /// @brief Get last confirmed index of an agent. Default my own.
  ///   Safe ONLY IF via executeLock() (see example Supervision.cpp)
  index_t confirmed(std::string const& serverId = std::string()) const;

  /// @brief Invoked by leader to replicate log entries ($5.3);w
  ///        also used as heartbeat ($5.2). This is the version used by
  ///        the constituent to send out empty heartbeats to keep
  ///        the term alive.
  void sendEmptyAppendEntriesRPC(std::string const& followerId);

  /// @brief 1. Deal with appendEntries to slaves.
  ///        2. Report success of write processes.
  void run() override final;

  /// @brief Are we still booting?
  bool booting();

  /// @brief Gossip in
  query_t gossip(velocypack::Slice, bool callback = false, size_t version = 0);

  /// @brief Persisted agents
  bool persistedAgents();

  /// @brief Gossip in
  bool activeAgency();

  /// @brief Get the index at which the leader is
  index_t index();

  /// @brief Start orderly shutdown of threads
  // cppcheck-suppress virtualCallInConstructor
  void beginShutdown() override final;

  /// @brief Report appended entries from AgentCallback
  void reportIn(std::string const&, index_t, size_t = 0);

  /// @brief Report a failed append entry call from AgentCallback
  void reportFailed(std::string const& slaveId, size_t toLog, bool sent = false);

  /// @brief Wait for slaves to confirm appended entries
  AgentInterface::raft_commit_t waitFor(index_t last_entry, double timeout = 10.0) override;

  /// @brief Check if everything up to a given index has been committed:
  bool isCommitted(index_t last_entry) override;

  /// @brief Convencience size of agency
  size_t size() const;

  Supervision& supervision() { return _supervision; }
  Supervision const& supervision() const { return _supervision; }

  /// @brief Rebuild DBs by applying state log to empty DB
  void rebuildDBs();

  /// @brief Rebuild DBs by applying state log to empty DB
  void compact();

  /// @brief Last log entry
  log_t lastLog() const;

  /// @brief State machine
  State const& state() const;

  /// @brief execute a callback while holding _ioLock
  ///  and read lock for _readDB
  void executeLockedRead(std::function<void()> const& cb);

  /// @brief execute a callback while holding _ioLock
  ///  and write lock for _readDB
  void executeLockedWrite(std::function<void()> const& cb);

  /// @brief execute a callback while holding _transientLock
  void executeTransientLocked(std::function<void()> const& cb);

    /// @brief Get read store and compaction index
  index_t readDB(Node&) const;

  /// @brief Get read store and compaction index
  index_t readDB(VPackBuilder&) const;

  /// @brief Get read store
  ///  WARNING: this assumes caller holds appropriate
  ///  locks or will use executeLockedRead() or
  ///  executeLockedWrite() with a lambda function
  Store const& readDB() const;

  /// @brief Get spearhead store
  ///  WARNING: this assumes caller holds appropriate
  ///  locks or will use executeLockedRead() or
  ///  executeLockedWrite() with a lambda function
  Store const& spearhead() const;

  /// @brief Get transient store
  /// WARNING: this assumes caller holds _transientLock
  Store const& transient() const;

  /// @brief Serve active agent interface
  bool serveActiveAgent();

  /// @brief Get notification as inactive pool member
  void notify(query_t const&);

  /// @brief All there is in the state machine
  query_t allLogs() const;

  /// @brief Get copy of log entries starting with begin ending on end
  std::vector<log_t> logs(index_t begin = 0, index_t end = (std::numeric_limits<uint64_t>::max)()) const;

  /// @brief Last contact with followers
  void lastAckedAgo(Builder&) const;

  /// @brief Am I active agent
  bool active() const;

  /// @brief Are we ready for RAFT?
  bool ready() const;

  /// @brief Set readyness for RAFT
  void ready(bool b);

  /// @brief Reset RAFT timeout intervals
  void resetRAFTTimes(double, double);

  /// @brief How long back did I take over leadership, result in seconds
  int64_t leaderFor() const;

  /// @brief Update a peers endpoint in my configuration
  void updatePeerEndpoint(query_t const& message);

  /// @brief Update a peers endpoint in my configuration
  void updatePeerEndpoint(std::string const& id, std::string const& ep);

  /// @brief Assemble an agency to commitId
  query_t buildDB(index_t);

  /// @brief Guarding taking over leadership
  void beginPrepareLeadership() { _preparing = 1; }
  void donePrepareLeadership() { _preparing = 2; }
  void endPrepareLeadership() {
    _preparing = 0;
    _leaderSince = std::chrono::duration_cast<std::chrono::duration<int64_t>>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  }
  int getPrepareLeadership() { return _preparing; }

  // #brief access Inception thread
  Inception const* inception() const;

  /// @brief persist agency configuration in RAFT
  void persistConfiguration(term_t t);

  /// @brief Assignment of persisted state, only used at startup, one needs
  /// to hold the _ioLock to call this
  void setPersistedState(VPackSlice const&);

  /// @brief Get our own id
  bool id(std::string const&);

  /// @brief Merge configuration with a persisted state
  bool mergeConfiguration(VPackSlice const&);

  /// @brief Wakeup main loop of the agent (needed from Constituent)
  void wakeupMainLoop() {
    CONDITION_LOCKER(guard, _appendCV);
    _agentNeedsWakeup = true;
    _appendCV.broadcast();
  }

  /// @brief Activate this agent in single agent mode.
  void activateAgency();

  /// @brief add agent to configuration (from State after successful local persistence)
  void updateConfiguration(VPackSlice const&);

  /// @brief patch some configuration values, this is for manual interaction with
  /// the agency leader.
  void updateSomeConfigValues(VPackSlice);

  Histogram<log_scale_t<float>>& commitHist() const;

 private:

  /// @brief load() has run
  bool loaded() const;

  /// @brief Find out, if we've had acknowledged RPCs recent enough
  bool challengeLeadership();

  void syncActiveAndAcknowledged();

  /// @brief Leader election delegate
  Constituent _constituent;

  /// @brief Cluster supervision module
  Supervision _supervision;

  /// @brief State machine
  State _state;

  /// @brief Configuration of command line options
  config_t _config;

  /// @brief
  /// Leader: Last index that is "committed" in the sense that the
  /// leader has convinced itself that an absolute majority (including
  /// the leader) have written the entry into their log. This is also
  /// the index of the highest log entry applied to the state machine
  /// _readDB (called "lastApplied" in the Raft paper).
  /// Follower: this indicates what the leader told them it has last
  /// "committed" in the above sense.
  /// Locking policy: Note that this is only ever changed at startup, when
  /// answers to appendEntriesRPC messages come in on the leader, and when
  /// appendEntriesRPC calls are received on the follower. In each case
  /// we hold the _ioLock when _commitIndex is changed. Reading and writing
  /// must be done under the write lock of _outputLog and the mutex of
  /// _waitForCV to allow a thread to wait for a change using that
  /// condition variable.
  /// Furthermore, we make the member atomic, such that we can occasionally
  /// read it without the locks, for example in sendEmptyAppendEntriesRPC.
  std::atomic<index_t> _commitIndex;

  /// @brief Spearhead (write) kv-store
  Store _spearhead;

  /// @brief Committed (read) kv-store
  Store _readDB;

  /// @brief Committed (read) kv-store for transient data. This is
  /// protected by the _transientLock mutex.
  Store _transient;

  /// @brief Condition variable for appending to the log and for
  /// AgentCallbacks. This is used by the main agent thread to go
  /// to sleep when all necessary checks have been performed. When
  /// new local log entries have been appended to the log or when
  /// followers have confirmed more replications, one needs to set the
  /// flag _agentNeedsWakeup (under the mutex) and then broadcast on
  /// _appendCV. This will wake up the agent thread immediately.
  arangodb::basics::ConditionVariable _appendCV;
  bool _agentNeedsWakeup;

  /// The following two members are strictly only used in the
  /// Agent thread in sendAppendEntriesRPC. Therefore no protection is
  /// necessary for these:

  /// @brief _lastSent stores for each follower the time stamp of the time
  /// when the main Agent thread has last sent a non-empty
  /// appendEntriesRPC to that follower.
  std::unordered_map<std::string, SteadyTimePoint> _lastSent;

  /// The following three members are protected by _tiLock:

  /// @brief stores for each follower the highest index log it has reported as
  /// locally logged, and the timestamp we last recevied an answer to sendAppendEntries
  std::unordered_map<std::string, std::pair<SteadyTimePoint, index_t>> _lastAckedIndex;

  /// @brief The earliest timepoint at which we will send new sendAppendEntries
  /// to a particular follower. This is a measure to avoid bombarding a
  /// follower, that has trouble keeping up.
  std::unordered_map<std::string, SteadyTimePoint> _earliestPackage;

  // @brief Lock for the above time data about other agents. This
  // protects _confirmed, _lastAcked and _earliestPackage:
  mutable arangodb::Mutex _tiLock;

  /// @brief Facilitate quick note of followership on leaders
  mutable arangodb::Mutex _emptyAppendLock;
  std::unordered_map<std::string, SteadyTimePoint> _lastEmptyAcked;

  /// @brief RAFT consistency lock:
  ///   _spearhead
  ///
  mutable arangodb::Mutex _ioLock;

  /// @brief Callback trash bin lock
  ///   _callbackTrashBin
  ///
  mutable arangodb::Mutex _cbtLock;

  /// @brief RAFT consistency lock:
  ///   _readDB and _commitIndex
  /// Allows reading from one or both if used alone.
  /// Writing requires this held first, then _waitForCV's mutex
  mutable arangodb::basics::ReadWriteLock _outputLock;

  /// @brief The following mutex protects the _transient store. It is
  /// needed for all accesses to _transient.
  mutable arangodb::Mutex _transientLock;

  /// @brief RAFT consistency lock and update notifier:
  ///   _readDB and _commitIndex
  /// _waitForCV's mutex held alone, allows reads from _readDB or _commitIndex.
  /// Writing requires _outputLock in Write mode first, then _waitForCV's mutex
  ///
  /// Condition variable for waiting for confirmation. This is used
  /// in threads that wait until the _commitIndex has reached a certain
  /// index. Whenever _commitIndex is advanced (by incoming confirmations
  /// in AgentCallbacks and later discovery in advanceCommitIndex). All
  /// changes to _commitIndex are done under the mutex of _waitForCV
  /// and are followed by a broadcast on this condition variable.
  mutable arangodb::basics::ConditionVariable _waitForCV;

  /// Rules for access and locks: This covers the following locks:
  ///    _ioLock (here)
  ///    _logLock (in State)
  ///    _outputLock reading or writing
  ///    _waitForCV
  ///    _tiLock (here)
  /// One may never acquire a log in this list whilst holding another one
  /// that appears further down on this list. This is to prevent deadlock.
  //
  /// For _logLock: This is local to State and we make sure that the few
  /// functions in State that call Agent methods only call those that do
  /// not acquire the _ioLock. They only call Agent::setPersistedState which
  /// acquires _outputLock and _waitForCV but this is OK.
  //
  /// For _ioLock: We put in assertions to ensure that when this lock is
  /// acquired we do not have the _tiLock.

  /// @brief Inception thread getting an agent up to join RAFT from cmd or persistence
  std::unique_ptr<Inception> _inception;

  /// @brief Compactor
  Compactor _compactor;

  /// @brief Agent is ready for RAFT
  std::atomic<bool> _ready;
  std::atomic<int> _preparing;  // 0 means not preparing, 1 means preparations
                                // scheduled, 2 means preparations done, only
                                // waiting until _commitIndex is at end of
                                // our log

  /// @brief Keep track of when I last took on leadership, this is seconds
  /// since the epoch of the steady clock.
  std::atomic<int64_t> _leaderSince;

  /// @brief load() has completed
  std::atomic<bool> _loaded;

  /// @brief Container for callbacks for removal
  std::unordered_map<std::string, std::unordered_set<std::string>> _callbackTrashBin;
  std::chrono::time_point<std::chrono::steady_clock> _callbackLastPurged;

  /// @brief Ids of ongoing transactions, used for inquire:
  std::unordered_set<std::string> _ongoingTrxs;

  // lock for _ongoingTrxs
  arangodb::Mutex _trxsLock;

  // @brief promises for poll interface and the guard
  //        The map holds all current poll promises.
  //        key,value: expiry time of this poll, the promise
  //        When expired or when any change to commitIndex, promise is fullfilled
  //        All rest handlers will receive the same vpack,
  //        They need to sort out, what is sent to client
  std::mutex _promLock;
  index_t _lowestPromise;
  std::multimap<SteadyTimePoint, futures::Promise<query_t>> _promises;

  Counter& _write_ok;
  Counter& _write_no_leader;
  Counter& _read_ok;
  Counter& _read_no_leader;
  Histogram<log_scale_t<float>>& _write_hist_msec;
  Histogram<log_scale_t<float>>& _commit_hist_msec;
  Histogram<log_scale_t<float>>& _append_hist_msec;
  Histogram<log_scale_t<float>>& _compaction_hist_msec;
  Gauge<uint64_t>& _local_index;

};
}  // namespace consensus
}  // namespace arangodb

#endif
