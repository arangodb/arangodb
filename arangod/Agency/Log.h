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

#ifndef __ARANGODB_CONSENSUS_STATE__
#define __ARANGODB_CONSENSUS_STATE__


#include "AgencyCommon.h"
#include "State.h"

#include <Basics/Thread.h>
#include <Cluster/ClusterComm.h>
#include <velocypack/vpack.h>

#include <cstdint>
#include <functional>


//using namespace arangodb::velocypack;

class Slice {};

namespace arangodb {
namespace consensus {

typedef uint64_t index_t;

/**
 * @brief State entry
 */
struct log_t {
  term_t      term;
  id_t        leaderId;
  index_t     index;
  std::string entry;
};

class Agent;

/**
 * @brief State replica
 */
class State : public arangodb::Thread, public arangodb::ClusterCommCallback,
            std::enable_shared_from_this<State> {
  
public:
  
  /**
   * @brief Default constructor
   */
  State ();
  
  /**
   * @brief Default Destructor
   */
  virtual ~State();
  
  void configure(Agent* agent);
  
  /**
   * @brief State
   */
  template<typename T> id_t log (T const&);
  
  /**
   * @brief Call back for log results from slaves
   */
  virtual bool operator()(ClusterCommResult*);
    
  /**
   * @brief My daily business
   */
  void run();

  /**
   * @brief 
   */
  void respHandler (index_t);

  /**
   * @brief Attempt write
   */
  query_ret_t write (query_t const&) ;

  /**
   * @brief Read from agency
   */
  query_ret_t read (query_t const&) const;

  /**
   * @brief Append entries
   */
  bool appendEntries (query_t const&); 
  

private:
  
  State   _state;        /**< @brief  State machine */
  State   _spear_head;   /**< @brief  Spear head */
  Agent*  _agent;        /**< @brief  My boss */
  log_t   _log;          /**< @brief  State entries */
  
  
};

}}

#endif
