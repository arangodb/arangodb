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

#ifndef ARANGOD_CONSENSUS_AGENT_H
#define ARANGOD_CONSENSUS_AGENT_H 1

#include "Agency/AgencyCommon.h"
#include "Agency/AgentActivator.h"
#include "Agency/AgentCallback.h"
#include "Agency/AgentConfiguration.h"
#include "Agency/AgentInterface.h"
#include "Agency/Compactor.h"
#include "Agency/Constituent.h"
#include "Agency/Inception.h"
#include "Agency/State.h"
#include "Agency/Store.h"
#include "Agency/Supervision.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace consensus {

class Agent : public arangodb::Thread,
              public AgentInterface {

 public:
  /// @brief Construct with program options
  explicit Agent(config_t const&);

  /// @brief Clean up
  ~Agent();

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
  std::pair<index_t, index_t> lastCommitted() const;

  /// @brief Leader ID
  std::string leaderID() const;

  /// @brief Are we leading?
  bool leading() const;

  /// @brief Pick up leadership tasks
  void lead();

  /// @brief Prepare leadership
  bool prepareLead();

  /// @brief Unprepare for leadership, needed when we resign during preparation
  void unprepareLead() {
    _preparing = false;
  }

  /// @brief Load persistent state
  void load();

  /// @brief Unpersisted key-value-store
  trans_ret_t transient(query_t const&) override;

  /// @brief Attempt write
  ///        Startup flag should NEVER be discarded solely for purpose of
  ///        persisting the agency configuration
  write_ret_t write(query_t const&, bool discardStartup = false) override;

  /// @brief Read from agency
  read_ret_t read(query_t const&);

  /// @brief Inquire success of logs given clientIds
  inquire_ret_t inquire(query_t const&);

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
  bool recvAppendEntriesRPC(term_t term, std::string const& leaderId,
                            index_t prevIndex, term_t prevTerm,
                            index_t leaderCommitIndex, query_t const& queries);

  /// @brief Invoked by leader to replicate log entries ($5.3);
  ///        also used as heartbeat ($5.2).
  void sendAppendEntriesRPC();

  /// @brief 1. Deal with appendEntries to slaves.
  ///        2. Report success of write processes.
  void run() override final;

  /// @brief Are we still booting?
  bool booting();

  /// @brief Gossip in
  query_t gossip(query_t const&, bool callback = false, size_t version = 0);

  /// @brief Persisted agents
  bool persistedAgents();

  /// @brief Activate new agent in pool to replace failed
  void reportActivated(std::string const&, std::string const&, query_t);

  /// @brief Activate new agent in pool to replace failed
  void failedActivation(std::string const&, std::string const&);

  /// @brief Gossip in
  bool activeAgency();

  /// @brief Start orderly shutdown of threads
  void beginShutdown() override final;

  /// @brief Report appended entries from AgentCallback
  void reportIn(std::string const&, index_t, size_t = 0);

  /// @brief Wait for slaves to confirm appended entries
  AgentInterface::raft_commit_t waitFor(index_t last_entry, double timeout = 2.0) override;

  /// @brief Convencience size of agency
  size_t size() const;

  /// @brief Rebuild DBs by applying state log to empty DB
  index_t rebuildDBs();

  /// @brief Rebuild DBs by applying state log to empty DB
  void compact();

  /// @brief Last log entry
  log_t lastLog() const;

  /// @brief State machine
  State const& state() const;

  /// @brief execute a callback while holding _ioLock
  void executeLocked(std::function<void()> const& cb);

  /// @brief Get read store and compaction index
  index_t readDB(Node&) const;

  /// @brief Get read store
  Store const& readDB() const;

  /// @brief Get spearhead store
  Store const& spearhead() const;

  /// @brief Get transient store
  Store const& transient() const;

  /// @brief Serve active agent interface
  bool serveActiveAgent();

  /// @brief Start constituent
  void startConstituent();

  /// @brief Get notification as inactive pool member
  void notify(query_t const&);

  /// @brief Detect active agent failures
  void detectActiveAgentFailures();

  /// @brief All there is in the state machine
  query_t allLogs() const;

  /// @brief Last contact with followers
  query_t lastAckedAgo() const;

  /// @brief Am I active agent
  bool active() const;

  /// @brief Become active agent
  query_t activate(query_t const&);

  /// @brief Report measured round trips to inception
  void reportMeasurement(query_t const&);

  /// @brief Are we ready for RAFT?
  bool ready() const;

  /// @brief Set readyness for RAFT
  void ready(bool b);

  /// @brief Reset RAFT timeout intervals
  void resetRAFTTimes(double, double);

  /// @brief Get start time of leadership
  TimePoint const& leaderSince() const;

  /// @brief Update a peers endpoint in my configuration
  void updatePeerEndpoint(query_t const& message);

  /// @brief Update a peers endpoint in my configuration
  void updatePeerEndpoint(std::string const& id, std::string const& ep);

  /// @brief Assemble an agency to commitId
  query_t buildDB(index_t);

  /// @brief Guarding taking over leadership
  void beginPrepareLeadership() { _preparing = true; }
  void endPrepareLeadership()  { _preparing = false; }

  // #brief access Inception thread
  Inception const* inception() const;

  /// @brief State reads persisted state and prepares the agent
  friend class State;
  friend class Compactor;

 private:

  /// @brief persist agency configuration in RAFT
  void persistConfiguration(term_t t);

  /// @brief Find out, if we've had acknowledged RPCs recent enough
  bool challengeLeadership();

  /// @brief Notify inactive pool members of changes in configuration
  void notifyInactive() const;

  /// @brief Activate this agent in single agent mode.
  bool activateAgency();

  /// @brief Assignment of persisted state
  Agent& operator=(VPackSlice const&);

  /// @brief Get current term
  bool id(std::string const&);

  /// @brief Get current term
  bool mergeConfiguration(VPackSlice const&);

  /// @brief Leader ID
  void lastCommitted(index_t);

  /// @brief Leader election delegate
  Constituent _constituent;

  /// @brief Cluster supervision module
  Supervision _supervision;

  /// @brief State machine
  State _state;

  /// @brief Configuration of command line options
  config_t _config;

  /// @brief
  /// Leader: Last index that is "committed" in the sense that the leader
  /// has convinced itself that an absolute majority (including the leader)
  /// have written the entry into their log, this variable is only maintained
  /// on the leader.
  /// Follower: this is only kept on followers and indicates what the leader
  /// told them it has last "committed" in the above sense.
  index_t _commitIndex;

  /// @brief Index of highest log entry applied to state achine (initialized
  /// to 0, increases monotonically)
  index_t _lastApplied;

  /// @brief Spearhead (write) kv-store
  Store _spearhead;

  /// @brief Committed (read) kv-store
  Store _readDB;

  /// @brief Committed (read) kv-store for transient data
  Store _transient;

  /// @brief Condition variable for appendEntries
  arangodb::basics::ConditionVariable _appendCV;

  /// @brief Condition variable for waitFor
  arangodb::basics::ConditionVariable _waitForCV;

  /// @brief Confirmed indices of all members of agency
  std::unordered_map<std::string, index_t> _confirmed;
  std::unordered_map<std::string, index_t> _lastHighest;

  std::unordered_map<std::string, TimePoint> _lastAcked;
  std::unordered_map<std::string, TimePoint> _lastSent;
  std::unordered_map<std::string, TimePoint> _earliestPackage;

  /**< @brief RAFT consistency lock:
     _spearhead
     _readDB
     _commitIndex (log index)
     _lastAcked
     _lastSent
     _confirmed
     _nextCompactionAfter
   */
  mutable arangodb::Mutex _ioLock;

  // lock for _leaderCommitIndex
  mutable arangodb::Mutex _liLock;

  // note: when both _ioLock and _liLock are acquired,
  // the locking order must be:
  // 1) _ioLock
  // 2) _liLock

  // @brief guard _activator 
  mutable arangodb::Mutex _activatorLock;

  /// @brief Next compaction after
  index_t _nextCompactionAfter;

  /// @brief Inception thread getting an agent up to join RAFT from cmd or persistence
  std::unique_ptr<Inception> _inception;

  /// @brief Activator thread for the leader to wake up a sleeping agent from pool
  std::unique_ptr<AgentActivator> _activator;

  /// @brief Compactor
  Compactor _compactor;

  /// @brief Agent is ready for RAFT
  std::atomic<bool> _ready;
  std::atomic<bool> _preparing;

  /// @brief Keep track of when I last took on leadership
  TimePoint _leaderSince;
  
  /// @brief Ids of ongoing transactions, used for inquire:
  std::unordered_set<std::string> _ongoingTrxs;

  // lock for _ongoingTrxs
  arangodb::Mutex _trxsLock;
 
 public:
  mutable arangodb::Mutex _compactionLock;
};
}
}

#endif
