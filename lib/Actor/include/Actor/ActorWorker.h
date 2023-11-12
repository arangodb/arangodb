////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Actor/ActorBase.h"

namespace arangodb::actor {

struct ActorWorker {
  // capture a weak_ptr to the actor: this way, the actor can be destroyed
  // although this worker is still waiting in the scheduler. When this worker is
  // executed in the queue after actor was destroyed, the weak_ptr will
  // be empty and work will just not be executed
  explicit ActorWorker(ActorBase* actor) : actor(actor->weak_from_this()) {}

  void operator()() {
    auto me = actor.lock();
    if (me != nullptr) {
      me->work();
    }
  }
  std::weak_ptr<ActorBase> actor;
};

}  // namespace arangodb::actor
