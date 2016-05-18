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

#include <random>

#include "AgencyCommon.h"
#include "AgentConfiguration.h"
#include "NotifierThread.h"

#include "Basics/Common.h"
#include "Basics/Thread.h"
#include "Basics/ConditionVariable.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {
class QueryRegistry;
}

namespace consensus {

class Agent;

/// @brief RAFT leader election
class Constituent : public arangodb::Thread {
 public:
  /// @brief Distribution type
  typedef std::uniform_real_distribution<double> dist_t;

  /// @brief Default ctor
  Constituent();

  /// @brief Clean up and exit election
  virtual ~Constituent();

  /// @brief Configure with agent's configuration
  void configure(Agent*);

  /// @brief Current term
  term_t term() const;

  /// @brief Get current role
  role_t role() const;

  /// @brief Are we leading?
  bool leading() const;

  /// @brief Are we following?
  bool following() const;

  /// @brief Are we running for leadership?
  bool running() const;

  /// @brief Called by REST handler
  bool vote(term_t, arangodb::consensus::id_t, index_t, term_t);

  /// @brief My daily business
  void run() override final;

  /// @brief Who is leading
  arangodb::consensus::id_t leaderID() const;

  /// @brief Configuration
  config_t const& config() const;

  /// @brief Become follower
  void follow(term_t);

  /// @brief Agency size
  size_t size() const;

  /// @brief Orderly shutdown of thread
  void beginShutdown() override;

  bool start(TRI_vocbase_t* vocbase, aql::QueryRegistry*);

 private:
  /// @brief set term to new term
  void term(term_t);

  /// @brief Agency endpoints
  std::vector<std::string> const& endpoints() const;

  /// @brief Endpoint of agent with id
  std::string const& endpoint(arangodb::consensus::id_t) const;

  /// @brief Run for leadership
  void candidate();

  /// @brief Become leader
  void lead();

  /// @brief Call for vote (by leader or candidates after timeout)
  void callElection();

  /// @brief Count my votes
  void countVotes();

  /// @brief Wait for sync
  bool waitForSync() const;

  /// @brief Notify everyone, that we are good to go.
  ///        This is the task of the last process starting up.
  ///        Will be taken care of by gossip
  void notifyAll();

  /// @brief Sleep for how long
  duration_t sleepFor(double, double);

  TRI_vocbase_t* _vocbase;
  aql::QueryRegistry* _queryRegistry;

  term_t _term;                /**< @brief term number */
  std::atomic<bool> _cast;     /**< @brief cast a vote this term */
  std::atomic<state_t> _state; /**< @brief State (follower, candidate, leader)*/

  arangodb::consensus::id_t _leaderID; /**< @brief Current leader */
  arangodb::consensus::id_t _id;       /**< @brief My own id */
  constituency_t _constituency;        /**< @brief List of consituents */
  std::mt19937 _gen;                   /**< @brief Random number generator */
  role_t _role;                        /**< @brief My role */
  Agent* _agent;                       /**< @brief My boss */
  arangodb::consensus::id_t _votedFor;

  std::unique_ptr<NotifierThread> _notifier;

  arangodb::basics::ConditionVariable _cv;  // agency callbacks
  mutable arangodb::Mutex _castLock;
};
}
}

#endif
