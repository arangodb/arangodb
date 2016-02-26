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

class Agent;

/**
 * @brief State replica
 */
class State : public arangodb::ClusterCommCallback, // We need to provide callBack 
xb
              std::enable_shared_from_this<State> { // For making shared_ptr from this class
  
public:
  
  /**
   * @brief Default constructor
   */
  State ();
  
  /**
   * @brief Default Destructor
   */
  virtual ~State();
  
  /**
   * @brief Append log entry
   */
  void append (query_t const& query);

  /**
   * @brief Update log entry
   */
  
  /**
   * @brief Save currentTerm, votedFor, log entries
   */
  bool save (std::string const& ep = "tcp://localhost:8529");

  /**
   * @brief Load persisted data from above or start with empty log
   */
  bool load (std::string const& ep = "tcp://localhost:8529");
  

private:
  
  std::vector<log_t> _log;          /**< @brief  State entries */
  
};

}}

#endif
