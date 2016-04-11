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

#ifndef __ARANGODB_CONSENSUS_SANITY_CHECK__
#define __ARANGODB_CONSENSUS_SANITY_CHECK__

#include "Basics/Thread.h"
#include "Basics/ConditionVariable.h"

namespace arangodb {
namespace consensus {

class Agent;

class SanityCheck : public arangodb::Thread {
  
public:
  
  /// @brief Construct sanity checking
  SanityCheck ();
  
  /// @brief Default dtor
  ~SanityCheck ();
  
  /// @brief Configure with agent
  void configure (Agent* agent);
  
  /// @brief Run woker
  void run() override final;
  
  /// @brief Begin thread shutdown
  void beginShutdown() override final;

  /// @brief Wake up to task
  void wakeUp ();

  /// @brief Stop task and wait
  void passOut ();
  
private:
  
  Agent* _agent;

  arangodb::basics::ConditionVariable _cv; /**< @brief Control if thread should run */
  

  
};

}}

#endif //__ARANGODB_CONSENSUS_SANITY_CHECK__
