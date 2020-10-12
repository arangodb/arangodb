////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_COMPACTOR_H
#define ARANGOD_CONSENSUS_COMPACTOR_H 1

#include "Agency/AgencyCommon.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

namespace arangodb {
namespace consensus {

// Forward declaration
class Agent;

class Compactor : public arangodb::Thread {
 public:
  // @brief Construct with agent pointer
  explicit Compactor(Agent* _agent);

  virtual ~Compactor();

  /// @brief 1. Deal with appendEntries to slaves.
  ///        2. Report success of write processes.
  void run() override final;

  /// @brief Start orderly shutdown of threads
  void beginShutdown() override final;

  /// @brief Wake up compaction
  void wakeUp();

  /// @brief Do compaction
  void compact();

 private:
  Agent* _agent;  //< @brief Agent
  // This condition variable is used for the compactor thread to go to
  // sleep. One has to set _wakeupCompactor to true under the Mutex of _cv
  // and then broadcast on the _cv to wake up the compactor thread.
  // Note that the Mutex is not held during the actual compaction!
  basics::ConditionVariable _cv;
  bool _wakeupCompactor;
  long _waitInterval;  //< @brief Wait interval
};

}  // namespace consensus
}  // namespace arangodb

#endif
