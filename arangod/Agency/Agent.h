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
#include "Constituent.h"
#include "State.h"
#include "Store.h"

namespace arangodb {
namespace consensus {
    
class Agent : public arangodb::Thread { // We need to asynchroneously append entries
  
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
  query_t requestVote(term_t , id_t, index_t, index_t);
  
  /**
   * @brief Provide configuration
   */
  Config<double> const& config () const;
  
  /**
   * @brief Start thread
   */
  void start ();
  
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

  /**
   * @brief Attempt write
   */
  query_ret_t write (query_t const&);
  
  /**
   * @brief Read from agency
   */
  query_ret_t read (query_t const&) const;
  
  /**
   * @brief Invoked by leader to replicate log entries (§5.3);
   *        also used as heartbeat (§5.2).
   */
  query_ret_t appendEntries (term_t, id_t, index_t, term_t, index_t, query_t const&);

  /**
   * @brief 1. Deal with appendEntries to slaves. 2. Report success of write processes. 
   */
  void run ();

  private:
  Constituent _constituent; /**< @brief Leader election delegate */
  State       _state;       /**< @brief Log replica              */
  config_t    _config;
  status_t    _status;
  
  store<std::string> _spear_head;
  store<std::string> _read_db;

  arangodb::basics::ConditionVariable cv;
  
};

}

LoggerStream& operator<< (LoggerStream&, arangodb::consensus::Agent const&);
  
}



#endif
