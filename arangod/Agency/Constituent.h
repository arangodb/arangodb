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
#include <chrono>
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
class Constituent : public arangodb::basics::Thread {
  
public:

  enum mode_t {
    LEADER, CANDIDATE, FOLLOWER
  };
  typedef std::chrono::duration<double> duration_t;
  typedef uint64_t term_t;
  typedef uint32_t id_t;
  struct  constituent_t {
    id_t id;
    std::string endpoint;
  };
  typedef std::vector<constituent_t> constituency_t;
  typedef uint32_t state_t;
  typedef std::uniform_real_distribution<double> dist_t; 
  
  Constituent ();
  
  /**
   * @brief Clean up and exit election
   */
  virtual ~Constituent ();
  
  void configure (Agent*);

  term_t term() const;
  
  void runForLeaderShip (bool b);
  
  state_t state () const;
  
  mode_t mode () const;
  
  /**
   * @brief Gossip protocol: listen
   */
  void gossip (Constituent::constituency_t const& constituency);
  
  /**
   * @brief Gossip protocol: talk
   */
  const Constituent::constituency_t& gossip ();
  
  bool leader() const;

  bool vote(id_t, term_t);

  void run();

  void voteCallBack();

private:
  
  /**
   * @brief Call for vote (by leader or candidates after timeout)
   */
  void callElection ();
  
  /**
   * @brief Count my votes
   */
  void countVotes();
  
  /**
   * @brief Sleep for how long
   */
  duration_t sleepFor ();
  
  term_t _term;                /**< @brief term number */
  id_t _leader_id;           /**< @brief Current leader */
  id_t _cur_vote;            /**< @brief My current vote */
  id_t _id;
  constituency_t _constituency; /**< @brief List of consituents */
  uint32_t _nvotes;            /**< @brief Votes in my favour
                                * (candidate/leader) */
  state_t _state;             /**< @brief State (follower,
                               * candidate, leader)*/
  std::mt19937 _gen;
  
  mode_t _mode;
  
  bool _run;

  std::vector<bool> _votes; 
  
  Agent* _agent;

};
  
}}

#endif //__ARANGODB_CONSENSUS_CONSTITUENT__
