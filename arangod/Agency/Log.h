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

#ifndef __ARANGODB_CONSENSUS_LOG__
#define __ARANGODB_CONSENSUS_LOG__


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

class Agent;

/**
 * @brief Log replica
 */
  class Log : public arangodb::Thread, public arangodb::ClusterCommCallback,
              std::enable_shared_from_this<Log> {
  
public:
  typedef uint64_t index_t;
  typedef int32_t ret_t;

  /**
   * @brief Default constructor
   */
    Log ();

  /**
   * @brief Default Destructor
   */
  virtual ~Log();

  void configure(Agent* agent);

  /**
   * @brief Log
   */
  template<typename T> ret_t log (T const&);

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

private:

  /**
   * @brief         Write transaction
   * @param state   State demanded
   * @param expiry  Time of expiration
   * @param update  Update state
   */
   template<typename T> std::shared_ptr<T> readTransaction (
     T const& state, T const& update);
    
  /**
   * @brief         Write transaction
   * @param state   State demanded
   * @param expiry  Time of expiration
   * @param update  Update state
   */
   template<typename T> std::shared_ptr<T> writeTransaction (
     T const& state, duration_t expiry, T const& update);
    
  /**
   * @brief         Check transaction condition
   * @param state   State demanded
   * @param pre     Prerequisite
   */
  template<typename T> bool checkTransactionPrecondition (
    T const& state, T const& pre);
  
  index_t _commit_id;    /**< @brief  index of highest log entry known
                                      to be committed (initialized to 0, increases monotonically) */
  index_t _last_applied; /**< @brief  index of highest log entry applied to state machine  */
  State   _state;        /**< @brief  State machine */

  Agent*  _agent;        /**< @brief  My boss */
  
};
  
}}

#endif
