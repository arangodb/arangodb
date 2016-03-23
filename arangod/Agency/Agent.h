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

#ifndef __ARANGODB_CONSENSUS_AGENT__
#define __ARANGODB_CONSENSUS_AGENT__

#include "AgencyCommon.h"
#include "AgentCallback.h"
#include "Constituent.h"
#include "State.h"
#include "Store.h"

namespace arangodb {
namespace consensus {
    
class Agent : public arangodb::Thread {
  
public:
  
  /**
   * @brief Default ctor
   */
  Agent ();
  
  /**
   * @brief Construct with program options
   */
  Agent (config_t const&);

  /**
   * @brief Clean up
   */
  virtual ~Agent();
  
  /**
   * @brief Get current term
   */
  term_t term() const;
  
  /**
   * @brief Get current term
   */
  id_t id() const;
  
  /**
   * @brief Vote request
   */
  priv_rpc_ret_t requestVote(term_t , id_t, index_t, index_t, query_t const&);
  
  /**
   * @brief Provide configuration
   */
  config_t const& config () const;
  
  /**
   * @brief Start thread
   */
  bool start ();
  
  /**
   * @brief Verbose print of myself
   */ 
  void print (arangodb::LoggerStream&) const;
  
  /**
   * @brief Are we fit to run?
   */
  bool fitness () const;
  
  /**
   * @brief 
   */
  void report (status_t);
  
  /**
   * @brief Leader ID
   */
  id_t leaderID () const;
  bool leading () const;

  bool lead ();

  bool load ();

  /**
   * @brief Attempt write
   */
  write_ret_t write (query_t const&);
  
  /**
   * @brief Read from agency
   */
  read_ret_t read (query_t const&) const;
  
  /**
   * @brief Received by followers to replicate log entries (§5.3);
   *        also used as heartbeat (§5.2).
   */
  bool recvAppendEntriesRPC (term_t term, id_t leaderId, index_t prevIndex,
    term_t prevTerm, index_t lastCommitIndex, query_t const& queries);

  /**
   * @brief Invoked by leader to replicate log entries (§5.3);
   *        also used as heartbeat (§5.2).
   */
  append_entries_t sendAppendEntriesRPC (id_t slave_id);
    
  /**
   * @brief 1. Deal with appendEntries to slaves.
   *        2. Report success of write processes. 
   */
  void run () override;
  void beginShutdown () override;

  /**
   * @brief Report appended entries from AgentCallback
   */
  void reportIn (id_t id, index_t idx);
  
  /**
   * @brief Wait for slaves to confirm appended entries
   */
  bool waitFor (index_t last_entry, duration_t timeout = duration_t(2.0));

  /**
   * @brief Convencience size of agency
   */
  size_t size() const;

  /**
   * @brief Rebuild DBs by applying state log to empty DB
   */
  bool rebuildDBs();

  /**
   * @brief Last log entry
   */
  log_t const& lastLog () const;

  friend std::ostream& operator<< (std::ostream& o, Agent const& a) {
    o << a.config();
    return o;
  }

  State const& state() const;

private:

  Constituent _constituent; /**< @brief Leader election delegate */
  State       _state;       /**< @brief Log replica              */
  config_t    _config;      /**< @brief Command line arguments   */

  std::atomic<index_t> _last_commit_index;

  arangodb::Mutex _uncommitedLock;
  
  Store _spearhead;
  Store _read_db;
  
  AgentCallback _agent_callback;

  arangodb::basics::ConditionVariable _cv;      // agency callbacks
  arangodb::basics::ConditionVariable _rest_cv; // rest handler


  std::atomic<bool> _stopping;

  std::vector<index_t> _confirmed;
  arangodb::Mutex _ioLock;
  
};

}}

#endif
