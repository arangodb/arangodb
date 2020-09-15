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

#ifndef ARANGOD_CONSENSUS_INCEPTION_H
#define ARANGOD_CONSENSUS_INCEPTION_H 1

#include <memory>

#include "Agency/AgencyCommon.h"
#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"
#include "Basics/Thread.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

class Agent;

/// @brief This class organizes the startup of the agency until the point
///        where the RAFT implementation can commence function
class Inception : public Thread {
 public:
  /// @brief Construct with agent
  explicit Inception(Agent&);

  /// @brief Defualt dtor
  virtual ~Inception();

  /// @brief Report acknowledged version for peer id
  void reportVersionForEp(std::string const&, size_t);

  void beginShutdown() override;
  void run() override;

  void signalConditionVar();

 private:
  /// @brief We are a restarting active RAFT agent
  ///
  /// Make sure that majority of agents agrees on pool and active list.
  /// Subsequently, start agency. The exception to agreement over active agent
  /// list among peers is if an agent joins with empty active list. This allows
  /// for a peer with empty list to join nevertheless.
  /// The formation of an agency is tried for an hour before giving up.
  bool restartingActiveAgent();

  /// @brief Gossip your way into the agency
  ///
  /// No persistence: gossip an agency together.
  void gossip();

  Agent& _agent;                            //< @brief The agent
  arangodb::basics::ConditionVariable _cv;  //< @brief For proper shutdown
  std::unordered_map<std::string, size_t> _acked;  //< @brief acknowledged config version
  mutable arangodb::Mutex _vLock;                  //< @brieg Guard _acked
};

}  // namespace consensus
}  // namespace arangodb

#endif
