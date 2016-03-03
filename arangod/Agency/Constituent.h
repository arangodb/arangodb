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

namespace arangodb {
namespace consensus {

class Agent;

/**
 * @brief Raft leader election
 */
class Constituent : public arangodb::Thread {
  
public:

  typedef std::uniform_real_distribution<double> dist_t; 
  
  Constituent();
  
  /**
   * @brief Clean up and exit election
   */
  virtual ~Constituent();
  
  void configure(Agent*);

  term_t term() const;
  
  void runForLeaderShip (bool b);
  
  role_t role() const;
  
  /**
   * @brief Gossip protocol: listen
   */
  void gossip(constituency_t const&);
  
  /**
   * @brief Gossip protocol: talk
   */
  const constituency_t& gossip();

  bool leading() const;
  bool following() const;
  bool running() const;

  /**
   * @brief Called by REST handler
   */
  bool vote(term_t, id_t, index_t, term_t);

  /**
   * @brief My daily business
   */
  void run();

  /**
   * @brief Who is leading
   */
  id_t leaderID () const;

  /**
   * @brief Become follower
   */
  void follow(term_t);

  /**
   * @brief Agency size
   */
  size_t size() const;

  void beginShutdown () override;

private:
  
  std::vector<std::string> const& end_points() const;
  std::string const& end_point(id_t) const;

  /**
   * @brief Run for leadership
   */
  void candidate();

  /**
   * @brief Become leader
   */
  void lead();

  /**
   * @brief Call for vote (by leader or candidates after timeout)
   */
  void callElection();
  
  /**
   * @brief Count my votes
   */
  void countVotes();

  /**
   * @brief Notify everyone, that we are good to go.
   *        This is the task of the last process starting up.
   *        Will be taken care of by gossip
   */
  size_t notifyAll();
  
  /**
   * @brief Sleep for how long
   */
  duration_t sleepFor(double, double);
  double sleepFord(double, double);

  // mission critical
  term_t  _term;         /**< @brief term number */
  std::atomic<bool>    _cast;         /**< @brief cast a vote this term */
  std::atomic<state_t> _state;        /**< @brief State (follower, candidate, leader)*/

  // just critical
  id_t                 _leader_id;    /**< @brief Current leader */
  id_t                 _id;           /**< @brief My own id */
  constituency_t       _constituency; /**< @brief List of consituents */
  std::mt19937         _gen;          /**< @brief Random number generator */
  role_t               _role;         /**< @brief My role */
  std::vector<bool>    _votes;        /**< @brief My list of votes cast in my favour*/
  Agent*               _agent;        /**< @brief My boss */
  
  arangodb::basics::ConditionVariable _cv;      // agency callbacks

};
  
}}

#endif //__ARANGODB_CONSENSUS_CONSTITUENT__
