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

#ifndef ARANGOD_CONSENSUS_INCEPTION_H
#define ARANGOD_CONSENSUS_INCEPTION_H 1

#include <memory>

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace consensus {

class Agent;

class Inception : public Thread {

public:
  
  Inception();
  explicit Inception(Agent*);
  virtual ~Inception();
  
  void run() override;
  bool start();
  
  /// @brief Orderly shutdown of thread
  void beginShutdown() override;
  
private:

  bool _done;
  Agent* _agent;
  arangodb::basics::ConditionVariable _cv;
  
};

}}

#endif
