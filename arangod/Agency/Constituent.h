#ifndef __ARANGODB_CONSENSUS_CONSTITUENT__
#define __ARANGODB_CONSENSUS_CONSTITUENT__

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include <random>

namespace arangodb {
namespace consensus {

/**
 * @brief Raft leader election
 */
class Constituent {
  
public:
  
  enum mode_t {
    LEADER, CANDIDATE, FOLLOWER
  };
  typedef std::chrono::duration<double> duration_t;
  typedef uint64_t term_t;
  typedef uint32_t id_t;
  struct  constituent_t {
    id_t id;
    std::string address;
    std::string port;
  };
  typedef std::vector<constituent_t> constituency_t;
  typedef uint32_t state_t;
  typedef std::uniform_real_distribution<double> dist_t; 
  
  /**
   * brief Construct with size of constituency
   */
  Constituent ();
  
  /**
   * brief Construct with size of constituency
   */
  Constituent (const uint32_t n);
  
  /**
   * brief Construct with subset of peers
   */
  Constituent (constituency_t const & constituency);
  
  /**
   * @brief Clean up and exit election
   */
  virtual ~Constituent ();
  
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
  constituency_t _constituency; /**< @brief List of consituents */
  uint32_t _nvotes;            /**< @brief Votes in my favour
                                * (candidate/leader) */
  state_t _state;             /**< @brief State (follower,
                               * candidate, leader)*/
  std::mt19937 _gen;
  
  mode_t _mode;
  
  std::vector<bool> _votes; 
  
};
  
}}

#endif //__ARANGODB_CONSENSUS_CONSTITUENT__
