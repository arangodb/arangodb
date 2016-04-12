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
#include "SanityCheck.h"
#include "State.h"
#include "Store.h"

struct TRI_server_t;
struct TRI_vocbase_t;

namespace arangodb {
class ApplicationV8;
namespace aql {
class QueryRegistry;
}

namespace consensus {

class Agent : public arangodb::Thread {

public:
  /// @brief Construct with program options
  Agent(TRI_server_t*, config_t const&, ApplicationV8*, aql::QueryRegistry*);

  /// @brief Clean up
  virtual ~Agent();

  /// @brief Get current term
  term_t term() const;

  /// @brief Get current term
  id_t id() const;

  TRI_vocbase_t* vocbase() const {
    return _vocbase;
  }

  /// @brief Vote request
  priv_rpc_ret_t requestVote(term_t, id_t, index_t, index_t, query_t const&);

  /// @brief Provide configuration
  config_t const& config() const;

  /// @brief Start thread
  bool start();

  /// @brief Verbose print of myself
  ////
  void print(arangodb::LoggerStream&) const;

  /// @brief Are we fit to run?
  bool fitness() const;

  /// @brief Leader ID
  id_t leaderID() const;

  /// @brief Are we leading?
  bool leading() const;

  /// @brief Pick up leadership tasks
  bool lead();

  /// @brief Load persistent state
  bool load();

  /// @brief Attempt write
  write_ret_t write(query_t const&);

  /// @brief Read from agency
  read_ret_t read(query_t const&) const;

  /// @brief Received by followers to replicate log entries (§5.3);
  ///        also used as heartbeat (§5.2).
  bool recvAppendEntriesRPC(term_t term, id_t leaderId, index_t prevIndex,
                            term_t prevTerm, index_t lastCommitIndex,
                            query_t const& queries);

  /// @brief Invoked by leader to replicate log entries (§5.3);
  ///        also used as heartbeat (§5.2).
  append_entries_t sendAppendEntriesRPC(id_t slave_id);

  /// @brief 1. Deal with appendEntries to slaves.
  ///        2. Report success of write processes.
  void run() override final;

  /// @brief Start orderly shutdown of threads
  void beginShutdown() override final;

  /// @brief Report appended entries from AgentCallback
  void reportIn(id_t id, index_t idx);

  /// @brief Wait for slaves to confirm appended entries
  bool waitFor(index_t last_entry, double timeout = 2.0);

  /// @brief Convencience size of agency
  size_t size() const;

  /// @brief Rebuild DBs by applying state log to empty DB
  bool rebuildDBs();

  /// @brief Last log entry
  log_t const& lastLog() const;

  /// @brief Pipe configuration to ostream
  friend std::ostream& operator<<(std::ostream& o, Agent const& a) {
    o << a.config();
    return o;
  }

  /// @brief Persist term
  void persist (term_t, id_t);

  /// @brief State machine
  State const& state() const;

  /// @brief Get read store
  Store const& readDB() const;

  /// @brief Get spearhead store
  Store const& spearhead() const; 

 private:
  TRI_server_t* _server;
  TRI_vocbase_t* _vocbase; 
  ApplicationV8* _applicationV8;
  aql::QueryRegistry* _queryRegistry;

  Constituent _constituent; /**< @brief Leader election delegate */
  SanityCheck _sanity_check; /**< @brief sanitychecking */
  State _state;             /**< @brief Log replica              */
  
  config_t _config;         /**< @brief Command line arguments   */

  std::atomic<index_t> _last_commit_index; /**< @brief Last commit index */

  arangodb::Mutex _uncommitedLock; /**< @brief  */

  Store _spearhead; /**< @brief Spearhead key value store */
  Store _read_db;   /**< @brief Read key value store */

  arangodb::basics::ConditionVariable _cv; /**< @brief Internal callbacks */
  arangodb::basics::ConditionVariable _rest_cv; /**< @brief Rest handler */

  std::vector<index_t>
      _confirmed;          /**< @brief Confirmed log index of each slave */
  arangodb::Mutex _ioLock; /**< @brief Read/Write lock */
};

}}

#endif
