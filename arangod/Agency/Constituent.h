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

#ifndef ARANGOD_CONSENSUS_CONSTITUENT_H
#define ARANGOD_CONSENSUS_CONSTITUENT_H 1

#include "AgencyCommon.h"

#include "AgentConfiguration.h"
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

#include <list>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class QueryRegistry;
}

namespace consensus {

static inline double readSystemClock() {
  return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}

class Agent;

// RAFT leader election
class Constituent : public Thread {
 public:
  Constituent();

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
  bool vote(term_t, std::string, index_t, term_t);

  // Check leader
  bool checkLeader(term_t, std::string, index_t, term_t);

  // My daily business
  void run() override final;

  // Who is leading
  std::string leaderID() const;

  // Configuration
  config_t const& config() const;

  // Become follower
  void follow(term_t);
  void followNoLock(term_t);

  // Agency size
  size_t size() const;

  // Orderly shutdown of thread
  void beginShutdown() override;

  bool start(TRI_vocbase_t* vocbase, aql::QueryRegistry*);

  friend class Agent;

 private:
  // update leaderId and term if inactive
  void update(std::string const&, term_t);

  // set term to new term
  void term(term_t);
  void termNoLock(term_t);

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
  aql::QueryRegistry* _queryRegistry;

  term_t _term;            // term number
  bool _cast;              // cast a vote this term

  std::string _leaderID; // Current leader
  std::string _id;       // My own id

  double _lastHeartbeatSeen;

  role_t _role;  // My role
  Agent* _agent; // My boss
  std::string _votedFor;

  arangodb::basics::ConditionVariable _cv;  // agency callbacks
  mutable arangodb::Mutex _castLock;

  // Keep track of times of last few elections:
  mutable arangodb::Mutex _recentElectionsMutex;
  std::list<double> _recentElections;
};
}
}

#endif
