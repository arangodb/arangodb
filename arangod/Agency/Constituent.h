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

#ifndef __ARANGODB_CONSENSUS_CONSTITUENT__
#define __ARANGODB_CONSENSUS_CONSTITUENT__

#include <cstdint>
#include <string>
#include <vector>
#include <random>


#include "AgencyCommon.h"
#include "Basics/Thread.h"

struct TRI_server_t;
struct TRI_vocbase_t;

namespace arangodb {
class ApplicationV8;
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
  
  /// @brief Gossip protocol: listen
  void gossip(constituency_t const&);
  
  /// @brief Gossip protocol: talk
  const constituency_t& gossip();

  /// @brief Are we leading?
  bool leading() const;

  /// @brief Are we following?
  bool following() const;

  /// @brief Are we running for leadership?
  bool running() const;

  /// @brief Called by REST handler
  bool vote(term_t, id_t, index_t, term_t);

  /// @brief My daily business
  void run() override final;

  /// @brief Who is leading
  id_t leaderID () const;

  /// @brief Become follower
  void follow(term_t);

  /// @brief Agency size
  size_t size() const;

  /// @brief Orderly shutdown of thread
  void beginShutdown () override;

  bool start (TRI_vocbase_t* vocbase, ApplicationV8*, aql::QueryRegistry*);

private:

  /// @brief set term to new term
  void term (term_t);

  /// @brief Agency endpoints
  std::vector<std::string> const& end_points() const;

  /// @brief Endpoint of agent with id
  std::string const& end_point(id_t) const;

  /// @brief Run for leadership
  void candidate();

  /// @brief Become leader
  void lead();

  /// @brief Call for vote (by leader or candidates after timeout)
  void callElection();
  
  /// @brief Count my votes
  void countVotes();

  /// @brief Notify everyone, that we are good to go.
  ///        This is the task of the last process starting up.
  ///        Will be taken care of by gossip
  size_t notifyAll();
  
  /// @brief Sleep for how long
  duration_t sleepFor(double, double);

  TRI_server_t* _server;
  TRI_vocbase_t* _vocbase; 
  ApplicationV8* _applicationV8;
  aql::QueryRegistry* _queryRegistry;


  term_t               _term;         /**< @brief term number */
  std::atomic<bool>    _cast;         /**< @brief cast a vote this term */
  std::atomic<state_t> _state;        /**< @brief State (follower, candidate, leader)*/

  id_t                 _leader_id;    /**< @brief Current leader */
  id_t                 _id;           /**< @brief My own id */
  constituency_t       _constituency; /**< @brief List of consituents */
  std::mt19937         _gen;          /**< @brief Random number generator */
  role_t               _role;         /**< @brief My role */
  Agent*               _agent;        /**< @brief My boss */
  id_t                 _voted_for;

  arangodb::basics::ConditionVariable _cv;      // agency callbacks

};
  
}}

#endif //__ARANGODB_CONSENSUS_CONSTITUENT__
