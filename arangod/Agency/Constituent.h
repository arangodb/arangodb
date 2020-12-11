////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_CONSTITUENT_H
#define ARANGOD_CONSENSUS_CONSTITUENT_H 1

#include "AgencyCommon.h"

#include "AgentConfiguration.h"
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "RestServer/MetricsFeature.h"

#include <list>

struct TRI_vocbase_t;

namespace arangodb {
namespace consensus {

static inline double steadyClockToDouble() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

class Agent;

// RAFT leader election
class Constituent : public Thread {
 public:
  explicit Constituent(application_features::ApplicationServer&);

  // clean up and exit election
  virtual ~Constituent();

  // Configure with agent's configuration
  void configure(Agent*);

  // Current term
  term_t term() const;

  // Get current role
  role_t role() const;

  // Are we leading?
  bool leading() const;

  // Are we following?
  bool following() const;

  // Are we running for leadership?
  bool running() const;

  // Called by REST handler
  bool vote(term_t termOfPeer, std::string const& id, index_t prevLogIndex, term_t prevLogTerm);

  // Check leader
  bool checkLeader(term_t term, std::string const& id, index_t prevLogIndex, term_t prevLogTerm);

  // Notify about heartbeat being sent out:
  void notifyHeartbeatSent(std::string const& followerId);

  // My daily business
  void run() override final;

  // Who is leading
  std::string leaderID() const;

  // Configuration
  config_t const& config() const;

  // Become follower, if t is > 0, termNoLock is called and _term and
  // _votedFor is set as a consequence. If t is 0, neither _term nor
  // _votedFor are adjusted! This, or calling term or termNoLock is the
  // only way to change _term or _votedFor that is allowed!
  void follow(term_t t, std::string const& votedFor = NO_LEADER);
  void followNoLock(term_t t, std::string const& votedFor = NO_LEADER);

  // Agency size
  size_t size() const;

  // Orderly shutdown of thread
  void beginShutdown() override;

  bool start(TRI_vocbase_t* vocbase);

  // update leaderId and term if inactive
  void update(std::string const&, term_t);

 private:
  // set term to new term, will always overwrite _term, so term 0 is
  // not allowed. This will always overwrite _votedFor!
  void term(term_t term, std::string const& votedFor);
  void termNoLock(term_t term, std::string const& votedFor);

  // Agency endpoints
  std::vector<std::string> const& endpoints() const;

  // Endpoint of agent with id
  std::string endpoint(std::string) const;

  // Run for leadership
  void candidate();

  // Become leader
  void lead(term_t);

  // Call for vote (by leader or candidates after timeout)
  void callElection();

  // Count my votes
  void countVotes();

  // Wait for sync
  bool waitForSync() const;

  // Check if log up to date with ours
  bool logUpToDate(index_t, term_t) const;

  // Check if log start matches entry in my log
  bool logMatches(index_t, term_t) const;

  // Count election events which are more recent than `threshold` seconds.
  int64_t countRecentElectionEvents(double threshold);

  TRI_vocbase_t* _vocbase;

  term_t _term;  // term number
  Gauge<term_t>& _gterm;  // term number

  std::string _leaderID;  // Current leader
  std::string _id;        // My own id

  // Last time an AppendEntriesRPC message has arrived, this is used to
  // organize out-of-patience in the follower. Note that this variable is
  // also set to the current time when a vote is cast, either for ourselves
  // or for somebody else. The constituent calls for an election if and only
  // if the time since _lastHeartbeatSeen is greater than a random timeout:
  std::atomic<double> _lastHeartbeatSeen;

  std::atomic<role_t> _role;           // My role
  // We use this to read off leadership without acquiring a lock.
  // It is still only changed under _termVoteLock.
  Agent* _agent;          // My boss
  std::string _votedFor;  // indicates whether or not we have voted for
                          // anybody in this term, we will always reset
                          // this to NO_LEADER if _term is advanced
                          // unless we immediately cast a vote for us or
                          // somebody else, _term and _votedFor are only
                          // ever changed together via the termNoLock
                          // method (which might be called through term,
                          // followNoLock or follow), termNoLock persists
                          // the pair for every change

  arangodb::basics::ConditionVariable _cv;  // this is  only used to wake
                                            // up the Constituent thread
                                            // when an AgentCallback
                                            // arrives
  mutable arangodb::Mutex _termVoteLock;
  // This mutex protects _term, _votedFor, _role and _leaderID, note that
  // all this Constituent data is usually only accessed from the Constituent
  // thread. However, the AgentCallback is executed in a Scheduler thread
  // which calls methods of Constituent. This is why we need mutexes here.

  // Keep track of times of last few elections:
  mutable arangodb::Mutex _recentElectionsMutex;
  std::list<double> _recentElections;

  // For leader case: Last time we have sent out AppendEntriesRPC message
  // to some follower, this is used to find out if additional empty
  // heartbeats have to be sent out by the Constituent:
  std::unordered_map<std::string, double> _lastHeartbeatSent;

  /// @brief _heartBeatMutex, protection for _lastHeartbeatSent
  mutable arangodb::Mutex _heartBeatMutex;
};
}  // namespace consensus
}  // namespace arangodb

#endif
